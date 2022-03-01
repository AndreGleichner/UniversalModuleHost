#pragma once

#include "ipc.h"
#include "ManagedHost.h"
#include "guid.h"

// forward declarations for module entry points
namespace Entry
{
HRESULT InitModule(void* mod, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag);
HRESULT TermModule();
HRESULT OnMessage(PCSTR msg, const ipc::Target* target);
}

class UniversalModuleHost;
class NativeModule final
{
    friend UniversalModuleHost;

public:
    NativeModule(const std::filesystem::path& path) : path_(path)
    {
    }

    HRESULT Load();
    HRESULT Unload();

    // send message to module
    HRESULT Send(const std::string_view msg, const ipc::Target& target) noexcept;
    // message from module
    static HRESULT CALLBACK OnMsg(void* mod, PCSTR msg, const Guid* service, DWORD session) noexcept;
    // log from module
    static HRESULT CALLBACK OnDiag(void* mod, PCSTR msg) noexcept;

private:
    const std::filesystem::path path_;
    wil::unique_hmodule         hmodule_;

    decltype(&Entry::InitModule) InitModule_ = nullptr;
    decltype(&Entry::TermModule) TermModule_ = nullptr;
    decltype(&Entry::OnMessage)  OnMessage_  = nullptr;
};

class UniversalModuleHost final
{
    friend ManagedHost;
    friend NativeModule;

public:
    UniversalModuleHost() = default;
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
