#include "pch.h"
#include <shellapi.h>
#include <chrono>
using namespace std::chrono_literals;

#include <nlohmann/json.hpp>

#include "SpdlogCustomFormatter.h"

#include "UniversalModuleHost.h"
#include "ManagedHost.h"

#include "UmhProcess.h"
#include "ipc.h"
#include "HostMsg.h"
#include "FileImage.h"
#include "magic_enum_extensions.h"

#pragma comment(lib, "delayimp")

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
    wil::unique_environstrings_ptr env {::GetEnvironmentStringsW()};

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

int main()
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
    SPDLOG_INFO(L"Starting UniversalModuleHost '{}'", ::GetCommandLineW());
    auto logExit = wil::scope_exit([&] { SPDLOG_INFO("Exiting UniversalModuleHost: {}", exitCode); });

    /*while (!::IsDebuggerPresent())
    {
        ::Sleep(1000);
    }
    ::DebugBreak();*/

    UniversalModuleHost host;
    exitCode = host.Run();

    return exitCode;
}

int UniversalModuleHost::Run()
{
    std::thread reader;
    FAIL_FAST_IF_FAILED(
        ipc::StartRead(reader, [&](const std::string_view msg, const ipc::Target& target) { OnMessage(msg, target); }));

    terminate_.wait();

    if (reader.joinable())
        reader.join();

    return 0;
}

void UniversalModuleHost::OnMessage(const std::string_view msg, const ipc::Target& target)
{
    spdlog::info("RX-H: {} for {}", msg, Strings::ToUtf8(target.ToString()));
    /*while (!::IsDebuggerPresent())
    {
        ::Sleep(1000);
    }
    ::DebugBreak();*/

    if (target.Session == ipc::KnownSession::HostInit)
    {
        auto       j    = nlohmann::json::parse(msg);
        const auto init = j.get<ipc::HostInitMsg>();

        FAIL_FAST_IF_MSG(!target_.Equals(ipc::Target()), "Alread processed an init message before");

        target_ = target;
    }
    else if (target == target_)
    {
        auto       j       = nlohmann::json::parse(msg);
        const auto hostMsg = j.get<ipc::HostCmdMsg>();

        switch (hostMsg.Cmd)
        {
            case ipc::HostCmdMsg::Cmd::Terminate:
            {
                terminate_.SetEvent();
                break;
            }

            case ipc::HostCmdMsg::Cmd::CtrlModule:
            {
                auto       ja   = nlohmann::json::parse(hostMsg.Args);
                const auto args = j.get<ipc::HostCtrlModuleArgs>();

                if (args.Cmd == ipc::HostCtrlModuleArgs::Cmd::Load)
                    LoadModule(ToUtf16(args.Module));
                else
                    UnloadModule(ToUtf16(args.Module));
                break;
            }

            default:
            {
                SPDLOG_ERROR("Host received invalid command {}", hostMsg.Cmd);
            }
        }
    }
}

void UniversalModuleHost::OnModOut(PCWSTR mod, PCWSTR message)
{
}

HRESULT UniversalModuleHost::LoadModule(const std::wstring& path) noexcept
try
{
    auto kind = FileImage::GetKind(path.c_str());
    if (kind == FileImage::Kind::Unknown)
        return E_FAIL;

    if (AnyBitSet(kind, FileImage::Kind::Exe))
        return E_FAIL;

#if _WIN64
    if (AnyBitSet(kind, FileImage::Kind::Bitness32))
        return E_FAIL;
#else
    if (AnyBitSet(kind, FileImage::Kind::Bitness64))
        return E_FAIL;
#endif

    if (AnyBitSet(kind, FileImage::Kind::Native))
        return LoadNativeModule(path);

    return LoadManagedModule(path);
}
CATCH_RETURN();

HRESULT UniversalModuleHost::UnloadModule(const std::wstring& path) noexcept
try
{
    return S_OK;
}
CATCH_RETURN();

HRESULT UniversalModuleHost::LoadNativeModule(const std::wstring& path) noexcept
try
{
    return S_OK;
}
CATCH_RETURN();

HRESULT UniversalModuleHost::LoadManagedModule(const std::wstring& path) noexcept
try
{
    if (!managedHost_)
    {
        managedHost_ = std::make_unique<ManagedHost>(this);
        managedHost_->RunAsync();
    }
    RETURN_IF_FAILED(managedHost_->LoadModule(path));
    return S_OK;
}
CATCH_RETURN();
