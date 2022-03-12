#include "pch.h"
#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <wil/result.h>

#include "Orchestrator.h"
#include "ChildProcessConfig.h"
#include "ChildProcessInstance.h"
#include "TMProcess.h"
#include "string_extensions.h"
using namespace Strings;

#include "ModuleMeta.h"
#include "ConfStore.h"

Orchestrator::Orchestrator()
{
    FAIL_FAST_IF_WIN32_BOOL_FALSE(::ProcessIdToSessionId(::GetCurrentProcessId(), &session_));
}

// Process broker.json and launch host child processes as configured.
HRESULT Orchestrator::Init() noexcept
try
{
    // while (!::IsDebuggerPresent())
    //{
    //    ::Sleep(1000);
    //}

    //::DebugBreak();

    AssignProcessToJobObject(::GetCurrentProcess(), session_);

    // Bootstrap-config by launching the ConfStore module in some process.
    auto conf = R"(
{
  "Broker": {
    "ChildProcesses": [
      {
        "GroupName": "ConfStore",
        "Session": 0,
        "Modules": [
          "ConfStore"
        ]
      }
    ]
  }
}
)"_json;

    RETURN_IF_FAILED(UpdateChildProcessConfig(conf));
    RETURN_IF_FAILED(LaunchChildProcesses());

#ifdef DEBUG
    const DWORD milliSecondsToWait = INFINITE;
#else
    const DWORD milliSecondsToWait = 60 * 1000;
#endif
    // Need to wait for ModuleMeta sent by the ConfStore module.
    // Now we know the ConfStore service is ready and the broker was notified about the supported services.
    confStoreReady_.wait(milliSecondsToWait);

    // Ask the just launched ConfStore to send around the Broker config
    // so we can launch the current set of configured processes/modules.
    json msg = ipc::ConfStore {ipc::ConfStore::Cmd::Query, "Broker"};
    RETURN_IF_FAILED(SendToAllChildren(msg.dump().c_str(), ipc::Target(ipc::KnownService::ConfStore)));

    return S_OK;
}
CATCH_RETURN();

// Process given JSON it should have a "Broker" object.
HRESULT Orchestrator::UpdateChildProcessConfig(const json& conf) noexcept
try
{
    childProcessesConfigs_.clear();

    for (auto& p : conf["Broker"]["ChildProcesses"])
    {
        bool        allUsers             = p["Session"] == -1;
        bool        wow64                = (sizeof(void*) == 8) && p.contains("Wow64") && p["Wow64"];
        bool        higherIntegrityLevel = p.contains("IntegrityLevel") && p["IntegrityLevel"] == "Higher";
        bool        ui                   = p.contains("Ui") && p["Ui"];
        std::string groupName            = p["GroupName"];
        std::vector<std::wstring> modules;

        for (auto& m : p["Modules"])
        {
            modules.push_back(ToUtf16(m));
        }
        auto cp = std::make_shared<ChildProcessConfig>(allUsers, wow64, higherIntegrityLevel, ui, groupName, modules);
        childProcessesConfigs_.push_back(cp);
    }
    return S_OK;
}
CATCH_RETURN();

