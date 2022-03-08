#include "pch.h"
#include "ChildProcessInstance.h"
#include "ipc.h"
#include "UmhProcess.h"
#include "HostMsg.h"
#include "Orchestrator.h"
#include "ChildProcessConfig.h"

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

HRESULT ChildProcessInstance::Launch(LaunchReason launchReason) noexcept
try
{
    if (orchestrator_->IsShuttingDown())
        return S_OK;

    // Cleanup, in case Launch() was run before
    if (launchReason == LaunchReason::Restart)
    {
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
    }
    else if (launchReason == LaunchReason::ApplyConfig)
    {
        // Allready running
        if (processInfo_.dwProcessId)
            return S_OK;
    }
    else
    {
        RETURN_HR(E_INVALIDARG);
    }

    PCWSTR name = childProcessConfig_->Wow64 ? L"UniversalModuleHost32.exe" : L"UniversalModuleHost64.exe";

    // Having GroupName on cmdline is just there so that we can easily see externally which stuff is running within a
    // child process.
    std::wstring cmdline = std::format(
        L"\"{}\" {}", Process::ImagePath().replace_filename(name).c_str(), ToUtf16(childProcessConfig_->GroupName));

    // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output

    // Set the bInheritHandle flag so pipe handles are inherited.
    SECURITY_ATTRIBUTES saAttr {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    // Create pipes for the child process's STDOUT,STDERR,STDIN.
    // https://stackoverflow.com/questions/60645/overlapped-i-o-on-anonymous-pipe
    // Buffer size defaults to 4096
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

    const bool canLauchProtectedChild = !childProcessConfig_->Ui && Process::IsProtectedService();

    const DWORD attrCount         = canLauchProtectedChild ? 3 : 2;
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

    if (canLauchProtectedChild)
    {
        // UI processes can't run protected
        //
        // https://docs.microsoft.com/en-us/windows/win32/services/protecting-anti-malware-services-
        // If the running process is not a AM-PPL the PROTECTION_LEVEL_SAME and CREATE_PROTECTED_PROCESS wont harm.
        // Limitations:
        //  1. A protected process can't have a GUI
        //  2. HTTPS from a protected process only works since Win10-1703
        DWORD protectionLevel = PROTECTION_LEVEL_SAME;
        RETURN_IF_WIN32_BOOL_FALSE(::UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PROTECTION_LEVEL,
            &protectionLevel, sizeof(protectionLevel), nullptr, nullptr));
    }

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
        // .Net 6.0.1 fails to load with CETCompat.
        // Background on CETCompat:
        // https://techcommunity.microsoft.com/t5/windows-kernel-internals-blog/developer-guidance-for-hardware-enforced-stack-protection/ba-p/2163340
        //PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_ALWAYS_ON |
        //PROCESS_CREATION_MITIGATION_POLICY2_USER_CET_SET_CONTEXT_IP_VALIDATION_ALWAYS_ON |
        PROCESS_CREATION_MITIGATION_POLICY2_LOADER_INTEGRITY_CONTINUITY_ALWAYS_ON
    };
    // clang-format on

    RETURN_IF_WIN32_BOOL_FALSE(::UpdateProcThreadAttribute(
        attrList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &policy, sizeof(policy), nullptr, nullptr));

