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
        std::string session = p["Session"];
        if (session == "Broker")
        {
            // Launch in same session as the running broker process.
        }
        else if (session == "AllUsers")
        {
            bool higherIntegrityLevel = p["IntegrityLevel"] == "Higher";
        }

        std::string groupName = p["GroupName"];
        for (auto& m : p["Modules"])
        {
            std::wstring moduleFile = ToUtf16(m);
        }
    }

    return S_OK;
}
CATCH_RETURN();

HRESULT BrokerInstance::Release() noexcept
try
{
    return S_OK;
}
CATCH_RETURN();
