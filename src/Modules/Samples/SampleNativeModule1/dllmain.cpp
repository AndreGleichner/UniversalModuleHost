#include "pch.h"

#include "SampleNativeModule.h"

SampleNativeModule g_module;

extern "C" __declspec(dllexport) HRESULT InitModule(void* mod, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag)
{
    RETURN_IF_FAILED(g_module.Initialize(mod, sendMsg, sendDiag));
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT TermModule()
{
    RETURN_IF_FAILED(g_module.Terminate());
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT OnMessage(PCSTR msg, const ipc::Target* target)
{
    RETURN_IF_FAILED(g_module.HandleMessage(msg, *target));
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
