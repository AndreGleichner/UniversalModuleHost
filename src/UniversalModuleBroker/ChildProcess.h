#pragma once
#include <Windows.h>
#include <wil/resource.h>
#include "ipc.h"

class BrokerInstance;
class ChildProcess final
{
public:
    ChildProcess(BrokerInstance* brokerInstance, bool allUsers, bool wow64, bool higherIntegrityLevel,
        const std::string& groupName, const std::vector<std::wstring>& modules, DWORD session = ipc::KnownSession::Any)
        : brokerInstance_(brokerInstance)
        , target_(Guid(true), session)
        , allUsers_(allUsers)
        , wow64_(wow64)
        , higherIntegrityLevel_(higherIntegrityLevel)
        , groupName_(groupName)
        , modules_(modules)
    {
    }

    HRESULT Launch(bool keepAlive = true) noexcept;
    HRESULT Terminate() noexcept;
    HRESULT LoadModules() noexcept;
    HRESULT UnloadModules() noexcept;

    HRESULT SendMsg(const nlohmann::json& msg, const ipc::Target& target);

private:
    void StartForwardStderr() noexcept;

    BrokerInstance*                 brokerInstance_;
    ipc::Target                     target_;
    wil::unique_process_information processInfo_;
    wil::unique_handle              inRead_;
    wil::unique_handle              inWrite_;
    wil::unique_handle              outRead_;
    wil::unique_handle              outWrite_;
    wil::unique_handle              errRead_;
    wil::unique_handle              errWrite_;
    std::thread                     stderrForwarder_;
    std::thread                     reader_;
    std::thread                     keepAlive_;
    bool                            allUsers_;
    bool                            wow64_;
    bool                            higherIntegrityLevel_;
    const std::string               groupName_;
    const std::vector<std::wstring> modules_;
};
