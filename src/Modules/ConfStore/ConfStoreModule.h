#pragma once
#include "ModuleBase.h"
#include "ipc.h"
#include "ConfStore.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class ConfStoreModule final : public ModuleBase
{
public:
    ConfStoreModule() : ModuleBase({ipc::ConfStoreTopic})
    {
    }

protected:
    HRESULT OnMessage(std::string_view msg, const ipc::Topic& topic) noexcept override;

private:
    json DefaultConfigFor(const std::string& moduleName) noexcept;
};
