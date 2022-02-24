#include "pch.h"
#include <Windows.h>
#include <string>
#include <string_view>
#include <shellapi.h>

#include "ipc.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "ModuleMeta.h"
#include "string_extensions.h"
using namespace Strings;

#include <wil/win32_helpers.h>

void*         Mod      = nullptr;
ipc::SendMsg  SendMsg  = nullptr;
ipc::SendDiag SendDiag = nullptr;

extern "C" __declspec(dllexport) HRESULT InitModule(void* mod, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag)
{
    Mod      = mod;
    SendMsg  = sendMsg;
    SendDiag = sendDiag;

    // Tell the world which services we provide
    auto                  dll = wil::GetModuleFileNameW((HMODULE)wil::GetModuleInstanceHandle());
    std::filesystem::path path(dll.get());

    json msg = ipc::ModuleMeta {
        ::GetCurrentProcessId(), ToUtf8(path.stem().wstring()), {ToUtf8(ipc::KnownService::ShellExec.ToString())}};

    RETURN_IF_FAILED(SendMsg(Mod, msg.dump().c_str(), &ipc::KnownService::ModuleMetaConsumer, (DWORD)-1));

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
