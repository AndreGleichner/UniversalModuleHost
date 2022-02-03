#include "pch.h"

#include "SpdlogCustomFormatter.h"
#include "UniversalModuleBrokerService.h"
#include "BrokerInstance.h"
#include "ChildProcess.h"
#include "ipc.h"

#pragma region                  Logging
std::shared_ptr<spdlog::logger> g_loggerStdErr;

namespace
{
void SetDefaultLogger()
{
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting

    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ThreadnameFlagFormatter>('t').set_pattern(
        ::IsDebuggerPresent() ? "[%l] %-64v [%t][%! @ %s:%#]" : "[%l] %-64v [UMH-%t][%! @ %s:%#]");
    msvc_sink->set_formatter(std::move(formatter));

    std::vector<spdlog::sink_ptr> sinks {msvc_sink};

    if (::GetStdHandle(STD_ERROR_HANDLE))
    {
        {
            auto err_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
                "[%T.%e] [%^%l%$] %-64v [%P/%t][%! @ %s:%#]");
            err_sink->set_formatter(std::move(formatter));

            sinks.push_back(err_sink);
        }

        // For stderr from host
        {
            auto err_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
                "[%T.%e] [%^%l%$] %-64v");
            err_sink->set_formatter(std::move(formatter));
            g_loggerStdErr = std::make_shared<spdlog::logger>("umh", err_sink);
            g_loggerStdErr->set_level(spdlog::level::trace);
        }
    }

    {
        const auto file = L"c:\\temp\\native-logfile.txt";

        auto daily_file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(file, 23, 59);

        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
            "[%Y-%m-%d %T.%e %z] [%^%l%$] %-64v [%P/%t][%! @ %s:%#]");
        daily_file_sink->set_formatter(std::move(formatter));

        sinks.push_back(daily_file_sink);
    }

    auto logger = std::make_shared<spdlog::logger>("umh", sinks.begin(), sinks.end());
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

                HANDLE err  = ::GetStdHandle(STD_ERROR_HANDLE);
                DWORD  mode = 0;
                if (::GetConsoleMode(err, &mode))
                {
                    // Enable ANSI escape codes
                    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    ::SetConsoleMode(err, mode);
                }

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

            auto         host = Process::ImagePath().replace_filename(L"UniversalModuleHost64.exe");
            ChildProcess cp;
            cp.Create(host.c_str());



            (void)getc(stdin);

            g_broker.Release();
        }
    }

    return exitCode;
}
