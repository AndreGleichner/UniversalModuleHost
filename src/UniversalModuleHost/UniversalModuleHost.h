#pragma once

#include "ipc.h"
#include "ManagedHost.h"

namespace ModuleHostApp
{
bool StartAsync();
}

class UniversalModuleHost final
{
    friend ManagedHost;

public:
    UniversalModuleHost() = default;
    int Run();

private:
    // message from broker
    void OnMessage(const std::string_view msg, const ipc::Target& target);

    void OnModOut(PCWSTR mod, PCWSTR message);

    HRESULT LoadModule(const std::wstring& path) noexcept;
    HRESULT UnloadModule(const std::wstring& path) noexcept;

    HRESULT LoadNativeModule(const std::wstring& path) noexcept;
    HRESULT LoadManagedModule(const std::wstring& path) noexcept;

    ipc::Target                  target_;
    wil::unique_event_failfast   terminate_ {wil::EventOptions::ManualReset};
    std::unique_ptr<ManagedHost> managedHost_;
};