#pragma endregion

    // Handler for messages from child process.
    // clang-format off
    auto onMessage = [&](const std::string_view msg, const ipc::Target& target)
    {
        if (spdlog::should_log(spdlog::level::trace))
        {
            std::string m = msg.data();
            std::erase_if(m, [](char c) { return c=='\r'||c=='\n'; });
            spdlog::trace("RX-B: {} for {}", m, Strings::ToUtf8(target.ToString()));
        }
                        
        return orchestrator_->OnMessage(this, msg, target) == S_FALSE;
    };
    // clang-format on

    STARTUPINFOEX startInfo {0};
    startInfo.StartupInfo.cb         = sizeof(startInfo);
    startInfo.StartupInfo.hStdError  = errWrite_.get();
    startInfo.StartupInfo.hStdOutput = outWrite_.get();
    startInfo.StartupInfo.hStdInput  = inRead_.get();
    startInfo.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startInfo.lpAttributeList = attrList;

    DWORD creationFlags = NORMAL_PRIORITY_CLASS | CREATE_DEFAULT_ERROR_MODE | EXTENDED_STARTUPINFO_PRESENT |
                          CREATE_NO_WINDOW | (canLauchProtectedChild ? CREATE_PROTECTED_PROCESS : 0);
    if (ShouldBreakAwayFromJob())
    {
        creationFlags |= CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED;
    }

    const auto imageDir = Process::ImagePath().parent_path();

    if (target_.Session == ipc::KnownSession::Any)
    {
        // This either means we were requested to launch a process in the same session,
        // or requested to lauch a process in every session but we're not a service (e.g. during debugging).
        // In either case launch in same session.

        RETURN_IF_WIN32_BOOL_FALSE(::CreateProcessW(nullptr, // applicationName
            const_cast<PWSTR>(cmdline.data()),               // commandLine shall be non-const
            nullptr,                                         // process security attributes
            nullptr,                                         // primary thread security attributes
            TRUE,                                            // handles are inherited
            creationFlags,                                   // creation flags
            nullptr,                                         // use parent's environment
            imageDir.c_str(),                                // use image directory
            (LPSTARTUPINFOW)&startInfo,                      // STARTUPINFO pointer
            &processInfo_));                                 // receives PROCESS_INFORMATION
    }
    else
    {
        // We only get here if we're running as a service and were requested to launch
        // a child process in every session.

        wil::unique_handle token;
        // Ignor any error as the respective session may just have closed
        if (::WTSQueryUserToken(target_.Session, &token))
        {
            wil::unique_handle dupToken;
            ::DuplicateTokenEx(token.get(), MAXIMUM_ALLOWED, NULL, SecurityIdentification, TokenPrimary, &dupToken);

            if (childProcessConfig_->HigherIntegrityLevel)
            {
                // Slightly increase the current integrity level to e.g. "Medium-Plus".
                // Hint: the default Administrator account runs at "High" integrity level.

                // https://zeltser.com/windows-integrity-levels-for-spyware-protection-processe/
                // https://docs.microsoft.com/en-US/windows/security/identity-protection/access-control/security-identifiers

                SID                      integritySid;
                SID_IDENTIFIER_AUTHORITY sia = {SECURITY_MANDATORY_LABEL_AUTHORITY};
                ::InitializeSid(&integritySid, &sia, 1);

                // Integrity Level SIDs are in the form of S-1-16-0xXXXX. (e.g.
                // S-1-16-0x1000 stands for low integrity level SID). There is one
                // and only one subauthority.
                DWORD* sidSubAuth = ::GetSidSubAuthority(&integritySid, 0);

                // Read the integrity level from the users token and increase it slightly
                DWORD len = 0;
                (void)::GetTokenInformation(dupToken.get(), TokenIntegrityLevel, NULL, 0, &len);
                RETURN_HR_IF(HRESULT_FROM_WIN32(::GetLastError()), ::GetLastError() != ERROR_INSUFFICIENT_BUFFER);

                TOKEN_MANDATORY_LABEL* ptml = (TOKEN_MANDATORY_LABEL*)malloc(len);
                RETURN_IF_WIN32_BOOL_FALSE(::GetTokenInformation(dupToken.get(), TokenIntegrityLevel, ptml, len, &len));
                *sidSubAuth = *::GetSidSubAuthority(ptml->Label.Sid, 0) + 0x100;
                free(ptml);

                TOKEN_MANDATORY_LABEL tml = {0};
                tml.Label.Attributes      = SE_GROUP_INTEGRITY;
                tml.Label.Sid             = &integritySid;

                RETURN_IF_WIN32_BOOL_FALSE(::SetTokenInformation(dupToken.get(), TokenIntegrityLevel, &tml,
                    sizeof(TOKEN_MANDATORY_LABEL) + ::GetLengthSid(&integritySid)));
            }

            wil::unique_environment_block env;
            RETURN_IF_WIN32_BOOL_FALSE(::CreateEnvironmentBlock(&env, dupToken.get(), FALSE));
            BOOL impersonated = ::ImpersonateLoggedOnUser(dupToken.get());
            auto revert       = wil::scope_exit([&] {
                if (impersonated)
                    ::RevertToSelf();
            });

            creationFlags |= CREATE_UNICODE_ENVIRONMENT;

            RETURN_IF_WIN32_BOOL_FALSE(::CreateProcessAsUserW(dupToken.get(), // user token
                nullptr,                                                      // applicationName
                const_cast<PWSTR>(cmdline.data()),                            // commandLine shall be non-const
                nullptr,                                                      // process security attributes
                nullptr,                                                      // primary thread security attributes
                TRUE,                                                         // handles are inherited
                creationFlags,                                                // creation flags
                env.get(),                                                    // use users environment
                imageDir.c_str(),                                             // use image directory
                (LPSTARTUPINFOW)&startInfo,                                   // STARTUPINFO pointer
                &processInfo_));
        }
    }
    // Close handles to the stdin and stdout pipes no longer needed by the parent process.
    // If they are not explicitly closed, there is no way to recognize that the child process has ended.

    outWrite_.reset();
    errWrite_.reset();
    inRead_.reset();

    if (WI_IsFlagSet(creationFlags, CREATE_BREAKAWAY_FROM_JOB))
    {
        orchestrator_->AssignProcessToJobObject(this);
    }

    if (WI_IsFlagSet(creationFlags, CREATE_SUSPENDED))
    {
        ::ResumeThread(processInfo_.hThread);
    }

    // If the host process writes to stdout it is a message to some service/session.
    ipc::StartRead(outRead_.get(), reader_, onMessage, processInfo_.dwProcessId);

    // If the host process writes to stderr it is logging output.
    // This will be forwarded to a specific spdlog logger.
    StartForwardStderr();

    // If the host process terminates unexpectedly we try to re-launch it.
    keepAlive_ = std::jthread([this](std::stop_token stoken) {
        Process::SetThreadName(std::format(L"UMB-KeepAlive-{}", processInfo_.dwProcessId).c_str());
        if (WAIT_OBJECT_0 == ::WaitForSingleObject(processInfo_.hProcess, INFINITE) &&
            !orchestrator_->IsShuttingDown() && !stoken.stop_requested())
        {
#ifdef DEBUG
            // In case we've a console attached and just closed it, we'll get terminated soon
            // by the default console control handler. Wait some time to be sure we're
            // not in the process of shutting down.
            // This only helps when running e.g. 1 child process as it takes some time for the child
            // to process the Ctrl-C. As long as not every child has completed processing the broker wont be
            // terminated.

            ::Sleep(1000);
#endif
            // run in another thread so that keepAlive_ can be joined
            auto launcher = std::thread([this] {
                Process::SetThreadName(std::format(L"UMB-KeepAliveRelaunch-{}", processInfo_.dwProcessId).c_str());
                Launch(LaunchReason::Restart);
                LoadModules();
            });
            launcher.detach();
        }
    });

    // Tell the host his Service GUID. This is used to talk to the host as such to e.g. load modules.
    // Modules hosted within the host process have their own one or multiple service GUIDs.
    json msg = ipc::HostInitMsg {target_.Service, childProcessConfig_->GroupName};
    RETURN_IF_FAILED(SendMsg(msg.dump(), ipc::Target(ipc::KnownService::HostInit)));

    return S_OK;
}
CATCH_RETURN();