// Based on current config check which actual processes are desired.
// Depending on logged in users this may differ from run to run as there may be procs configured to run in all user
// sessions.
// If there are already porcesses running superflous procs are terminated and missing are started.
HRESULT Orchestrator::LaunchChildProcesses() noexcept
try
{
    // Collect the desired collection of child processes.
    // There shall be no duplicate configs.
    std::vector<std::unique_ptr<ChildProcessInstance>> desiredChildProcesses;

    for (auto process : childProcessesConfigs_)
    {
        if (process->AllUsers && Process::IsWindowsService())
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

                auto cp = std::make_unique<ChildProcessInstance>(this, process, si->SessionId);
                desiredChildProcesses.push_back(std::move(cp));
            }
        }
        else
        {
            auto cp = std::make_unique<ChildProcessInstance>(this, process);
            desiredChildProcesses.push_back(std::move(cp));
        }
    }

    std::vector<std::unique_ptr<ChildProcessInstance>> processesToTerminate;
    // Check whether running processes match desired set of processes.
    // Terminate any non-desired.
    for (auto pi = childProcesses_.begin(); pi != childProcesses_.end();)
    {
        auto p = pi->get();

        bool stillDesired = false;
        for (auto dpi = desiredChildProcesses.cbegin(); dpi != desiredChildProcesses.cend(); ++dpi)
        {
            auto dp = dpi->get();
            if (*p == *dp)
            {
                stillDesired = true;
                (void)desiredChildProcesses.erase(dpi);
                break;
            }
        }

        if (stillDesired)
        {
            ++pi;
        }
        else
        {
            // No longer desired, so terminate (deferred in a thread below) and remove.
            processesToTerminate.emplace_back(std::move(*pi));
            pi = childProcesses_.erase(pi);
        }
    }

    if (!processesToTerminate.empty())
    {
        std::jthread terminator([&] {
            for (auto& process : processesToTerminate)
            {
                process.get()->Terminate();
            }
        });
    }

    for (auto& newProcess : desiredChildProcesses)
    {
        childProcesses_.emplace_back(std::move(newProcess));
    }

    for (auto& process : childProcesses_)
    {
        process->Launch(ChildProcessInstance::LaunchReason::ApplyConfig);
    }
    for (auto& process : childProcesses_)
    {
        process->LoadModules();
    }
    return S_OK;
}
CATCH_RETURN();

HRESULT Orchestrator::Release() noexcept
try
{
    for (auto& process : childProcesses_)
    {
        process->Terminate();
    }
    return S_OK;
}
CATCH_RETURN();

HRESULT Orchestrator::OnSessionChange(DWORD dwEventType, DWORD dwSessionId) noexcept
try
{
    RETURN_IF_FAILED(LaunchChildProcesses());

    return S_OK;
}
CATCH_RETURN();

HRESULT Orchestrator::SendToAllChildren(const std::string_view msg, const ipc::Target& target) noexcept
try
{
    // Dispatch to any process which may have a respective handler.
    for (auto& process : childProcesses_)
    {
        // KnownService::All means a module has declared it wants to handle messages to any service, e.g. for
        // debugging.
        if (process->services_.contains(ipc::KnownService::All) || process->services_.contains(target.Service))
        {
            process->SendMsg(msg, target);
        }
    }
    return S_OK;
}
CATCH_RETURN();

HRESULT Orchestrator::OnMessage(
    ChildProcessInstance* fromProcess, const std::string_view msg, const ipc::Target& target) noexcept
try
{
    if (IsShuttingDown())
        return S_FALSE;

    if (target.Service == ipc::KnownService::Broker)
    {
    }
    else if (target.Service == ipc::KnownService::ModuleMetaConsumer)
    {
        // Some module tells us which services it supports.
        const auto mm = json::parse(msg).get<ipc::ModuleMeta>();

        for (const auto& s : mm.Services)
        {
            fromProcess->services_.emplace(Guid(s));
        }

        if (mm.Services.contains(ipc::KnownService::ConfStore.ToUtf8()))
            confStoreReady_.SetEvent();
    }
    else
    {
        if (target.Service == ipc::KnownService::ConfConsumer)
        {
            // Some config changed.
            // In case of the Broker config we need to recalc desired child processes.
            const json conf = json::parse(msg);
            if (conf.contains("Broker"))
            {
                RETURN_IF_FAILED(UpdateChildProcessConfig(conf));
                RETURN_IF_FAILED(LaunchChildProcesses());
            }
        }

        // Dispatch to the world.
        RETURN_IF_FAILED(SendToAllChildren(msg, target));
    }
    return S_OK;
}
CATCH_RETURN()

void Orchestrator::AssignProcessToJobObject(const ChildProcessInstance* childProcess)
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

void Orchestrator::AssignProcessToJobObject(HANDLE process, DWORD session)
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
