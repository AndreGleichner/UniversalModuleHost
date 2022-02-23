#pragma once

#include <string>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include "guid.h"
#include "string_extensions.h"

using namespace Strings;

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
