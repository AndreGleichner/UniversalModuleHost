#include "pch.h"
#include <fstream>
#include "BrokerInstance.h"
#include "UmhProcess.h"
#include "string_extensions.h"
using namespace Strings;
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <wil/result.h>

HRESULT BrokerInstance::Init() noexcept
try
{
    auto confFile   = Process::ImagePath().replace_filename(L"broker.json");
    auto modulesDir = Process::ImagePath().replace_filename(L"modules");

    std::ifstream confStream(confFile.c_str());
    json          conf;
    confStream >> conf;

    for (auto& p : conf["ChildProcesses"])
    {
        bool        allUsers             = p["Session"] == "AllUsers";
        bool        wow64                = (sizeof(void*) == 4) && p.contains("Wow64") && p["Wow64"];
        bool        higherIntegrityLevel = p.contains("IntegrityLevel") && p["IntegrityLevel"] == "Higher";
        std::string groupName            = p["GroupName"];
        std::vector<std::wstring> modules;

        for (auto& m : p["Modules"])
        {
            modules.push_back(ToUtf16(m));
        }

        auto cp = std::make_unique<ChildProcess>(allUsers, wow64, higherIntegrityLevel, groupName, modules);
        childProcesses_.push_back(std::move(cp));

        break;
    }

    for (auto& process : childProcesses_)
    {
        process->Launch();
    }
    /*for (auto& process : childProcesses_)
    {
        process->LoadModules();
    }*/

    return S_OK;
}
CATCH_RETURN();

HRESULT BrokerInstance::Release() noexcept
try
{
    for (auto& process : childProcesses_)
    {
        process->Terminate();
    }
    return S_OK;
}
CATCH_RETURN();

HRESULT BrokerInstance::OnSessionChange(DWORD dwEventType, DWORD dwSessionId) noexcept
try
{
    return S_OK;
}
CATCH_RETURN();
