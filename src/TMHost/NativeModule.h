#pragma once

#include "ipc.h"

// forward declarations for module entry points
namespace Entry
{
HRESULT InitModule(void* mod, ipc::Pub sendMsg);
HRESULT TermModule();
HRESULT OnMessage(PCSTR msg, const ipc::Topic* topic);
}

class ModuleHost;
class NativeModule final
{
    friend ModuleHost;

public:
    NativeModule(const std::filesystem::path& path) : path_(path)
    {
    }

    HRESULT Load();
    HRESULT Unload();

    // send message to module
    HRESULT Send(const ipc::MsgItem& msgItem) noexcept;
    // message from module
    static HRESULT CALLBACK OnMsg(void* mod, PCSTR msg, const Guid* topicId, DWORD session) noexcept;
    // log from module
    static HRESULT CALLBACK OnDiag(void* mod, PCSTR msg) noexcept;

private:
    const std::filesystem::path path_;
    wil::unique_hmodule         hmodule_;

    decltype(&Entry::InitModule) InitModule_ = nullptr;
    decltype(&Entry::TermModule) TermModule_ = nullptr;
    decltype(&Entry::OnMessage)  OnMessage_  = nullptr;
};
