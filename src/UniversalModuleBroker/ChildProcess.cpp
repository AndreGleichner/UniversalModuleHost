#include "pch.h"
#include "ChildProcess.h"
#include "ipc.h"

namespace
{
void DumpPipeInfos(HANDLE pipe)
{
    // https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-getnamedpipeinfo
    DWORD flags = 0, outBufferSize = 0, inBufferSize = 0, maxInstances = 0;
    BOOL  res = ::GetNamedPipeInfo(pipe, &flags, &outBufferSize, &inBufferSize, &maxInstances);

    spdlog::info(L"flags:{} outSize:{} inSize:{} instances:{}", flags, outBufferSize, inBufferSize, maxInstances);
}
}
extern HANDLE g_inWrite;
HRESULT       ChildProcess::Create(PCWSTR commandline)
{
    // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output

    // Set the bInheritHandle flag so pipe handles are inherited.
    SECURITY_ATTRIBUTES saAttr {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    // Create pipes for the child process's STDOUT,STDERR,STDIN.
    RETURN_IF_WIN32_BOOL_FALSE(::CreatePipe(&outRead_, &outWrite_, &saAttr, 0));
    RETURN_IF_WIN32_BOOL_FALSE(::CreatePipe(&errRead_, &errWrite_, &saAttr, 0));
    RETURN_IF_WIN32_BOOL_FALSE(::CreatePipe(&inRead_, &inWrite_, &saAttr, 0));

    // DumpPipeInfos(outRead_.get());
    // DumpPipeInfos(errRead_.get());
    // DumpPipeInfos(inRead_.get());

    // DumpPipeInfos(outWrite_.get());
    // DumpPipeInfos(errWrite_.get());
    // DumpPipeInfos(inWrite_.get());

    // Ensure the read handle to the pipe for STDOUT,STDERR and write handle for STDIN are not inherited.
    // RETURN_IF_WIN32_BOOL_FALSE(::SetHandleInformation(outRead_.get(), HANDLE_FLAG_INHERIT, 0));
    // RETURN_IF_WIN32_BOOL_FALSE(::SetHandleInformation(errRead_.get(), HANDLE_FLAG_INHERIT, 0));
    // RETURN_IF_WIN32_BOOL_FALSE(::SetHandleInformation(inWrite_.get(), HANDLE_FLAG_INHERIT, 0));

    wil::unique_process_information processInfo;
    ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

#pragma region Init process thread attributes
    // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873

    const DWORD attrCount         = 3;
    SIZE_T      attributeListSize = 0;
    ::InitializeProcThreadAttributeList(nullptr, attrCount, 0, &attributeListSize);
    LPPROC_THREAD_ATTRIBUTE_LIST attrList =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(::HeapAlloc(::GetProcessHeap(), 0, attributeListSize));
    RETURN_IF_NULL_ALLOC(attrList);
    RETURN_IF_WIN32_BOOL_FALSE(::InitializeProcThreadAttributeList(attrList, attrCount, 0, &attributeListSize));
    auto cleanAttr = wil::scope_exit([&] {
        ::DeleteProcThreadAttributeList(attrList);
        ::HeapFree(GetProcessHeap(), 0, attrList);
    });

    // https://docs.microsoft.com/en-us/windows/win32/services/protecting-anti-malware-services-
    // If the running process is not a AM-PPL the PROTECTION_LEVEL_SAME and CREATE_PROTECTED_PROCESS wont harm.
    // Limitations:
    //  1. A protected process can't have a GUI
    //  2. HTTPS from a protected process only works since Win10-1703
    DWORD protectionLevel = PROTECTION_LEVEL_SAME;
    RETURN_IF_WIN32_BOOL_FALSE(::UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PROTECTION_LEVEL,
        &protectionLevel, sizeof(protectionLevel), nullptr, nullptr));

    // Ensure only these handles are inherited.
    // We may launch multiple child processes and w/o this call each would inherit all pipe handles created so far.
    HANDLE handlesToInherit[] = {inRead_.get(), outWrite_.get(), errWrite_.get()};
    RETURN_IF_WIN32_BOOL_FALSE(::UpdateProcThreadAttribute(
        attrList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &handlesToInherit, sizeof(handlesToInherit), nullptr, nullptr));

    // clang-format off
    DWORD64 policy[2] =
    {
        PROCESS_CREATION_MITIGATION_POLICY_DEP_ENABLE |
        PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE |
        PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_STRICT_HANDLE_CHECKS_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_ON,
        // 2nd
        PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY2_USER_CET_SET_CONTEXT_IP_VALIDATION_ALWAYS_ON
    };
    // clang-format on

    RETURN_IF_WIN32_BOOL_FALSE(::UpdateProcThreadAttribute(
        attrList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &policy, sizeof(policy), nullptr, nullptr));
#pragma endregion

    STARTUPINFOEX startInfo {0};
    startInfo.StartupInfo.cb         = sizeof(startInfo);
    startInfo.StartupInfo.hStdError  = errWrite_.get();
    startInfo.StartupInfo.hStdOutput = outWrite_.get();
    startInfo.StartupInfo.hStdInput  = inRead_.get();
    startInfo.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startInfo.lpAttributeList = attrList;

    // Create the child process.
    std::wstring cmdline(commandline);

    RETURN_IF_WIN32_BOOL_FALSE(::CreateProcessW(nullptr,         // applicationName
        const_cast<PWSTR>(cmdline.data()),                       // commandLine shall be non-const
        nullptr,                                                 // process security attributes
        nullptr,                                                 // primary thread security attributes
        TRUE,                                                    // handles are inherited
        EXTENDED_STARTUPINFO_PRESENT | CREATE_PROTECTED_PROCESS, // creation flags
        nullptr,                                                 // use parent's environment
        nullptr,                                                 // use parent's current directory
        (LPSTARTUPINFOW)&startInfo,                              // STARTUPINFO pointer
        &processInfo));                                          // receives PROCESS_INFORMATION

    // Close handles to the stdin and stdout pipes no longer needed by the parent process.
    // If they are not explicitly closed, there is no way to recognize that the child process has ended.

    outWrite_.reset();
    errWrite_.reset();
    inRead_.reset();

    ::SetStdHandle(STD_ERROR_HANDLE, errRead_.get());

    g_inWrite = inWrite_.get();

    ipc::Send("xxx", {ipc::KnownService::WebBrowser});

    return S_OK;
}
