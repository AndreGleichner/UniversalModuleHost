#include "pch.h"
#include <Windows.h>
#include <string>
#include <string_view>
#include <shellapi.h>

#include "ipc.h"

void*         Mod      = nullptr;
ipc::SendMsg  SendMsg  = nullptr;
ipc::SendDiag SendDiag = nullptr;

extern "C" __declspec(dllexport) HRESULT InitModule(void* mod, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag)
{
    Mod      = mod;
    SendMsg  = sendMsg;
    SendDiag = sendDiag;

    // json msg = ipc::HostCmdMsg {ipc::HostCmdMsg::Cmd::Terminate, ""};

    // RETURN_IF_FAILED(ipc::Send(inWrite_.get(), msg.dump(), target_));

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT TermModule()
{
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT OnMessage(PCSTR msg, const ipc::Target* target)
{
    if (target->Equals(ipc::Target(ipc::KnownService::ShellExec)))
    {
        ::ShellExecuteA(nullptr, "open", msg, nullptr, nullptr, SW_SHOWNORMAL);
    }
    return S_OK;
}

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
