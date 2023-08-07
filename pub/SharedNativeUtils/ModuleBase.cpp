#include "pch.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "spdlog_headers.h"
#include "SpdlogCustomFormatter.h"
#include "env.h"

#include "ModuleBase.h"
#include "ModuleMeta.h"
#include "TMProcess.h"

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

HRESULT ModuleBase::Initialize(void* mod, ipc::Pub sendMsg) noexcept
try
{
    mod_     = mod;
    sendMsg_ = sendMsg;

    SetDefaultLogger();

    // Tell the world which topics we subscribe
    json msg = ipc::ModuleMeta(topicIds_);

    RETURN_IF_FAILED(sendMsg_(mod_, msg.dump().c_str(), &ipc::ModuleMetaTopic, (DWORD)-1));

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

HRESULT ModuleBase::HandleMessage(std::string_view msg, const ipc::Topic& topic) noexcept
try
{
    if (!topicIds_.contains(ipc::AllTopics) && !topicIds_.contains(topic.TopicId))
        return S_FALSE;

    RETURN_IF_FAILED(OnMessage(msg, topic));
    return S_OK;
}
CATCH_RETURN()

HRESULT ModuleBase::Publish(std::string_view msg, const ipc::Topic& topic) noexcept
try
{
    RETURN_HR_IF_NULL(E_FAIL, sendMsg_);
    RETURN_IF_FAILED(sendMsg_(mod_, msg.data(), &topic.TopicId, topic.Session));
    return S_OK;
}
CATCH_RETURN()

std::filesystem::path ModuleBase::PathFor(std::wstring_view moduleName, bool bitnessSpecific)
{
    const std::wstring bitness = bitnessSpecific ? (sizeof(void*) == 4 ? L"32.dll" : L"64.dll") : L".dll";
    return Process::ImagePath().replace_filename(L"modules") / moduleName.data() / (std::wstring(moduleName) + bitness);
}
