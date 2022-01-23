#include "stdafx.h"
#include <shellapi.h>

#include "UniversalModuleHost.h"
#include "ManagedHost.h"


#ifdef _WIN32
#    include "Process.h"
#    include "UniversalModuleHostService.h"

#    pragma region StandardHandleBinding
namespace
{
enum class BindCrtHandleType : std::uint32_t
{
    None = 0,

    In  = 1,
    Out = 2,
    Err = 4,

    All = In | Out | Err
};

// https://stackoverflow.com/questions/311955/redirecting-cout-to-a-console-in-windows
void BindCrtHandlesToStdHandles(BindCrtHandleType bindCrtHandleType)
{
    // Re-initialize the C runtime "FILE" handles with clean handles bound to "nul". We do this because it has been
    // observed that the file number of our standard handle file objects can be assigned internally to a value of -2
    // when not bound to a valid target, which represents some kind of unknown internal invalid state. In this state our
    // call to "_dup2" fails, as it specifically tests to ensure that the target file number isn't equal to this value
    // before allowing the operation to continue. We can resolve this issue by first "re-opening" the target files to
    // use the "nul" device, which will place them into a valid state, after which we can redirect them to our target
    // using the "_dup2" function.

    bool bindStdIn  = AnyBitSet(bindCrtHandleType, BindCrtHandleType::In);
    bool bindStdOut = AnyBitSet(bindCrtHandleType, BindCrtHandleType::Out);
    bool bindStdErr = AnyBitSet(bindCrtHandleType, BindCrtHandleType::Err);

    if (bindStdIn)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "r", stdin);
    }
    if (bindStdOut)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "w", stdout);
    }
    if (bindStdErr)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "w", stderr);
    }

    // Redirect unbuffered stdin from the current standard input handle
    if (bindStdIn)
    {
        HANDLE stdHandle = GetStdHandle(STD_INPUT_HANDLE);
        if (stdHandle != INVALID_HANDLE_VALUE)
        {
            int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
            if (fileDescriptor != -1)
            {
                FILE* file = _fdopen(fileDescriptor, "r");
                if (file != NULL)
                {
                    int dup2Result = _dup2(_fileno(file), _fileno(stdin));
                    if (dup2Result == 0)
                    {
                        setvbuf(stdin, NULL, _IONBF, 0);
                    }
                }
            }
        }
    }

    // Redirect unbuffered stdout to the current standard output handle
    if (bindStdOut)
    {
        HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdHandle != INVALID_HANDLE_VALUE)
        {
            int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
            if (fileDescriptor != -1)
            {
                FILE* file = _fdopen(fileDescriptor, "w");
                if (file != NULL)
                {
                    int dup2Result = _dup2(_fileno(file), _fileno(stdout));
                    if (dup2Result == 0)
                    {
                        setvbuf(stdout, NULL, _IONBF, 0);
                    }
                }
            }
        }
    }

    // Redirect unbuffered stderr to the current standard error handle
    if (bindStdErr)
    {
        HANDLE stdHandle = GetStdHandle(STD_ERROR_HANDLE);
        if (stdHandle != INVALID_HANDLE_VALUE)
        {
            int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
            if (fileDescriptor != -1)
            {
                FILE* file = _fdopen(fileDescriptor, "w");
                if (file != NULL)
                {
                    int dup2Result = _dup2(_fileno(file), _fileno(stderr));
                    if (dup2Result == 0)
                    {
                        setvbuf(stderr, NULL, _IONBF, 0);
                    }
                }
            }
        }
    }

    // Clear the error state for each of the C++ standard stream objects. We need to do this, as attempts to access the
    // standard streams before they refer to a valid target will cause the iostream objects to enter an error state. In
    // versions of Visual Studio after 2005, this seems to always occur during startup regardless of whether anything
    // has been read from or written to the targets or not.
    if (bindStdIn)
    {
        std::wcin.clear();
        std::cin.clear();
    }
    if (bindStdOut)
    {
        std::wcout.clear();
        std::cout.clear();
    }
    if (bindStdErr)
    {
        std::wcerr.clear();
        std::cerr.clear();
    }
}
}
#    pragma endregion
#endif

namespace ModuleHostApp
{
bool StartAsync()
{
    return TheManagedHost.RunAsync();
}
}


#pragma region Logging
#include "spdlog/pattern_formatter.h"
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
        ::IsDebuggerPresent() ? "[n%l] %-64v [%t][%! @ %s:%#]" : "[n%l] %-64v [UMH-%t][%! @ %s:%#]");
    msvc_sink->set_formatter(std::move(formatter));

    std::vector<spdlog::sink_ptr> sinks{msvc_sink};

    if (::GetStdHandle(STD_OUTPUT_HANDLE))
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<ThreadnameFlagFormatter>('t').add_flag<ProcessnameFlagFormatter>('P').set_pattern(
            "[%Y-%m-%d %T.%e %z] [n%^%l%$] %-64v [%P/%t][%! @ %s:%#]");
        console_sink->set_formatter(std::move(formatter));

        sinks.push_back(console_sink);
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

    // When our app is using a per app installed .Net we may want to prevent using any global :net installation parts.
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
    SPDLOG_INFO("Starting UniversalModuleHost");
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
            ::AllocConsole();
            ::SetConsoleTitleW(L"UniversalModuleHost Debug Console");
            // Update the C/C++ runtime standard input, output, and error targets to use the just created console window
            BindCrtHandlesToStdHandles(BindCrtHandleType::All);

            // Re-run to utilize the newly created console.
            SetDefaultLogger();

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
