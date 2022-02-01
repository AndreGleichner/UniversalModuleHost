#include "pch.h"

#include "SpdlogCustomFormatter.h"
#include "UniversalModuleBrokerService.h"
#include "BrokerInstance.h"

#pragma region Logging
namespace
{
void SetDefaultLogger()
{
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting

    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
        "[%Y-%m-%d %T.%e %z] [%^%l%$] %-64v [%P/%t][%! @ %s:%#]");
    console_sink->set_formatter(std::move(formatter));

    auto logger = std::make_shared<spdlog::logger>("umh", console_sink);
    logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(logger);
}
}
#pragma endregion

BrokerInstance g_broker;

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    Process::SetThreadName(L"UMB-Native");

#ifdef DEBUG
    wil::g_fBreakOnFailure = true;
#endif

#pragma region ConfigureLogging
    auto cleanLogger = wil::scope_exit([&] { spdlog::shutdown(); });

    SetDefaultLogger();

    wil::SetResultLoggingCallback([](wil::FailureInfo const& failure) noexcept {
        constexpr std::size_t sizeOfLogMessageWithNul = 2048;
        wchar_t               logMessage[sizeOfLogMessageWithNul];
        if (SUCCEEDED(wil::GetFailureLogString(logMessage, sizeOfLogMessageWithNul, failure)))
        {
            spdlog::error(logMessage);
        }
    });
#pragma endregion

    int exitCode = 0;
    SPDLOG_INFO(L"Starting UniversalModuleBroker '{}'", lpCmdLine);
    auto logExit = wil::scope_exit([&] { SPDLOG_INFO("Exiting UniversalModuleBroker: {}", exitCode); });

    if (Process::IsWindowsService())
    {
        UniversalModuleBrokerService service;
        exitCode = service.Run();
    }
    else
    {
        if (!ServiceBase::CmdlineAction(lpCmdLine, UniversalModuleBrokerService::UMHTraits, exitCode))
        {
            if (::AllocConsole())
            {
                ::SetConsoleTitleW(L"UniversalModuleBroker Debug Console");
                FILE* unused;
                // Re-initialize the C runtime "FILE" handles
                freopen_s(&unused, "CONIN$", "r", stdin);
                freopen_s(&unused, "CONOUT$", "w", stdout);
                freopen_s(&unused, "CONOUT$", "w", stderr);

                std::ios::sync_with_stdio();

                // Re-run to utilize the newly created console.
                SetDefaultLogger();

                FAIL_FAST_IF_FAILED(g_broker.Init());
            }
            SPDLOG_INFO("UniversalModuleBroker started in console mode");
        }
    }

    return exitCode;
}
