#pragma once
#include "ModuleBase.h"
#include "ipc.h"

class ShellExecModule final : public ModuleBase
{
public:
    ShellExecModule() : ModuleBase({ipc::ShellExecTopic})
    {
    }

protected:
    HRESULT OnMessage(std::string_view msg, const ipc::Topic& topic) noexcept override;
};
