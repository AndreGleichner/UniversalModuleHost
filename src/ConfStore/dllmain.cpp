#include "pch.h"
#include <Windows.h>
#include <string>
#include <string_view>
#include <stdio.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "ipc.h"
#include "spdlog_headers.h"
#include "SpdlogCustomFormatter.h"
#include "ConfStore.h"
#include "env.h"
#include "string_extensions.h"
using namespace Strings;
#include "ModuleMeta.h"

void*         Mod      = nullptr;
ipc::SendMsg  SendMsg  = nullptr;
ipc::SendDiag SendDiag = nullptr;

namespace
{
void SetDefaultLogger()
{
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting

    auto console_sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();

    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
        "[%l] %-64v [%P/%t][%! @ %s:%#]");
    console_sink->set_formatter(std::move(formatter));

    auto logger = std::make_shared<spdlog::logger>("umh", console_sink);
    logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(logger);
}
}

extern "C" __declspec(dllexport) HRESULT InitModule(void* mod, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag)
{
    Mod      = mod;
    SendMsg  = sendMsg;
    SendDiag = sendDiag;

    SetDefaultLogger();

    /* while (!::IsDebuggerPresent())
     {
         ::Sleep(1000);
     }

     ::DebugBreak();*/

    // Tell the world which services we provide
    json msg = ipc::ModuleMeta({ipc::KnownService::ConfStore});

    RETURN_IF_FAILED(SendMsg(Mod, msg.dump().c_str(), &ipc::KnownService::ModuleMetaConsumer, (DWORD)-1));

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT TermModule()
{
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT OnMessage(PCSTR msg, const ipc::Target* target)
try
{
    if (target->Equals(ipc::Target(ipc::KnownService::ConfStore)))
    {
        const auto c = json::parse(msg).get<ipc::ConfStore>();
        if (c.Cmd != ipc::ConfStore::Cmd::Query && c.Cmd != ipc::ConfStore::Cmd::Update)
        {
            SPDLOG_ERROR("Invalid ConfStore command {}", c.Cmd);
            return E_INVALIDARG;
        }

        // Sync file access in any session.

        wil::unique_mutex_failfast lock(L"Global\\ConfStore-{93D385EC-6F61-4594-9386-464E0802BAAB}");
        auto                       releaseOnExit = lock.acquire();

        auto             confDir = env::PrivateDataDir(L"conf");
        auto             file    = confDir / L"store.json";
        wil::unique_file confFile;
        size_t           size = 0;
        json             conf;
        bool             store = false;

        // Read current config or init to empty.
        // Sample where Mod1/2 are module names:
        //     {
        //       "Mod1": {
        //         "ConfVal": 12
        //       },
        //       "Mod2": {
        //         "ConfVal": 22
        //       }
        //     }

        if (std::filesystem::exists(file) && (size = std::filesystem::file_size(file)) > 2)
        {
            confFile.reset(_wfsopen(file.c_str(), L"r+", _SH_DENYWR));
            RETURN_HR_IF(E_FAIL, !confFile);

            // Read entire content, which should be some JSON.
            std::string j;
            j.resize(size);
            RETURN_HR_IF(E_FAIL, 0 == fread_s(j.data(), size, 1, size, confFile.get()));
            try
            {
                conf = json::parse(j);
            }
            catch (...)
            {
                // Seems some invalid non-JSON file content
                conf  = "{}"_json;
                store = true;
                SPDLOG_ERROR("Invalid ConfStore file content {}", j);
            }
        }
        else
        {
            SPDLOG_INFO(L"Creating new ConfStore file {}", file.c_str());

            conf = "{}"_json;
            confFile.reset(_wfsopen(file.c_str(), L"w", _SH_DENYWR));
            RETURN_HR_IF(E_FAIL, !confFile);
            store = true;
        }

        if (c.Cmd == ipc::ConfStore::Cmd::Query)
        {
            releaseOnExit.reset();

            // Send json fragment as selected by given module name.
            // Make a JSON Pointer from module name (https://datatracker.ietf.org/doc/html/rfc6901)
            const auto& val = conf[json::json_pointer("/" + c.Args)];
            json        res;
            res[c.Args] = val;

            auto r = res.dump();
            SPDLOG_TRACE("Queried ConfStore value {}", r);

            SendMsg(Mod, r.c_str(), &ipc::KnownService::ConfConsumer, ipc::KnownSession::Any);
        }
        else
        {
            // Apply the given json patch.
            // https://datatracker.ietf.org/doc/html/rfc7386

            auto patch = json::parse(c.Args);
            conf.merge_patch(patch);
            store = true;

            SPDLOG_TRACE("Apply ConfStore patch {}", patch.dump());
        }

        if (store)
        {
            auto   s    = conf.dump(2);
            size_t size = s.length();

            RETURN_HR_IF(E_FAIL, 0 != fseek(confFile.get(), 0, SEEK_SET));
            RETURN_HR_IF(E_FAIL, size != fwrite(s.c_str(), 1, size, confFile.get()));
        }
    }

    return S_OK;
}
CATCH_RETURN()

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
