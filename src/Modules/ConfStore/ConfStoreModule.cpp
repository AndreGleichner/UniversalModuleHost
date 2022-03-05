#include "pch.h"
#include <format>
#include <fstream>

#include <wil/result.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "spdlog_headers.h"

#include "string_extensions.h"
using namespace Strings;

#include "ConfStoreModule.h"
#include "ConfStore.h"
#include "env.h"
#include "UmhProcess.h"

json ConfStoreModule::DefaultConfigFor(const std::string& moduleName) noexcept
{
    json conf;

    try
    {
        std::wstring          name = ToUtf16(moduleName);
        std::filesystem::path defConfPath;

        if (name == L"Broker")
        {
            defConfPath = Process::ImagePath().replace_filename(L"broker.json");
        }
        else
        {
            defConfPath = ModuleBase::PathFor(name.c_str(), false).replace_filename(name + L".json");
        }

        if (std::filesystem::exists(defConfPath))
        {
            std::ifstream confStream(defConfPath.c_str());
            confStream >> conf;
        }
    }
    catch (...)
    {
    }
    return conf;
}

namespace
{
json GetModuleConf(json& conf, const std::string& moduleName)
{
    // If module conf is not present this will create a null object, which we need to remove.
    // invoking the const operator[] wont create such null object instead would assert.
    json val = conf[json::json_pointer("/" + moduleName)];
    if (val.is_null())
    {
        // revert adding the null object
        (void)conf.erase(moduleName);
    }

    return val;
}
}

HRESULT ConfStoreModule::OnMessage(std::string_view msg, const ipc::Target& target) noexcept
try
{
    if (target.Equals(ipc::Target(ipc::KnownService::ConfStore)))
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

        // Read current config or init to empty object.
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
                conf  = json({});
                store = true;
                SPDLOG_ERROR("Invalid ConfStore file content {}", j);
            }
        }
        else
        {
            SPDLOG_INFO(L"Creating new ConfStore file {}", file.c_str());

            conf = json({});

            confFile.reset(_wfsopen(file.c_str(), L"w", _SH_DENYWR));
            RETURN_HR_IF(E_FAIL, !confFile);
            store = true;
        }

        if (c.Cmd == ipc::ConfStore::Cmd::Query)
        {
            // Send json fragment as selected by given module name.
            // Make a JSON Pointer from module name (https://datatracker.ietf.org/doc/html/rfc6901)
            // const auto val = conf[json::json_pointer("/" + c.Args)];
            const json val = GetModuleConf(conf, c.Args);

            json        res;
            std::string r;
            if (val.is_null())
            {
                // Try reading a default from module specific file.
                res = DefaultConfigFor(c.Args);
                if (!res.is_null())
                {
                    r = res.dump();
                    SPDLOG_TRACE("Queried ConfStore default value {}", r);
                }
            }
            else
            {
                res[c.Args] = val;

                r = res.dump();
                SPDLOG_TRACE("Queried ConfStore value {}", r);
            }

            if (!r.empty())
                SendMsg(r.c_str(), ipc::Target(ipc::KnownService::ConfConsumer));
        }
        else
        {
            // Apply the given json patch.
            // https://datatracker.ietf.org/doc/html/rfc7386

            auto patch   = json::parse(c.Args);
            auto modName = patch.begin().key();

            // 1st check whether the module conf is already stored.
            // If not, try reading a default to get that as a base.
            const json val = GetModuleConf(conf, modName);
            if (val.is_null())
            {
                // Try reading a default from module specific file.
                json defConf = DefaultConfigFor(modName);
                if (!defConf.is_null())
                {
                    // merge that default into the current conf, before applying the patch below
                    conf.merge_patch(defConf);
                }
            }

            conf.merge_patch(patch);
            store = true;

            SPDLOG_TRACE("Apply ConfStore patch {}", patch.dump());

            // Read back the just patched conf for the module and broadcast it.
            json        res = {{modName, GetModuleConf(conf, modName)}};
            std::string r   = res.dump();
            if (!r.empty())
                SendMsg(r.c_str(), ipc::Target(ipc::KnownService::ConfConsumer));
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
