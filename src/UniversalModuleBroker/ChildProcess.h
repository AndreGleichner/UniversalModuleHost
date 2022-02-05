#pragma once
#include <Windows.h>
#include <wil/resource.h>
#include "ipc.h"

class ChildProcess final
{
public:
    ChildProcess(bool allUsers, bool wow64, bool higherIntegrityLevel, const std::string& groupName,
        const std::vector<std::wstring>& modules)
        : target_(Guid(true), ipc::KnownSession::HostInit)
        , allUsers_(allUsers)
        , wow64_(wow64)
        , higherIntegrityLevel_(higherIntegrityLevel)
        , groupName_(groupName)
        , modules_(modules)
    {
    }

    HRESULT Launch() noexcept;
    HRESULT Terminate() noexcept;
    HRESULT LoadModules() noexcept;
    HRESULT UnloadModules() noexcept;

private:
    void StartForwardStderr() noexcept;

    void OnMessage(const std::string_view msg, const ipc::Target& target)
    {
    }

    ipc::Target                     target_;
    wil::unique_process_handle      process_;
    wil::unique_handle              inRead_;
    wil::unique_handle              inWrite_;
    wil::unique_handle              outRead_;
    wil::unique_handle              outWrite_;
    wil::unique_handle              errRead_;
    wil::unique_handle              errWrite_;
    std::thread                     stderrForwarder_;
    std::thread                     reader_;
    bool                            allUsers_;
    bool                            wow64_;
    bool                            higherIntegrityLevel_;
    const std::string               groupName_;
    const std::vector<std::wstring> modules_;
};
