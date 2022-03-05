#pragma once
#include "ModuleBase.h"
#include "ipc.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class ConfStoreModule final : public ModuleBase
{
public:
    ConfStoreModule() : ModuleBase({ipc::KnownService::ConfStore})
    {
    }

protected:
    HRESULT OnMessage(std::string_view msg, const ipc::Target& target) noexcept override;

private:
    json DefaultConfigFor(const std::string& moduleName) noexcept;
};
