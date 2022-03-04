#include "pch.h"
#include <Windows.h>
#include <string>
#include <string_view>
#include <shellapi.h>
#include <format>

#include "ipc.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "ModuleMeta.h"
#include "string_extensions.h"
using namespace Strings;

#include <wil/result.h>
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
    json msg = ipc::ModuleMeta({ipc::KnownService::ShellExec});

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
        // Instead of directly calling ShellExecute() we have to execute this from within a process not being part of
        // the "kill on job close" job object. Otherwise stopping the broker would kill all its child processes
        // including a freshly started browser (e.g. Chrome). If the browser at the time of calling ShellExecute() is
        // already running everything is ok, as in that case the browser porcesses wont become part of the "kill on job
        // close" job object. If the only thing we wanna do is launching a web site we can run: CreateProcess("cmd /c
        // start https://www.heise.de")

        wil::unique_process_information processInfo;
        STARTUPINFO                     startInfo {sizeof(startInfo)};

        const DWORD creationFlags =
            CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS | CREATE_DEFAULT_ERROR_MODE | CREATE_BREAKAWAY_FROM_JOB;

        std::wstring cmdline = std::format(L"cmd /c start {}", ToUtf16(msg));

        RETURN_IF_WIN32_BOOL_FALSE(::CreateProcessW(nullptr, // applicationName
            const_cast<PWSTR>(cmdline.data()),               // commandLine shall be non-const
            nullptr,                                         // process security attributes
            nullptr,                                         // primary thread security attributes
            FALSE,                                           // handles are inherited
            creationFlags,                                   // creation flags
            nullptr,                                         // use parent's environment
            nullptr,                                         // use parent's current directory
            (LPSTARTUPINFOW)&startInfo,                      // STARTUPINFO pointer
            &processInfo));                                  // receives PROCESS_INFORMATION
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
