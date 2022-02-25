#pragma once

#include <string>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace ipc
{

struct ModuleMeta
{
    DWORD                           Pid;
    std::string                     Name;
    std::unordered_set<std::string> Services;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModuleMeta, Pid, Name, Services);
}
