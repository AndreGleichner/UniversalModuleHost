#pragma once
#include "ChildProcess.h"

class BrokerInstance final
{
    friend ChildProcess;

public:
    BrokerInstance()
    {
    }

    HRESULT Init() noexcept;
    HRESULT Release() noexcept;
    HRESULT OnSessionChange(DWORD dwEventType, DWORD dwSessionId) noexcept;

    void ShuttingDown()
    {
        shuttingDown_ = true;
    }

    bool IsShuttingDown()
    {
        return shuttingDown_;
    }

private:
    // dispatch to all but the sending child process
    void OnMessage(const ChildProcess* fromProcess, const std::string_view msg, const ipc::Target& target);

    std::vector<std::unique_ptr<ChildProcess>> childProcesses_;

    bool shuttingDown_ = false;
};
