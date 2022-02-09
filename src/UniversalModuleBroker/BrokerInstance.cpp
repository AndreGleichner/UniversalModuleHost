#include "pch.h"
#include <fstream>
#include "BrokerInstance.h"
#include "UmhProcess.h"
#include "string_extensions.h"
using namespace Strings;
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <wil/result.h>

BrokerInstance::BrokerInstance()
{
    FAIL_FAST_IF_WIN32_BOOL_FALSE(::ProcessIdToSessionId(::GetCurrentProcessId(), &session_));
}

// Process broker.json and launch host child processes as configured.
HRESULT BrokerInstance::Init() noexcept
try
{
    // while (!::IsDebuggerPresent())
    //{
    //    ::Sleep(1000);
    //}

    //::DebugBreak();

    AssignProcessToJobObject(::GetCurrentProcess(), session_);

    auto confFile   = Process::ImagePath().replace_filename(L"broker.json");
    auto modulesDir = Process::ImagePath().replace_filename(L"modules");

    std::ifstream confStream(confFile.c_str());
    json          conf;
    confStream >> conf;

    for (auto& p : conf["ChildProcesses"])
    {
        bool        allUsers             = p["Session"] == "AllUsers";
        bool        wow64                = (sizeof(void*) == 8) && p.contains("Wow64") && p["Wow64"];
        bool        higherIntegrityLevel = p.contains("IntegrityLevel") && p["IntegrityLevel"] == "Higher";
        bool        ui                   = p.contains("Ui") && p["Ui"];
        std::string groupName            = p["GroupName"];
        std::vector<std::wstring> modules;

        for (auto& m : p["Modules"])
        {
            modules.push_back(ToUtf16(m));
        }

        if (allUsers && Process::IsWindowsService())
        {
            // Walk all currently active sessions to create a child process in every.

            wil::unique_wtsmem_ptr<WTS_SESSION_INFOW> sessionInfo;
            DWORD                                     sessionCount = 0;

            RETURN_IF_WIN32_BOOL_FALSE(
                ::WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, wil::out_param(sessionInfo), &sessionCount));

            for (DWORD sess = 0; sess < sessionCount; ++sess)
            {
                auto si = &sessionInfo.get()[sess];
                if (si->State != WTSActive && si->State != WTSDisconnected)
                    continue;

                if (si->SessionId == 0)
                    continue;

                auto cp = std::make_unique<ChildProcess>(
                    this, allUsers, wow64, higherIntegrityLevel, ui, groupName, modules, si->SessionId);
                childProcesses_.push_back(std::move(cp));
            }
        }
        else
        {
            auto cp =
                std::make_unique<ChildProcess>(this, allUsers, wow64, higherIntegrityLevel, ui, groupName, modules);
            childProcesses_.push_back(std::move(cp));
        }
    }

    for (auto& process : childProcesses_)
    {
        process->Launch();
    }
    for (auto& process : childProcesses_)
    {
        process->LoadModules();
    }

    return S_OK;
}
CATCH_RETURN();

HRESULT BrokerInstance::Release() noexcept
try
{
    for (auto& process : childProcesses_)
    {
        process->Terminate();
    }
    return S_OK;
}
CATCH_RETURN();

HRESULT BrokerInstance::OnSessionChange(DWORD dwEventType, DWORD dwSessionId) noexcept
try
{
    return S_OK;
}
CATCH_RETURN();

void BrokerInstance::OnMessage(const ChildProcess* fromProcess, const std::string_view msg, const ipc::Target& target)
{
    if (target.Service == ipc::KnownService::Broker)
    {
        // TODO: add valuable actions the broker may perform.
        return;
    }

    for (auto& process : childProcesses_)
    {
        if (process.get() != fromProcess)
            process->SendMsg(msg, target);
    }
}

