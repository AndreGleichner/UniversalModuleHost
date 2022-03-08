#pragma once

#include "ipc.h"

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
