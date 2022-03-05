#pragma once
#include "ModuleBase.h"
#include "ipc.h"

class ShellExecModule final : public ModuleBase
{
public:
    ShellExecModule() : ModuleBase({ipc::KnownService::ShellExec})
    {
    }

protected:
    HRESULT OnMessage(std::string_view msg, const ipc::Target& target) noexcept override;
};
