#pragma once

class BrokerInstance final
{
public:
    BrokerInstance()
    {
    }

    HRESULT Init() noexcept;
    HRESULT Release() noexcept;
    HRESULT OnSessionChange(DWORD dwEventType, DWORD dwSessionId) noexcept;
};
