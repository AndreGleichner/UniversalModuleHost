#pragma once
#include "ChildProcess.h"

class BrokerInstance final
{
    friend ChildProcess;

public:
    BrokerInstance();

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

    bool ShouldBreakAwayFromJob(const ChildProcess* childProcess) const;
    void AssignProcessToJobObject(const ChildProcess* childProcess);
    void AssignProcessToJobObject(HANDLE process, DWORD session);

    std::vector<std::unique_ptr<ChildProcess>> childProcesses_;
    std::map<DWORD, wil::unique_handle>        jobObjects_;

    DWORD session_ = ipc::KnownSession::Any;

    bool shuttingDown_ = false;
};
