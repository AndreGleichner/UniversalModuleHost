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
static const Guid ModuleMetaTopic {L"{6E6A094C-839F-4EAF-BD22-08CB9E1A318F}"};
struct ModuleMeta
{
    DWORD                           Pid;
    std::string                     Name;
    std::unordered_set<std::string> TopicIds;

    ModuleMeta() = default;

    ModuleMeta(std::unordered_set<Guid, absl::Hash<Guid>> topicIds)
    {
        Pid = ::GetCurrentProcessId();

        auto dll = wil::GetModuleFileNameW((HMODULE)wil::GetModuleInstanceHandle());
        Name     = Strings::ToUtf8(std::filesystem::path(dll.get()).stem().wstring());

        for (const auto& t : topicIds)
        {
            TopicIds.emplace(t.ToUtf8());
        }
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModuleMeta, Pid, Name, TopicIds);
}