HRESULT ChildProcessInstance::Terminate() noexcept
try
{
    // Ensure a stopped proc wont trigger a relaunch.
    keepAlive_.request_stop();
    // Let WaitForSingleObject return
    // processInfo_.reset();

    stderrForwarder_.request_stop();
    // errRead_.reset();

    reader_.request_stop();
    reader_.detach();
    // outRead_.reset();

    json msg = ipc::HostCmdMsg {ipc::HostCmdMsg::Cmd::Terminate, ""};

    RETURN_IF_FAILED(ipc::Send(inWrite_.get(), msg.dump(), target_));
    // This ensures the stdin read loop within the child proc exits.
    inWrite_.reset();

    return S_OK;
}
CATCH_RETURN();

HRESULT ChildProcessInstance::LoadModules() noexcept
try
{
    for (auto& mod : childProcessConfig_->Modules)
    {
        if (orchestrator_->IsShuttingDown())
            return S_OK;

        json args = ipc::HostCtrlModuleArgs {ipc::HostCtrlModuleArgs::Cmd::Load, ToUtf8(mod)};
        json msg  = ipc::HostCmdMsg {ipc::HostCmdMsg::Cmd::CtrlModule, args.dump()};

        RETURN_IF_FAILED(ipc::Send(inWrite_.get(), msg.dump(), target_));
    }
    return S_OK;
}
CATCH_RETURN();

