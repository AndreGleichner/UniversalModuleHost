#include "pch.h"
#include "ChildProcess.h"
#include "ipc.h"
#include "UmhProcess.h"
#include "HostMsg.h"

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

HRESULT ChildProcess::Launch(bool keepAlive /*= true*/) noexcept
try
{
    // Cleanup if Launch() was run before
    outRead_.reset();
    errRead_.reset();
    inWrite_.reset();
    processInfo_.reset();
    if (stderrForwarder_.joinable())
        stderrForwarder_.join();
    if (reader_.joinable())
        reader_.join();
    if (keepAlive_.joinable())
        keepAlive_.join();

    PCWSTR name = wow64_ ? L"UniversalModuleHost32.exe" : L"UniversalModuleHost64.exe";

    std::wstring cmdline = Process::ImagePath().replace_filename(name).c_str();

    // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output

    // Set the bInheritHandle flag so pipe handles are inherited.
    SECURITY_ATTRIBUTES saAttr {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    // Create pipes for the child process's STDOUT,STDERR,STDIN.
    // https://stackoverflow.com/questions/60645/overlapped-i-o-on-anonymous-pipe
    RETURN_IF_WIN32_BOOL_FALSE(::CreatePipe(&outRead_, &outWrite_, &saAttr, 0));
    RETURN_IF_WIN32_BOOL_FALSE(::CreatePipe(&errRead_, &errWrite_, &saAttr, 0));
    RETURN_IF_WIN32_BOOL_FALSE(::CreatePipe(&inRead_, &inWrite_, &saAttr, 0));

    // DumpPipeInfos(outRead_.get());
    // DumpPipeInfos(errRead_.get());
    // DumpPipeInfos(inRead_.get());

    // DumpPipeInfos(outWrite_.get());
    // DumpPipeInfos(errWrite_.get());
    // DumpPipeInfos(inWrite_.get());

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
        PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON,
        // 2nd
        PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY2_USER_CET_SET_CONTEXT_IP_VALIDATION_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY2_LOADER_INTEGRITY_CONTINUITY_ALWAYS_ON
    };
    // clang-format on

    RETURN_IF_WIN32_BOOL_FALSE(::UpdateProcThreadAttribute(
        attrList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &policy, sizeof(policy), nullptr, nullptr));

#pragma endregion

    StartForwardStderr();

    // clang-format off
    auto onMessage = [&](const std::string_view msg, const ipc::Target& target)
    {
        spdlog::info("RX-B: {} for {}", msg, Strings::ToUtf8(target.ToString()));
        OnMessage(msg, target);
    };
    // clang-format on
    ipc::StartRead(outRead_.get(), reader_, onMessage);

    STARTUPINFOEX startInfo {0};
    startInfo.StartupInfo.cb         = sizeof(startInfo);
    startInfo.StartupInfo.hStdError  = errWrite_.get();
    startInfo.StartupInfo.hStdOutput = outWrite_.get();
    startInfo.StartupInfo.hStdInput  = inRead_.get();
    startInfo.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startInfo.lpAttributeList = attrList;

    RETURN_IF_WIN32_BOOL_FALSE(::CreateProcessW(nullptr,         // applicationName
        const_cast<PWSTR>(cmdline.data()),                       // commandLine shall be non-const
        nullptr,                                                 // process security attributes
        nullptr,                                                 // primary thread security attributes
        TRUE,                                                    // handles are inherited
        EXTENDED_STARTUPINFO_PRESENT | CREATE_PROTECTED_PROCESS, // creation flags
        nullptr,                                                 // use parent's environment
        nullptr,                                                 // use parent's current directory
        (LPSTARTUPINFOW)&startInfo,                              // STARTUPINFO pointer
        &processInfo_));                                         // receives PROCESS_INFORMATION

    // Close handles to the stdin and stdout pipes no longer needed by the parent process.
    // If they are not explicitly closed, there is no way to recognize that the child process has ended.

    outWrite_.reset();
    errWrite_.reset();
    inRead_.reset();

    if (keepAlive)
    {
        keepAlive_ = std::thread([this] {
            if (WAIT_OBJECT_0 == ::WaitForSingleObject(processInfo_.hProcess, INFINITE))
            {
                // In case we've a console attached and just closed it, we'll get terminated soon
                // by the default console control handler. Wait some time to be sure we're
                // not in the process of shutting down.
                // TODO: How does this behave in case of running as a service? May need some global shutdown flag.
                ::Sleep(1000);

                // run in another thread so that keepAlive_ can be joined
                auto launcher = std::thread([this] { Launch(); });
                launcher.detach();
            }
        });
    }

    // Tell the host his new Service GUID
    nlohmann::json msg = ipc::HostInitMsg {target_.Service, groupName_};
    RETURN_IF_FAILED(ipc::Send(inWrite_.get(), msg.dump(), ipc::Target(target_.Service, ipc::KnownSession::HostInit)));

    return S_OK;
}
CATCH_RETURN();

