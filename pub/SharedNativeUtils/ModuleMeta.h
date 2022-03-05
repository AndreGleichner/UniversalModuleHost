#pragma once

#include <string>
#include <unordered_set>

#include <wil/win32_helpers.h>
#include <absl/hash/hash.h>

#include <nlohmann/json.hpp>
#include "guid.h"
#include "string_extensions.h"

namespace ipc
{

struct ModuleMeta
{
    DWORD                           Pid;
    std::string                     Name;
    std::unordered_set<std::string> Services;

    ModuleMeta() = default;

    ModuleMeta(std::unordered_set<Guid, absl::Hash<Guid>> services)
    {
        Pid = ::GetCurrentProcessId();

        auto dll = wil::GetModuleFileNameW((HMODULE)wil::GetModuleInstanceHandle());
        Name     = Strings::ToUtf8(std::filesystem::path(dll.get()).stem().wstring());

        for (const auto& s : services)
        {
            Services.emplace(s.ToUtf8());
        }
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModuleMeta, Pid, Name, Services);
}
