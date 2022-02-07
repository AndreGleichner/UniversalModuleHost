#include "pch.h"

#include "SpdlogCustomFormatter.h"
#include "UniversalModuleBrokerService.h"
#include "BrokerInstance.h"

#pragma region                  Logging
std::shared_ptr<spdlog::logger> g_loggerStdErr;

namespace
{
void SetDefaultLogger()
{
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting

    const auto file = L"c:\\temp\\native-logfile.txt";

    {
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<ThreadnameFlagFormatter>('t').set_pattern(
            ::IsDebuggerPresent() ? "[%l] %-64v [%t][%! @ %s:%#]" : "[%l] %-64v [UMH-%t][%! @ %s:%#]");
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

BrokerInstance g_broker;

// This typically gets called very late.
BOOL WINAPI ConsoleCtrlHandler(_In_ DWORD dwCtrlType)
{
    try
    {
        g_broker.ShuttingDown();
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

#pragma region ConfigureJobObject
    // Place the broker and all its child processes in a job, so that
    // if the broker dies the system also terminates any child processes.
    // Allow a child process to explicitly create child processes that don't belong to the job.
    // This may be usefull to create UI processes like default browsers which shouldn't terminate on broker exit.
    wil::unique_handle job(::CreateJobObjectW(nullptr, nullptr));
    FAIL_FAST_IF_MSG(!job, "Failed to create broker job object");

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobOptions;
    ZeroMemory(&jobOptions, sizeof(jobOptions));
    jobOptions.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
    RETURN_IF_WIN32_BOOL_FALSE(
        ::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &jobOptions, sizeof(jobOptions)));

    RETURN_IF_WIN32_BOOL_FALSE(::AssignProcessToJobObject(job.get(), ::GetCurrentProcess()));
#pragma endregion

    if (Process::IsWindowsService())
    {
        UniversalModuleBrokerService service;
        exitCode = service.Run();
    }
    else
    {
        if (!ServiceBase::CmdlineAction(lpCmdLine, UniversalModuleBrokerService::UMHTraits, exitCode))
        {
            FAIL_FAST_IF_WIN32_BOOL_FALSE(::AllocConsole());

            ::SetConsoleTitleW(L"UniversalModuleBroker Debug Console");
            ::SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

            FILE* unused;
            // Re-initialize the C runtime "FILE" handles
            freopen_s(&unused, "CONIN$", "r", stdin);
            freopen_s(&unused, "CONOUT$", "w", stdout);
            freopen_s(&unused, "CONOUT$", "w", stderr);

            std::ios::sync_with_stdio();

            // Re-run to utilize the newly created console.
            SetDefaultLogger();

            FAIL_FAST_IF_FAILED(g_broker.Init());

            SPDLOG_INFO("UniversalModuleBroker started in console mode");

            (void)getc(stdin);

            ::GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);

            // Maybe not call Release and depend on Ctrl-C propagation to child processes
            // g_broker.Release();
        }
    }

    return exitCode;
}
