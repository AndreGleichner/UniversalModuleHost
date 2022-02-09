#pragma once

#include "ipc.h"
#include "ManagedHost.h"

// forward declarations for module entry points
namespace Entry
{
HRESULT InitModule();
HRESULT TermModule();
HRESULT ConnectModule(void* mod, /*ipc::SendMsg sendMsg,*/ ipc::SendDiag sendDiag);
HRESULT OnMessage(PCSTR msg, const ipc::Target* target);
}

class Module final
{
public:
    Module(const std::wstring& path)
    {
    }

    HRESULT Load();
    HRESULT Unload();

    // send message to module
    HRESULT Send(const std::string& msg, const ipc::Target& target) noexcept;
    // message from module
    static HRESULT CALLBACK OnMsg(void* mod, PCSTR msg, const ipc::Target& target);
    // log from module
    static HRESULT CALLBACK OnDiag(void* mod, PCSTR msg);

private:
    const std::wstring  path_;
    wil::unique_hmodule hmodule_;

    decltype(&Entry::InitModule)    InitModule_;
    decltype(&Entry::TermModule)    TermModule_;
    decltype(&Entry::ConnectModule) ConnectModule_;
    decltype(&Entry::OnMessage)     OnMessage_;
};

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

    HRESULT LoadModule(const std::wstring& name) noexcept;
    HRESULT UnloadModule(const std::wstring& name) noexcept;

    HRESULT LoadNativeModule(const std::wstring& path) noexcept;
    HRESULT LoadManagedModule(const std::wstring& path) noexcept;

    ipc::Target                  target_;
    std::string                  groupName_;
    wil::unique_event_failfast   terminate_ {wil::EventOptions::ManualReset};
    std::unique_ptr<ManagedHost> managedHost_;
};
