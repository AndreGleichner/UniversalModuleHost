#include "stdafx.h"
#include <shellapi.h>

#include "UniversalModuleHost.h"
#include "ManagedHost.h"

#ifdef _WIN32
#    include "Process.h"
#    include "UniversalModuleHostService.h"
#endif

namespace ModuleHostApp
{
bool StartAsync()
{
    return TheManagedHost.RunAsync();
}
}

#pragma region Logging
namespace
{
class ThreadnameFlagFormatter : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override
    {
        std::wstring threadDetails;
        auto         threadname = Process::ThreadName();
        if (threadname.empty())
            threadDetails = std::format(L"{}", ::GetCurrentThreadId());
        else
            threadDetails = std::format(L"{}='{}'", ::GetCurrentThreadId(), threadname);
        spdlog::memory_buf_t buf;
        spdlog::details::os::wstr_to_utf8buf(threadDetails, buf);
        dest.append(buf);
    }

    std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return spdlog::details::make_unique<ThreadnameFlagFormatter>();
    }
};
class ProcessnameFlagFormatter : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override
    {
        spdlog::memory_buf_t buf;
        spdlog::details::os::wstr_to_utf8buf(
            std::format(L"{}='{}'", ::GetProcessId(::GetCurrentProcess()), Process::Name()), buf);
        dest.append(buf);
    }

    std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return spdlog::details::make_unique<ProcessnameFlagFormatter>();
    }
};

void SetDefaultLogger()
{
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting

    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ThreadnameFlagFormatter>('t').set_pattern(
        ::IsDebuggerPresent() ? "[%l] %-64v [%t][%! @ %s:%#]" : "[%l] %-64v [UMH-%t][%! @ %s:%#]");
    msvc_sink->set_formatter(std::move(formatter));

    std::vector<spdlog::sink_ptr> sinks{msvc_sink};

    if (::GetStdHandle(STD_OUTPUT_HANDLE))
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
            "[%T.%e] [%^%l%$] %-64v [%P/%t][%! @ %s:%#]");
        console_sink->set_formatter(std::move(formatter));

        sinks.push_back(console_sink);
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

namespace
{
// Remove/Add certain environment variables.
//
// E.g. prevent dll injections like:
// DOTNET_STARTUP_HOOKS
//      https://github.com/dotnet/runtime/blob/main/docs/design/features/host-startup-hook.md
//      Like used by https://www.ibm.com/docs/en/obi/current?topic=technologies-monitoring-net-core
//
// CORECLR_PROFILER_PATH
//      https://github.com/dotnet/runtime/blob/main/docs/design/coreclr/profiling/Profiler%20Loading.md
//      https://docs.microsoft.com/en-us/dotnet/core/run-time-config/debugging-profiling
//
// List of documented .Net env vars:
//      https://docs.microsoft.com/en-us/dotnet/core/tools/dotnet-environment-variables
//
void FilterEnvVars()
{
    wil::unique_environstrings_ptr env{::GetEnvironmentStringsW()};

    const wchar_t* nextVar = env.get();
    while (nextVar && *nextVar)
    {
        wchar_t* var = _wcsdup(nextVar);
        FAIL_FAST_IF_NULL(var);

        wchar_t* sep = wcschr(var, L'=');
        if (sep)
        {
            *sep = L'\0';
            _wcsupr_s(var, wcslen(var) + 1);

            // is it starting with any suspect, then remove it
            if (var == wcsstr(var, L"DOTNET_") || var == wcsstr(var, L"CORECLR_") || var == wcsstr(var, L"COREHOST_") ||
                var == wcsstr(var, L"COMPLUS_"))
            {
                // Although var is all uppercase here,
                // env vars are case independent and thus can be removed in either casing.
                ::SetEnvironmentVariableW(var, nullptr);
            }
        }

        free(var);
        nextVar += wcslen(nextVar) + 1;
    }

    ::SetEnvironmentVariableW(L"DOTNET_NOLOGO", L"1");
    ::SetEnvironmentVariableW(L"DOTNET_CLI_TELEMETRY_OPTOUT", L"1");
    ::SetEnvironmentVariableW(L"DOTNET_SKIP_FIRST_TIME_EXPERIENCE", L"1");

    // When our app is using a per app installed .Net we may want to prevent using any global .Net installation parts.
    // Unfortunately .Net servicing isn't trivial to disable as this requires tweaking the .Net deps file.
    // https://github.com/dotnet/core-setup/blob/master/Documentation/design-docs/multilevel-sharedfx-lookup.md
    ::SetEnvironmentVariableW(L"DOTNET_MULTILEVEL_LOOKUP", L"0");

    ::SetEnvironmentVariableW(L"DOTNET_CLI_UI_LANGUAGE", L"en-us");

    // For GUI-enabled generated executables - disables dialog popup,
    // which normally shows for certain classes of errors.
    // It only writes to stderr and exits in those cases.
    ::SetEnvironmentVariableW(L"DOTNET_DISABLE_GUI_ERRORS", L"1");
}
}

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    FilterEnvVars();

    Process::SetThreadName(L"UMH-Native");

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
    SPDLOG_INFO(L"Starting UniversalModuleHost '{}'", lpCmdLine);
    auto logExit = wil::scope_exit([&] { SPDLOG_INFO("Exiting UniversalModuleHost: {}", exitCode); });

#ifdef _WIN32
    if (Process::IsWindowsService())
    {
        UniversalModuleHostService service(lpCmdLine);
        exitCode = service.Run();
    }
    else
    {
        if (!ServiceBase::CmdlineAction(lpCmdLine, UniversalModuleHostService::UMHTraits, exitCode))
        {
            if (::AllocConsole())
            {
                ::SetConsoleTitleW(L"UniversalModuleHost Debug Console");
                FILE* unused;
                // Re-initialize the C runtime "FILE" handles
                freopen_s(&unused, "CONIN$", "r", stdin);
                freopen_s(&unused, "CONOUT$", "w", stdout);
                freopen_s(&unused, "CONOUT$", "w", stderr);

                std::ios::sync_with_stdio();

                // Re-run to utilize the newly created console.
                SetDefaultLogger();
            }
            SPDLOG_INFO("UniversalModuleHost started in console mode");

            if (ModuleHostApp::StartAsync())
                (void)getc(stdin);
            else
                exitCode = 1;
        }
    }
#else
#endif

    return exitCode;
}
