#include "pch.h"
#include <shellapi.h>
#include <chrono>
using namespace std::chrono_literals;

#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include "SpdlogCustomFormatter.h"

#include "UniversalModuleHost.h"
#include "ManagedHost.h"

#include "UmhProcess.h"
#include "ipc.h"
#include "HostMsg.h"
#include "FileImage.h"
#include "magic_enum_extensions.h"
#include "Permission.h"
#include "ModuleBase.h"

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

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
#ifndef DEBUG
    // Ensure we've been launched by the broker.
    auto pp = Process::ParentProcess();
    FAIL_FAST_IF(pp.ExePath != L"UniversalModuleBroker64.exe" && pp.ExePath != L"UniversalModuleBroker32.exe");

    // ensure our image dir is only admin writeable
    FAIL_FAST_IF(!Permission::IsDirectoryOnlyWriteableByElevatedUser(Process::ImagePath().parent_path()));
#endif

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

    // Process incoming messages and wait for termination.
    UniversalModuleHost host;
    exitCode = host.Run();

    return exitCode;
}

int UniversalModuleHost::Run()
{
    std::jthread reader;
    FAIL_FAST_IF_FAILED(ipc::StartRead(reader, [&](const std::string_view msg, const ipc::Target& target) {
        return OnMessageFromBroker(msg, target) == S_FALSE;
    }));

    terminate_.wait();

    // while (!::IsDebuggerPresent())
    //{
    //     ::Sleep(1000);
    // }
    //::DebugBreak();

    reader.request_stop();

    if (reader.joinable())
        reader.join();

    return 0;
}

