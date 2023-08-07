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

Orchestrator::Orchestrator() : so_(this), srb_(this), sm_ {this, so_, srb_, logger_}
{
    FAIL_FAST_IF_WIN32_BOOL_FALSE(::ProcessIdToSessionId(::GetCurrentProcessId(), &session_));
}

HRESULT Orchestrator::Init() noexcept
try
{
    // env::WaitForDebugger();

    process_event(sme::Initialize {});

    return S_OK;
}
CATCH_RETURN();

// Process brokerConfig_ it should have a "Broker" object.
void Orchestrator::UpdateChildProcessConfig()
{
    childProcessesConfigs_.clear();

    for (auto& p : brokerConfig_["Broker"]["ChildProcesses"])
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
}


void Orchestrator::CalcDesiredChildProcesses()
{
    // Calculate the desired collection of child processes.
    // There shall be no duplicate configs.
    desiredChildProcesses_.clear();

    for (auto process : childProcessesConfigs_)
    {
        if (process->AllUsers && Process::IsWindowsService())
        {
            // Walk all currently active sessions to create a child process in every.

            wil::unique_wtsmem_ptr<WTS_SESSION_INFOW> sessionInfo;
            DWORD                                     sessionCount = 0;

            THROW_IF_WIN32_BOOL_FALSE(
                ::WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, wil::out_param(sessionInfo), &sessionCount));

            for (DWORD sess = 0; sess < sessionCount; ++sess)
            {
                auto si = &sessionInfo.get()[sess];
                if (si->State != WTSActive && si->State != WTSDisconnected)
                    continue;

                if (si->SessionId == 0)
                    continue;

                auto cp = std::make_unique<ChildProcessInstance>(this, process, si->SessionId);
                desiredChildProcesses_.push_back(std::move(cp));
            }
        }
        else
        {
            auto cp = std::make_unique<ChildProcessInstance>(this, process);
            desiredChildProcesses_.push_back(std::move(cp));
        }
    }
}


// Based on current config check which actual processes are desired.
// Depending on logged in users this may differ from run to run as there may be procs configured to run in all user
// sessions.
// If there are already porcesses running superflous procs are terminated and missing are started.
HRESULT Orchestrator::LaunchChildProcesses() noexcept
try
{
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
        // TODO this will load modules again leading to duplicate Initialize calls, which may start threads, etc
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

HRESULT Orchestrator::SendToAllChildren(const ipc::MsgItem& msgItem) noexcept
try
{
    // Dispatch to any process which may have a respective handler.
    for (auto& process : childProcesses_)
    {
        // AllTopics means a module has declared it wants to subscripe to all topics, e.g. for debugging.
        if (process->topicIds_.contains(ipc::AllTopics) || process->topicIds_.contains(msgItem.Topic.TopicId))
        {
            process->Publish(msgItem.Msg, msgItem.Topic);
        }
    }
    return S_OK;
}
CATCH_RETURN();

HRESULT Orchestrator::OnMessage(ChildProcessInstance* fromProcess, const ipc::MsgItem& msgItem) noexcept
try
{
    if (IsShuttingDown())
        return S_FALSE;

    curMsg_ = msgItem;

    if (msgItem.Topic.TopicId == ipc::BrokerTopic)
    {
    }
    else if (msgItem.Topic.TopicId == ipc::ModuleMetaTopic)
    {
        // Some module tells us which topics it wants to subscribe to.
        const auto mm = json::parse(msgItem.Msg).get<ipc::ModuleMeta>();

        for (const auto& s : mm.TopicIds)
        {
            fromProcess->topicIds_.emplace(Guid(s));
        }

        // if (mm.TopicIds.contains(ipc::ConfStoreTopic.ToUtf8()))
        //  confStoreReady_.SetEvent();
    }
    else
    {
        if (msgItem.Topic.TopicId == ipc::ConfTopic)
        {
            // Some config changed.
            // In case of the Broker config we need to recalc desired child processes.
            const json conf = json::parse(msgItem.Msg);
            if (conf.contains("Broker"))
            {
                RETURN_IF_FAILED(UpdateChildProcessConfig(conf));
                RETURN_IF_FAILED(LaunchChildProcesses());
            }
        }

        // Dispatch to the world.
        RETURN_IF_FAILED(SendToAllChildren(msgItem));
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
