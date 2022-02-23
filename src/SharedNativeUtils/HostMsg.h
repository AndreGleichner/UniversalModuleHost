#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include "guid.h"
#include "string_extensions.h"
using namespace Strings;

namespace ipc
{

struct HostInitMsg
{
    Guid        Service;
    std::string GroupName;
};

inline void to_json(json& j, const HostInitMsg& msg)
{
    j = json {{"Service", ToUtf8(msg.Service.ToString())}, {"GroupName", msg.GroupName}};
}

inline void from_json(const json& j, HostInitMsg& msg)
{
    msg.Service.Parse(ToUtf16(j["Service"]));
    j.at("GroupName").get_to(msg.GroupName);
}

struct HostCmdMsg
{
    enum class Cmd
    {
        Terminate,
        CtrlModule
    };
    Cmd         Cmd;
    std::string Args; // e.g. CtrlModule => HostCtrlModuleArgs as JSON
};

inline void to_json(json& j, const HostCmdMsg& msg)
{
    j = json {{"Cmd", msg.Cmd}, {"Args", msg.Args}};
}

inline void from_json(const json& j, HostCmdMsg& msg)
{
    j.at("Cmd").get_to(msg.Cmd);
    j.at("Args").get_to(msg.Args);
}

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
inline void to_json(json& j, const HostCtrlModuleArgs& msg)
{
    j = json {{"Cmd", msg.Cmd}, {"Module", msg.Module}};
}

inline void from_json(const json& j, HostCtrlModuleArgs& msg)
{
    j.at("Cmd").get_to(msg.Cmd);
    j.at("Module").get_to(msg.Module);
}

}
