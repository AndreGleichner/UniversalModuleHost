#pragma once
#include "ChildProcess.h"

class BrokerInstance final
{
public:
    BrokerInstance()
    {
    }

    HRESULT Init() noexcept;
    HRESULT Release() noexcept;
    HRESULT OnSessionChange(DWORD dwEventType, DWORD dwSessionId) noexcept;

private:
    std::vector<std::unique_ptr<ChildProcess>> childProcesses_;
};
