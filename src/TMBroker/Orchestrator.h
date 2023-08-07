#pragma once
#include <Windows.h>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <boost/sml.hpp>
namespace sml = boost::sml;

#include "guid.h"
#include "ipc.h"

#include "ChildProcessInstance.h"
#include "SmOrchestrator.h"
#include "statemachine_logger.h"

struct ChildProcessConfig;

class Orchestrator
{
    friend ChildProcessInstance;
    friend SmOrchestrator;
    friend SmReconfigureBroker;

public:
    Orchestrator();

    template <typename TEvent>
    void process_event(const TEvent& event)
    {
        sm_.process_event(event);
    }

    HRESULT Init() noexcept;
    HRESULT Release() noexcept;
    HRESULT OnSessionChange(DWORD dwEventType, DWORD dwSessionId) noexcept;


    void ShuttingDown()
    {
        shuttingDown_ = true;
    }

    bool IsShuttingDown() const
    {
        return shuttingDown_;
    }

    void AssignProcessToJobObject(const ChildProcessInstance* childProcess);
    void AssignProcessToJobObject(HANDLE process, DWORD session);

private:
    SmOrchestrator                                        so_;
    SmReconfigureBroker                                   srb_;
    sml_ext::logger                                       logger_;
    sml::sm<SmOrchestrator, sml::logger<sml_ext::logger>> sm_;

    DWORD session_      = ipc::KnownSession::Any;
    bool  shuttingDown_ = false;
    // wil::unique_event_failfast                         confStoreReady_ {wil::EventOptions::ManualReset};
    std::vector<std::shared_ptr<ChildProcessConfig>>   childProcessesConfigs_;
    std::vector<std::unique_ptr<ChildProcessInstance>> desiredChildProcesses_;
    std::vector<std::unique_ptr<ChildProcessInstance>> childProcesses_;
    std::map<DWORD, wil::unique_handle>                jobObjects_;
    ipc::MsgItem                                       curMsg_;
    bool                                               bootstrapping_ = false;
    json                                               brokerConfig_;

    void    UpdateChildProcessConfig();
    HRESULT LaunchChildProcesses() noexcept;
    void    CalcDesiredChildProcesses();

    //  dispatch to all but the sending child process
    HRESULT OnMessage(ChildProcessInstance* fromProcess, const ipc::MsgItem& msgItem) noexcept;

    HRESULT SendToAllChildren(const ipc::MsgItem& msgItem) noexcept;
};