HRESULT ChildProcessInstance::UnloadModules() noexcept
try
{
    return S_OK;
}
CATCH_RETURN();

extern std::shared_ptr<spdlog::logger> g_loggerStdErr;

namespace
{
// Logs from host process via stderr are prefixed with e.g. [INF], [ERR]...
// Convert this back to a spdlog level, so we can use the broker configured logger
// for further processing.
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

void ChildProcessInstance::StartForwardStderr() noexcept
{
    // Host process is writing UTF8.
    // This may be multiple messages separated by \r\n
    // => split and process one-by-one.
    // Messages maybe aren't read by a single ReadFile(), e.g. if writing into stderr is faster than reading here.
    // Thus we need to find line endings (\r\n) and accumulate until then.
    stderrForwarder_ = std::jthread([&](std::stop_token stoken) {
        Process::SetThreadName(std::format(L"UMB-ForwardStderr-{}", processInfo_.dwProcessId).c_str());
        const size_t bufSize = 4096;

        std::string msg;
        char        buf[bufSize] = {};
        DWORD       read         = 0;
        while (
            !stoken.stop_requested() && ::ReadFile(errRead_.get(), buf, bufSize - 1, &read, nullptr) && read < bufSize)
        {
            buf[read] = 0; // so we can do str ops

            char* pos = buf;
            while (*pos)
            {
                char* end = strchr(pos, '\r');
                if (!end)
                {
                    msg.append(pos);
                    break;
                }

                *end++ = 0;
                if (*end == '\n')
                    *end++ = 0;

                msg.append(pos);

                auto level = LevelFromMsg(msg.c_str());
                if (level != spdlog::level::off)
                {
                    // skip leading "[INF] " etc
                    g_loggerStdErr->log(level, msg.c_str() + 6);
                    g_loggerStdErr->flush();
                }
                msg.clear();
                pos = end;
            }
        }
    });
}

HRESULT ChildProcessInstance::SendMsg(const std::string_view msg, const ipc::Target& target)
{
    if (target.Session != ipc::KnownSession::Any)
    {
        // Only send to a single session
        DWORD session = ipc::KnownSession::Any;
        if (!::ProcessIdToSessionId(processInfo_.dwProcessId, &session) || session != target.Session)
            return S_FALSE;
    }
    RETURN_IF_FAILED(ipc::Send(inWrite_.get(), msg, target));

    return S_OK;
}

// If we're running as service and the to be launched process will run in another session
// we have to use another job object since processes grouped in a job shall all run in the same session.
bool ChildProcessInstance::ShouldBreakAwayFromJob() const
{
    if (!Process::IsWindowsService())
        return false;

    DWORD session = ipc::KnownSession::Any;
    if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &session))
        return false;
    return session != target_.Session;
}

bool ChildProcessInstance::operator==(const ChildProcessInstance& rhs) const
{
    if (childProcessConfig_->AllUsers != rhs.childProcessConfig_->AllUsers ||
        childProcessConfig_->Wow64 != rhs.childProcessConfig_->Wow64 ||
        childProcessConfig_->HigherIntegrityLevel != rhs.childProcessConfig_->HigherIntegrityLevel ||
        childProcessConfig_->Ui != rhs.childProcessConfig_->Ui ||
        childProcessConfig_->GroupName != rhs.childProcessConfig_->GroupName ||
        childProcessConfig_->AllUsers != rhs.childProcessConfig_->AllUsers)
        return false;

    if (target_.Session != rhs.target_.Session)
        return false;

    if (childProcessConfig_->Modules.size() != rhs.childProcessConfig_->Modules.size())
        return false;

    for (size_t n = 0; n < childProcessConfig_->Modules.size(); ++n)
    {
        // Even reordered modules is treated as diff
        if (childProcessConfig_->Modules[n] != rhs.childProcessConfig_->Modules[n])
            return false;
    }
    return true;
}
