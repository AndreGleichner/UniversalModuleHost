#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace ipc
{
struct ConfStore
{
    enum class Cmd
    {
        Query, // => Args = Module name
        Update // => Args = JSON MergePatch (https://datatracker.ietf.org/doc/html/rfc7386)
    };
    Cmd         Cmd;
    std::string Args;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConfStore, Cmd, Args);
}