HRESULT UniversalModuleHost::OnMessageFromBroker(const std::string_view msg, const ipc::Target& target)
try
{
    if (spdlog::should_log(spdlog::level::trace))
    {
        std::string m = msg.data();
        std::erase_if(m, [](char c) { return c == '\r' || c == '\n'; });
        spdlog::trace("RX-H: {} for {}", m, Strings::ToUtf8(target.ToString()));
    }

    /*while (!::IsDebuggerPresent())
    {
        ::Sleep(1000);
    }
    ::DebugBreak();*/

    if (target.Service == ipc::KnownService::HostInit)
    {
        auto       j    = json::parse(msg);
        const auto init = j.get<ipc::HostInitMsg>();

        FAIL_FAST_IF_MSG(!target_.Equals(ipc::Target()), "Alread processed an init message before");

        target_    = ipc::Target(init.Service);
        groupName_ = init.GroupName;
    }
    else
    {
        FAIL_FAST_IF_MSG(target_.Equals(ipc::Target()), "Host not initialized yet");

        if (target.Service == target_.Service)
        {
            auto       j       = json::parse(msg);
            const auto hostMsg = j.get<ipc::HostCmdMsg>();

            switch (hostMsg.Cmd)
            {
                case ipc::HostCmdMsg::Cmd::Terminate:
                {
                    if (managedHost_)
                    {
                        managedHost_->Send(msg, ipc::Target(ipc::KnownService::ManagedHost));
                    }

                    terminate_.SetEvent();
                    return S_FALSE;
                }

                case ipc::HostCmdMsg::Cmd::CtrlModule:
                {
                    auto       ja   = json::parse(hostMsg.Args);
                    const auto args = ja.get<ipc::HostCtrlModuleArgs>();

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
        else
        {
            for (auto& mod : nativeModules_)
            {
                LOG_IF_FAILED(mod->Send(msg, target));
            }

            if (managedHost_)
            {
                managedHost_->Send(msg, target);
            }
        }
    }

    return S_OK;
}
CATCH_RETURN()

HRESULT UniversalModuleHost::LoadModule(const std::wstring& name) noexcept
try
{
    // while (!::IsDebuggerPresent())
    //{
    //     ::Sleep(1000);
    // }
    //::DebugBreak();

    auto path = ModuleBase::PathFor(name, true);

    auto kind = FileImage::GetKind(path.c_str());
    if (kind == FileImage::Kind::Unknown)
    {
        // managed assemblies are typically AnyCPU, so no need to have both 32 and 64bit versions.
        path = ModuleBase::PathFor(name, false);
        kind = FileImage::GetKind(path.c_str());
        if (kind == FileImage::Kind::Unknown)
            return E_FAIL;
    }

    // Managed assembly may be a PE32 image although can be loaded into a 64bit host.
    if (AnyBitSet(kind, FileImage::Kind::Managed))
    {
        if (AnyBitSet(kind, FileImage::Kind::Exe))
            return LoadManagedExeModule(path);

        return LoadManagedDllModule(path);
    }


#if _WIN64
    if (AnyBitSet(kind, FileImage::Kind::Bitness32))
        return E_FAIL;
#else
    if (AnyBitSet(kind, FileImage::Kind::Bitness64))
        return E_FAIL;
#endif

    return LoadNativeModule(path);
}
CATCH_RETURN();

HRESULT UniversalModuleHost::UnloadModule(const std::wstring& name) noexcept
try
{
    // try to find a native module with given name
    auto mod = std::find_if(nativeModules_.begin(), nativeModules_.end(),
        [&](const std::unique_ptr<NativeModule>& m) { return wcscmp(m->path_.stem().c_str(), name.c_str()) == 0; });

    if (mod != nativeModules_.end())
    {
        HRESULT hr = (*mod)->Unload();
        nativeModules_.erase(mod);
        RETURN_HR_MSG(hr, "native module unload failed %ls", name.c_str());
    }

    RETURN_HR_IF_NULL_MSG(E_FAIL, managedHost_, "Managed host not initialized");
    RETURN_IF_FAILED(managedHost_->UnloadModule(name));

    return S_OK;
}
CATCH_RETURN();

HRESULT UniversalModuleHost::LoadNativeModule(const std::filesystem::path& path) noexcept
try
{
    auto mod = std::make_unique<NativeModule>(path);
    RETURN_IF_FAILED(mod->Load());

    nativeModules_.push_back(std::move(mod));

    return S_OK;
}
CATCH_RETURN();

HRESULT UniversalModuleHost::LoadManagedDllModule(const std::filesystem::path& path) noexcept
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

HRESULT UniversalModuleHost::LoadManagedExeModule(const std::filesystem::path& path) noexcept
try
{
    if (managedHost_ != nullptr)
        return S_FALSE;

    managedHost_ = std::make_unique<ManagedHost>(this, path.c_str());
    managedHost_->RunAsync();

    return S_OK;
}
CATCH_RETURN();

HRESULT NativeModule::Load()
{
    hmodule_ = wil::unique_hmodule(::LoadLibraryW(path_.c_str()));
    RETURN_HR_IF_NULL_MSG(E_FAIL, hmodule_.get(), "Failed to load native module %ls", path_.c_str());

#define LoadEntry(fn)                                                                    \
    fn##_ = reinterpret_cast<decltype(Entry::fn)*>(GetProcAddress(hmodule_.get(), #fn)); \
    RETURN_LAST_ERROR_IF_NULL_MSG(fn##_, "Failed to load module entry " #fn)

    LoadEntry(InitModule);
    LoadEntry(TermModule);
    LoadEntry(OnMessage);

    RETURN_IF_FAILED(InitModule_(this, OnMsg, OnDiag));

#undef LoadEntry
    return S_OK;
}

HRESULT NativeModule::Unload()
{
    auto freeLib = wil::scope_exit([&] { hmodule_.reset(); });

    RETURN_IF_FAILED(TermModule_());

    return S_OK;
}

HRESULT NativeModule::Send(const std::string_view msg, const ipc::Target& target) noexcept
{
    return OnMessage_(msg.data(), &target);
}

HRESULT CALLBACK NativeModule::OnMsg(void* mod, PCSTR msg, const Guid* service, DWORD session) noexcept
{
    auto m = static_cast<NativeModule*>(mod);
    RETURN_IF_FAILED(ipc::Send(msg, ipc::Target(*service, session)));
    return S_OK;
}

HRESULT CALLBACK NativeModule::OnDiag(void* mod, PCSTR msg) noexcept
{
    auto m = static_cast<NativeModule*>(mod);
    RETURN_IF_FAILED(ipc::SendDiagMsg(msg));
    return S_OK;
}