HRESULT ChildProcess::Terminate() noexcept
try
{
    nlohmann::json msg = ipc::HostCmdMsg {ipc::HostCmdMsg::Cmd::Terminate, ""};

    RETURN_IF_FAILED(ipc::Send(inWrite_.get(), msg.dump(), target_));
    return S_OK;
}
CATCH_RETURN();

HRESULT ChildProcess::LoadModules() noexcept
try
{
    for (auto& mod : modules_)
    {
        nlohmann::json args = ipc::HostCtrlModuleArgs {ipc::HostCtrlModuleArgs::Cmd::Load, ToUtf8(mod)};
        nlohmann::json msg  = ipc::HostCmdMsg {ipc::HostCmdMsg::Cmd::CtrlModule, args.dump()};

        RETURN_IF_FAILED(ipc::Send(inWrite_.get(), msg.dump(), target_));
    }
    return S_OK;
}
CATCH_RETURN();

HRESULT ChildProcess::UnloadModules() noexcept
try
{
    return S_OK;
}
CATCH_RETURN();

extern std::shared_ptr<spdlog::logger> g_loggerStdErr;

namespace
{
spdlog::level::level_enum LevelFromMsg(PCSTR msg)
{
    static INIT_ONCE                                                    g_init {};
    static std::array<std::string, spdlog::level::level_enum::n_levels> g_msgPrefixes;

    wil::init_once(g_init, [] {
        PCSTR prefixes[] = SPDLOG_LEVEL_NAMES;
        for (int i = 0; i < _countof(prefixes); ++i)
        {
            g_msgPrefixes[i] = std::format("[{}] ", prefixes[i]);
        }
    });

    for (size_t i = 0; i < g_msgPrefixes.size(); ++i)
    {
        if (strstr(msg, g_msgPrefixes[i].c_str()) == msg)
            return static_cast<spdlog::level::level_enum>(i);
    }
    return spdlog::level::off;
}
}

void ChildProcess::StartForwardStderr() noexcept
{
    stderrForwarder_ = std::thread([&] {
        char  buf[4096] = {};
        DWORD read      = 0;
        while (::ReadFile(errRead_.get(), buf, sizeof(buf), &read, NULL))
        {
            if (read < sizeof(buf))
            {
                buf[read] = 0;
                // This may be multiple messages separated by \r\n
                // => split and process one-by-one.
                char* pos = buf;
                while (*pos)
                {
                    char* end = strchr(pos, '\r');
                    if (!end)
                        break;

                    const char* str = pos;

                    *end++ = 0;
                    if (*end == '\n')
                        *end++ = 0;

                    auto level = LevelFromMsg(str);
                    if (level != spdlog::level::off)
                    {
                        // skip leading "[INF] " etc
                        g_loggerStdErr->log(level, str + 6);
                        g_loggerStdErr->flush();
                    }

                    pos = end;
                }
            }
        }
    });
}
