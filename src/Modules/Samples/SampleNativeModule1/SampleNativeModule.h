#pragma once
#include "ModuleBase.h"

class SampleNativeModule : public ModuleBase
{
public:
    SampleNativeModule() : ModuleBase({ipc::AllTopics})
    {
    }

protected:
    HRESULT OnInitialize() noexcept override;
    HRESULT OnTerminate() noexcept override;
    HRESULT OnMessage(std::string_view msg, const ipc::Topic& topic) noexcept override;

private:
    std::unique_ptr<std::jthread> pingThread_;
};
