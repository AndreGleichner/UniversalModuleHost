#include "pch.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "spdlog_headers.h"
#include "SpdlogCustomFormatter.h"
#include "env.h"

#include "ModuleBase.h"
#include "ModuleMeta.h"

namespace
{
void SetDefaultLogger()
{
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting

    auto console_sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();

    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
        "[%l] %-64v [%P/%t][%! @ %s:%#]");
    console_sink->set_formatter(std::move(formatter));

    auto logger = std::make_shared<spdlog::logger>("umh", console_sink);
    logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(logger);
}
}

HRESULT ModuleBase::Initialize(void* mod, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag) noexcept
try
{
    mod_      = mod;
    sendMsg_  = sendMsg;
    sendDiag_ = sendDiag;

    SetDefaultLogger();

    // Tell the world which services we provide
    json msg = ipc::ModuleMeta(services_);

    RETURN_IF_FAILED(sendMsg_(mod_, msg.dump().c_str(), &ipc::KnownService::ModuleMetaConsumer, (DWORD)-1));

    RETURN_IF_FAILED(OnInitialize());
    return S_OK;
}
CATCH_RETURN()

HRESULT ModuleBase::Terminate() noexcept
try
{
    RETURN_IF_FAILED(OnTerminate());
    return S_OK;
}
CATCH_RETURN()

HRESULT ModuleBase::HandleMessage(std::string_view msg, const ipc::Target& target) noexcept
try
{
    if (!services_.contains(target.Service))
        return S_FALSE;

    RETURN_IF_FAILED(OnMessage(msg, target));
    return S_OK;
}
CATCH_RETURN()

HRESULT ModuleBase::SendMsg(std::string_view msg, const ipc::Target& target) noexcept
try
{
    RETURN_HR_IF_NULL(E_FAIL, sendMsg_);
    RETURN_IF_FAILED(sendMsg_(mod_, msg.data(), &target.Service, target.Session));
    return S_OK;
}
CATCH_RETURN()

HRESULT ModuleBase::SendDiag(std::string_view msg) noexcept
try
{
    RETURN_HR_IF_NULL(E_FAIL, sendDiag_);
    RETURN_IF_FAILED(sendDiag_(mod_, msg.data()));
    return S_OK;
}
CATCH_RETURN()
