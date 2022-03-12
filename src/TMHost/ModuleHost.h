#pragma once

#include "ipc.h"
#include "ManagedHost.h"
#include "NativeModule.h"

class ModuleHost final
{
    friend ManagedHost;
    friend NativeModule;

public:
    ModuleHost() = default;
    int Run();

private:
    // message from broker
    HRESULT OnMessageFromBroker(const std::string_view msg, const ipc::Target& target);

    HRESULT LoadModule(const std::wstring& name) noexcept;
    HRESULT UnloadModule(const std::wstring& name) noexcept;

    HRESULT LoadNativeModule(const std::filesystem::path& path) noexcept;
    HRESULT LoadManagedDllModule(const std::filesystem::path& path) noexcept;
    HRESULT LoadManagedExeModule(const std::filesystem::path& path) noexcept;

    ipc::Target                                target_;
    std::string                                groupName_;
    wil::unique_event_failfast                 terminate_ {wil::EventOptions::ManualReset};
    std::unique_ptr<ManagedHost>               managedHost_;
    std::vector<std::unique_ptr<NativeModule>> nativeModules_;
};
