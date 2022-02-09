#include "pch.h"
#include <Windows.h>
#include <string>
#include <string_view>

#include "ipc.h"

extern "C" __declspec(dllexport) HRESULT InitModule()
{
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT TermModule()
{
    return S_OK;
}

ipc::SendMsg  SendMsg  = nullptr;
ipc::SendDiag SendDiag = nullptr;

extern "C" __declspec(dllexport) HRESULT ConnectModule(void* ctx, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag)
{
    SendMsg  = sendMsg;
    SendDiag = sendDiag;
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT OnMessage(PCSTR msg, const ipc::Target* target)
{
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
