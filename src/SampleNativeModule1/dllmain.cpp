#include "pch.h"
#include <Windows.h>
#include <string>
#include <string_view>
#include <chrono>

#include "ipc.h"
using namespace std::chrono_literals;

void*                         Mod      = nullptr;
ipc::SendMsg                  SendMsg  = nullptr;
ipc::SendDiag                 SendDiag = nullptr;
std::unique_ptr<std::jthread> PingThread;

extern "C" __declspec(dllexport) HRESULT InitModule(void* mod, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag)
{
    Mod      = mod;
    SendMsg  = sendMsg;
    SendDiag = sendDiag;

    /* while (!::IsDebuggerPresent())
     {
         ::Sleep(1000);
     }

     ::DebugBreak();*/

    PingThread = std::make_unique<std::jthread>([](std::stop_token stoken) {
        Guid svc(L"{DA20876D-E81D-4AE7-912D-92E229EB871E}");
        int  n = 0;
        while (!stoken.stop_requested())
        {
            /*char msg[] = "[INF] A123456789\r\n[INF] B123456789\r\n";
            SendDiag(Mod, msg);*/

            SendMsg(Mod, std::format("{} Hello World!", n++).c_str(), &svc, 1);

            std::this_thread::sleep_for(5s);
        }
    });
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT TermModule()
{
    if (PingThread)
    {
        PingThread->request_stop();
        PingThread->join();
    }

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
