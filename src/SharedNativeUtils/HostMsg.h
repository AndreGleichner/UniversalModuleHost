#pragma once

#include <nlohmann/json.hpp>
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

void to_json(nlohmann::json& j, const HostInitMsg& msg)
{
    j = nlohmann::json {{"Service", ToUtf8(msg.Service.ToString())}, {"GroupName", msg.GroupName}};
}

void from_json(const nlohmann::json& j, HostInitMsg& msg)
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

void to_json(nlohmann::json& j, const HostCmdMsg& msg)
{
    j = nlohmann::json {{"Cmd", msg.Cmd}, {"Args", msg.Args}};
}

void from_json(const nlohmann::json& j, HostCmdMsg& msg)
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
void to_json(nlohmann::json& j, const HostCtrlModuleArgs& msg)
{
    j = nlohmann::json {{"Cmd", msg.Cmd}, {"Module", msg.Module}};
}

void from_json(const nlohmann::json& j, HostCtrlModuleArgs& msg)
{
    j.at("Cmd").get_to(msg.Cmd);
    j.at("Module").get_to(msg.Module);
}

}
