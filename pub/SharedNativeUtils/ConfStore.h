#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace ipc
{
static const Guid ConfStoreTopic {L"{8583CDC9-DB92-45BE-90CE-4D3AA4CD14F5}"};
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

static const Guid ConfTopic {L"{8ED3A4D7-7C78-4B88-A547-A4D87A9DDC35}"};
struct Conf
{
    std::string Val;
};
}
