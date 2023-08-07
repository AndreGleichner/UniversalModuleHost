#include "pch.h"

#include "SpdlogCustomFormatter.h"
#include "TMBrokerService.h"
#include "Orchestrator.h"
#include "Permission.h"
#include "env.h"

#pragma region                  Logging
std::shared_ptr<spdlog::logger> g_loggerStdErr;

namespace
{
void SetDefaultLogger()
{
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting

    auto file = env::PrivateDataDir(L"diag") / L"diag.log";

    {
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<ThreadnameFlagFormatter>('t').set_pattern(
            ::IsDebuggerPresent() ? "[%l] %-64v [%t][%! @ %s:%#]" : "[%l] %-64v [TM-%t][%! @ %s:%#]");
        msvc_sink->set_formatter(std::move(formatter));

        std::vector<spdlog::sink_ptr> sinks {msvc_sink};

        if (::GetStdHandle(STD_ERROR_HANDLE))
        {
            auto err_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
                "[%T.%e] [%^%l%$] %-64v [%P/%t][%! @ %s:%#]");
            err_sink->set_formatter(std::move(formatter));

            sinks.push_back(err_sink);
        }

        {
            auto daily_file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(file, 23, 59);

            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
                "[%Y-%m-%d %T.%e %z] [%l] %-64v [%P/%t][%! @ %s:%#]");
            daily_file_sink->set_formatter(std::move(formatter));

            sinks.push_back(daily_file_sink);
        }

        auto logger = std::make_shared<spdlog::logger>("umb", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
    }

    // clone for stderr from host
    {
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->set_pattern("STDERR [%l] %-64v");
        msvc_sink->set_formatter(std::move(formatter));

        std::vector<spdlog::sink_ptr> sinks {msvc_sink};

        if (::GetStdHandle(STD_ERROR_HANDLE))
        {
            auto err_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->set_pattern("[%T.%e] [%^%l%$] %-64v");
            err_sink->set_formatter(std::move(formatter));

            sinks.push_back(err_sink);
        }

        {
            auto daily_file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(file, 23, 59);

            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->set_pattern("[%Y-%m-%d %T.%e %z] [%l] %-64v");
            daily_file_sink->set_formatter(std::move(formatter));

            sinks.push_back(daily_file_sink);
        }

        g_loggerStdErr = std::make_shared<spdlog::logger>("umb", sinks.begin(), sinks.end());
        g_loggerStdErr->set_level(spdlog::level::trace);
    }
}
}
#pragma endregion

Orchestrator g_orchestrator;

// This typically gets called very late.
BOOL WINAPI ConsoleCtrlHandler(_In_ DWORD dwCtrlType)
{
    try
    {
        g_orchestrator.ShuttingDown();
        SPDLOG_INFO(L"ConsoleCtrlHandler: {}", dwCtrlType);
        spdlog::default_logger_raw()->flush();
    }
    catch (...)
    {
    }
    return TRUE;
}

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
#ifndef DEBUG
    // ensure our image dir is only admin writeable
    FAIL_FAST_IF(!Permission::IsDirectoryOnlyWriteableByElevatedUser(Process::ImagePath().parent_path()));
#endif

    Process::SetThreadName(L"TMB-Native");

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
    SPDLOG_INFO(L"Starting TheModularian '{}'", lpCmdLine);
    auto logExit = wil::scope_exit([&] { SPDLOG_INFO("Exiting TheModularian: {}", exitCode); });

    if (Process::IsWindowsService())
    {
        TMBrokerService service;
        exitCode = service.Run();
    }
    else
    {
        bool wasAnAction = ServiceBase::CmdlineAction(lpCmdLine, TMBrokerService::UMHTraits, exitCode);

#ifdef DEBUG
        if (!wasAnAction)
        {
            FAIL_FAST_IF_MSG(wcslen(lpCmdLine) != 0, "Unknown commandline");
            FAIL_FAST_IF_WIN32_BOOL_FALSE(::AllocConsole());

            ::SetConsoleTitleW(L"TheModularian Debug Console");
            ::SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

            FILE* unused;
            // Re-initialize the C runtime "FILE" handles
            freopen_s(&unused, "CONIN$", "r", stdin);
            freopen_s(&unused, "CONOUT$", "w", stdout);
            freopen_s(&unused, "CONOUT$", "w", stderr);

            std::ios::sync_with_stdio();

            // Re-run to utilize the newly created console.
            SetDefaultLogger();

            FAIL_FAST_IF_FAILED(g_orchestrator.Init());

            SPDLOG_INFO("TheModularian started in console mode");

            (void)getc(stdin);

            ::GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);

            // Maybe not call Release and depend on Ctrl-C propagation to child processes
            // g_orchestrator.Release();
        }
#else
        if (!wasAnAction)
            exitCode = 1;
#endif
    }

    return exitCode;
}
