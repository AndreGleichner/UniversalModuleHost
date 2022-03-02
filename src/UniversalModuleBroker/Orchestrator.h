#pragma once
#include <Windows.h>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "guid.h"
#include "ipc.h"

#include "ChildProcessInstance.h"

struct ChildProcessConfig;

class Orchestrator final
{
    friend ChildProcessInstance;

public:
    Orchestrator();

    HRESULT Init() noexcept;
    HRESULT Release() noexcept;
    HRESULT OnSessionChange(DWORD dwEventType, DWORD dwSessionId) noexcept;
    HRESULT UpdateChildProcessConfig(const json& conf) noexcept;
    HRESULT LaunchChildProcesses() noexcept;

    void ShuttingDown()
    {
        shuttingDown_ = true;
    }

    bool IsShuttingDown()
    {
        return shuttingDown_;
    }

    void AssignProcessToJobObject(const ChildProcessInstance* childProcess);
    void AssignProcessToJobObject(HANDLE process, DWORD session);

private:
    // dispatch to all but the sending child process
    HRESULT OnMessage(
        ChildProcessInstance* fromProcess, const std::string_view msg, const ipc::Target& target) noexcept;


    DWORD session_ = ipc::KnownSession::Any;

    bool shuttingDown_ = false;

    std::vector<std::shared_ptr<ChildProcessConfig>>   childProcessesConfigs_;
    std::vector<std::unique_ptr<ChildProcessInstance>> childProcesses_;
    std::map<DWORD, wil::unique_handle>                jobObjects_;
};
