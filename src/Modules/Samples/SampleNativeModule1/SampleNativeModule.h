#pragma once
#include "ModuleBase.h"

class SampleNativeModule : public ModuleBase
{
public:
    SampleNativeModule() : ModuleBase({})
    {
    }

protected:
    HRESULT OnInitialize() noexcept override;
    HRESULT OnTerminate() noexcept override;
    HRESULT OnMessage(std::string_view msg, const ipc::Target& target) noexcept override;

private:
    std::unique_ptr<std::jthread> pingThread_;
};
