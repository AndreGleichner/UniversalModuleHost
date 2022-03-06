#include "pch.h"
#include "SampleNativeModule.h"

#include <chrono>

using namespace std::chrono_literals;

HRESULT SampleNativeModule::OnInitialize() noexcept
try
{
    pingThread_ = std::make_unique<std::jthread>([&](std::stop_token stoken) {
        Guid svc(L"{DA20876D-E81D-4AE7-912D-92E229EB871E}");
        int  n = 0;
        while (!stoken.stop_requested())
        {
            /*char msg[] = "[INF] A123456789\r\n[INF] B123456789\r\n";
            SendDiag(Mod, msg);*/

            SendMsg(std::format("{} Hello World!", n++).c_str(), ipc::Target(svc));

            std::this_thread::sleep_for(5s);
        }
    });
    return S_OK;
}
CATCH_RETURN()

HRESULT SampleNativeModule::OnTerminate() noexcept
try
{
    if (pingThread_)
    {
        pingThread_->request_stop();
        pingThread_->join();
        pingThread_.reset();
    }

    return S_OK;
}
CATCH_RETURN()

HRESULT SampleNativeModule::OnMessage(std::string_view msg, const ipc::Target& target) noexcept
try
{
    return S_OK;
}
CATCH_RETURN()
