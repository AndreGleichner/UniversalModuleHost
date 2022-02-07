#include "pch.h"
#include <fstream>
#include "BrokerInstance.h"
#include "UmhProcess.h"
#include "string_extensions.h"
using namespace Strings;
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <wil/result.h>

// Process broker.json and launch host child processes as configured.
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

        if (allUsers && Process::IsWindowsService())
        {
            // Walk all currently active sessions to create a child process in every.

            wil::unique_wtsmem_ptr<WTS_SESSION_INFOW> sessionInfo;
            DWORD                                     sessionCount = 0;

            RETURN_IF_WIN32_BOOL_FALSE(
                ::WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, wil::out_param(sessionInfo), &sessionCount));

            for (DWORD sess = 0; sess < sessionCount; ++sess)
            {
                auto si = &sessionInfo.get()[sess];
                if (si->State != WTSActive && si->State != WTSDisconnected)
                    continue;

                if (si->SessionId == 0)
                    continue;

                auto cp = std::make_unique<ChildProcess>(
                    this, allUsers, wow64, higherIntegrityLevel, groupName, modules, si->SessionId);
                childProcesses_.push_back(std::move(cp));
            }
        }
        else
        {
            auto cp = std::make_unique<ChildProcess>(this, allUsers, wow64, higherIntegrityLevel, groupName, modules);
            childProcesses_.push_back(std::move(cp));
        }

        // break; // TODO remove
    }

    for (auto& process : childProcesses_)
    {
        process->Launch();
    }
    for (auto& process : childProcesses_)
    {
        process->LoadModules();
    }

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

void BrokerInstance::OnMessage(const ChildProcess* fromProcess, const std::string_view msg, const ipc::Target& target)
{
    if (target.Service == ipc::KnownService::Broker)
    {
        // TODO: add valuable actions the broker may perform.
        return;
    }

    for (auto& process : childProcesses_)
    {
        if (process.get() != fromProcess)
            process->SendMsg(msg, target);
    }
}