// If we're running as service and the to be launched process will run in another session
// we have to use another job object since processes grouped in a job shall all run in the same session.
bool BrokerInstance::ShouldBreakAwayFromJob(const ChildProcess* childProcess) const
{
    if (!Process::IsWindowsService())
        return false;

    DWORD session = ipc::KnownSession::Any;
    if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &session))
        return false;
    return session != childProcess->target_.Session;
}

void BrokerInstance::AssignProcessToJobObject(const ChildProcess* childProcess)
{
    if (!childProcess->processInfo_.hProcess)
        return;

    if (!Process::IsWindowsService())
        return;

    DWORD session = ipc::KnownSession::Any;
    if (!::ProcessIdToSessionId(childProcess->processInfo_.dwProcessId, &session))
        return;

    AssignProcessToJobObject(childProcess->processInfo_.hProcess, session);
}

void BrokerInstance::AssignProcessToJobObject(HANDLE process, DWORD session)
{
    // Already have a job object for this session?
    if (!jobObjects_.contains(session))
    {
#if 0 // seems not really required
      // Place the broker and all its child processes in a job, so that
      // if the broker dies the system also terminates any child processes.
      // Allow a child process to explicitly create child processes that don't belong to the job.
      // This may be usefull to create UI processes like default browsers which shouldn't terminate on broker exit.
      // https://stackoverflow.com/questions/41985061/open-a-jobobject-created-in-a-service-from-a-user-session-process
      // https://sourcegraph.com/github.com/sandboxie/sandboxie/-/blob/core/svc/GuiServer.cpp

        PSECURITY_DESCRIPTOR sd     = nullptr;
        auto                 freeSD = wil::scope_exit([&] {
            if (sd)
                ::LocalFree(sd);
        });

        if (Process::IsWindowsService())
        {
            // https://docs.microsoft.com/en-us/windows/win32/secauthz/security-descriptor-string-format
            // https://docs.microsoft.com/en-us/windows/win32/secauthz/sid-strings
            // https://docs.microsoft.com/en-us/windows/win32/secauthz/ace-strings

            // SID Strings:
            //      SY  SDDL_LOCAL_SYSTEM
            //      WD  SDDL_EVERYONE
            //
            // DACL/SACL ACE:
            //      Sample
            //                              D:(A;;GA;;;SY)              D:NO_ACCESS_CONTROLS:(ML;;;;;LW)
            //
            //      ace_type                A => SDDL_ACCESS_ALLOWED    ML => SDDL_MANDATORY_LABEL
            //      ace_flags               -
            //      rights                  GA => SDDL_GENERIC_ALL
            //      object_guid             -
            //      inherit_object_guid     -
            //      account_sid             SY => SDDL_LOCAL_SYSTEM     LW => SDDL_ML_LOW
            //      (resource_attribute)    not present
            //
            // O:   Owner
            // G:   Group
            //
            // D:   dacl_flags(string_ace1)(string_ace2)... (string_acen)
            // S:   sacl_flags(string_ace1)(string_ace2)... (string_acen)
            //      d/sacl_flags are optional
            //
            //
            // Create a SACL with low mandatory label
            FAIL_FAST_IF_WIN32_BOOL_FALSE(::ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:NO_ACCESS_CONTROLS:(ML;;;;;LW)", SDDL_REVISION_1, &sd, nullptr));
        }
        SECURITY_ATTRIBUTES sa = {sizeof(sa), sd, FALSE};
#endif
        wil::unique_handle job(::CreateJobObjectW(nullptr, nullptr));
        FAIL_FAST_IF_MSG(!job, "Failed to create job object for session %u", session);

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobOptions;
        ZeroMemory(&jobOptions, sizeof(jobOptions));
        jobOptions.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        FAIL_FAST_IF_WIN32_BOOL_FALSE(
            ::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &jobOptions, sizeof(jobOptions)));

        jobObjects_.emplace(session, std::move(job));
    }

    FAIL_FAST_IF_WIN32_BOOL_FALSE(::AssignProcessToJobObject(jobObjects_[session].get(), process));
}
