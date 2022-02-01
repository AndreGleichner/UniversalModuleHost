#pragma once

class BrokerInstance final
{
public:
    BrokerInstance()
    {
    }

    HRESULT Init() noexcept;
    HRESULT Release() noexcept;
};
