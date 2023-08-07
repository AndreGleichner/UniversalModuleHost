#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include "guid.h"
#include "string_extensions.h"
using namespace Strings;

namespace ipc
{
// ipc::HostInit
static const Guid HostInitTopic {L"{AA810FBD-B33C-4895-8E82-8814EE849E02}"};
struct HostInit
{
    Guid        TopicId;
    std::string GroupName;
};
#pragma region json
inline void    to_json(json& j, const HostInit& msg)
{
    j = json {{"TopicId", msg.TopicId.ToUtf8()}, {"GroupName", msg.GroupName}};
}

inline void from_json(const json& j, HostInit& msg)
{
    msg.TopicId.Parse(ToUtf16(j["TopicId"]));
    j.at("GroupName").get_to(msg.GroupName);
}
#pragma endregion

static const Guid ManagedHostTopic {L"{7924FE60-C967-449C-BA5D-2EBAA7D16024}"};
struct HostCmd
{
    enum class Cmd
    {
        Terminate,
        CtrlModule
    };
    Cmd         Cmd;
    std::string Args; // e.g. CtrlModule => HostCtrlModuleArgs as JSON
};
#pragma region json
inline void    to_json(json& j, const HostCmd& msg)
{
    j = json {{"Cmd", msg.Cmd}, {"Args", msg.Args}};
}

inline void from_json(const json& j, HostCmd& msg)
{
    j.at("Cmd").get_to(msg.Cmd);
    j.at("Args").get_to(msg.Args);
}
#pragma endregion

// see HostCmd::Args
struct HostCtrlModuleArgs
{
    enum class Cmd
    {
        Load,
        Unload
    };
    Cmd         Cmd;
    std::string Module;
};
#pragma region json
inline void    to_json(json& j, const HostCtrlModuleArgs& msg)
{
    j = json {{"Cmd", msg.Cmd}, {"Module", msg.Module}};
}

inline void from_json(const json& j, HostCtrlModuleArgs& msg)
{
    j.at("Cmd").get_to(msg.Cmd);
    j.at("Module").get_to(msg.Module);
}
#pragma endregion
}
