#include "stdafx.h"

#include "ManagedHost.h"
#include "error_codes.h"


#ifdef _WIN32
#    include <Windows.h>

#    define DIR_SEPARATOR L'\\'

#else
#    include <dlfcn.h>
#    include <limits.h>

#    define DIR_SEPARATOR '/'
#    define MAX_PATH PATH_MAX

#endif

#include "Process.h"

using string_t = std::basic_string<char_t>;

ManagedHost TheManagedHost;

ManagedHost::ManagedHost()
{
}

ManagedHost::~ManagedHost()
{
    if (hostContext_)
    {
        closeHostContext_(hostContext_);
        hostContext_ = nullptr;
    }
    if (mainThread_.joinable())
        mainThread_.join();
}

// https://docs.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting
// https://github.com/dotnet/runtime/blob/master/docs/design/features/native-hosting.md
bool ManagedHost::RunAsync()
{
    SPDLOG_INFO(L"-> ManagedHost::RunAsync");

    if (!LoadFxr())
    {
        SPDLOG_INFO(L"Failed to load hostfxr");
        return false;
    }

    // Get the current executable's directory.
    // This assumes the managed assembly to load and its runtime configuration file are next to the host.
    const auto imageDir = Process::ImagePath().parent_path();
    assemblyPath_       = imageDir / _X("ManagedHost.dll");

#if INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_CMDLINE
    const char_t* argv[]{assemblyPath_.c_str(), _X("arg1"), _X("-sw0 val0")};
    if (!InitHostContext((int)_countof(argv), argv))
        return false;
#elif INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_RUNTIMECONFIG

    if (!InitHostContext(imageDir / _X("ManagedHost.runtimeconfig.json")))
        return false;
#endif
    if (!InitFunctionPointerFactory())
        return false;

#if INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_RUNTIMECONFIG
    struct MainArgs
    {
        void*         ThisHost;
        const char_t* Commandline;
    };

    using mainFuncSig    = std::add_pointer_t<int CORECLR_DELEGATE_CALLTYPE(MainArgs args)>;
    mainFuncSig mainFunc = (mainFuncSig)CreateFunction(_X("NativeHostMain"));
    if (!mainFunc)
    {
        return false;
    }
#endif

    // Launch managed in its own thread, since we may run this in a service.
    mainThread_ = std::thread([&] {
        Process::SetThreadName(L"UMH-NetRunner");

        SPDLOG_INFO(L"Invoke managed Main");

#if INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_CMDLINE
        // returns only when the managed app finished
        int res = runApp_(hostContext_);
#elif INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_RUNTIMECONFIG
        MainArgs args{this, _X("arg1 -sw0 val0")};
        int      res = mainFunc(args);
#endif

        SPDLOG_INFO(L"Managed Main returned: {}", res);
    });

    // w/o this tiny sleep the final getc(stdin) causes a crash for unknown reason.
    // It wont happen if calling Main in this thread.
    Sleep(100);

    SPDLOG_INFO(L"<- ManagedHost::RunAsync");

    return true;
}

void HOSTFXR_CALLTYPE ManagedHost::ErrorWriter(const char_t* message)
{
    SPDLOG_ERROR(L".Net error: {}", message);
}

// Using the nethost library, discover the location of hostfxr and get exports
bool ManagedHost::LoadFxr()
{
    // get_hostfxr_parameters params{sizeof(get_hostfxr_parameters), nullptr, LR"(c:\umh\dotnet\x64\6.0.0)"};

    char_t hostfxrPath[MAX_PATH];
    size_t pathSize = sizeof(hostfxrPath) / sizeof(char_t);
    int    rc       = get_hostfxr_path(hostfxrPath, &pathSize, nullptr /*&params*/);
    if (!STATUS_CODE_SUCCEEDED(rc))
        return false;

    if (spdlog::should_log(spdlog::level::info))
    {
        SetEnvironmentVariableW(L"COREHOST_TRACE", L"1");
        SetEnvironmentVariableW(L"COREHOST_TRACE_VERBOSITY", L"4");

        const auto imageDir  = Process::ImagePath().parent_path();
        auto       traceFile = imageDir / _X("corehost.log");
        SetEnvironmentVariableW(L"COREHOST_TRACEFILE", traceFile.c_str());
    }

    // Load hostfxr and get desired exports
    void* lib = LoadLib(hostfxrPath);

#define GETEXPORT(var, name)                \
    var = (name##_fn)GetExport(lib, #name); \
    if (!var)                               \
        return false;

    // clang-format off
#if INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_CMDLINE
    GETEXPORT(initFromCmdline_,             hostfxr_initialize_for_dotnet_command_line);
    GETEXPORT(runApp_,                      hostfxr_run_app);
#elif INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_RUNTIMECONFIG
    GETEXPORT(initFromRuntimeconfig_,       hostfxr_initialize_for_runtime_config);
#endif
    GETEXPORT(getFunctionPointerFactory_,   hostfxr_get_runtime_delegate);
    GETEXPORT(closeHostContext_,            hostfxr_close);
    GETEXPORT(getRuntimeProperty_,          hostfxr_get_runtime_property_value);
    GETEXPORT(setRuntimeProperty_,          hostfxr_set_runtime_property_value);
    GETEXPORT(getAllRuntimeProperties_,     hostfxr_get_runtime_properties);
    GETEXPORT(setErrorWriter_,              hostfxr_set_error_writer);
    // clang-format on

#undef GETEXPORT

    setErrorWriter_(ErrorWriter);

    return true;
}

extern "C" __declspec(dllexport) int OnProgressFromManaged(void* thisHost, int progress)
{
    SPDLOG_INFO(L"OnProgressFromManaged {}", thisHost);

    auto host = (ManagedHost*)thisHost;
    return TheManagedHost.OnProgress(progress);
}

int ManagedHost::OnProgress(int progress) const
{
    SPDLOG_INFO(L"Progress {}", progress);
    return progress * 10;
}

void* ManagedHost::CreateFunction(const char_t* name) const
{
    void* func = nullptr;
#if INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_CMDLINE
    int rc = functionFactory_(dotnetType_, name, UNMANAGEDCALLERSONLY_METHOD, hostContext_, nullptr, (void**)&func);
#elif INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_RUNTIMECONFIG
    int rc =
        functionFactory_(assemblyPath_.c_str(), dotnetType_, name, UNMANAGEDCALLERSONLY_METHOD, nullptr, (void**)&func);
#endif
    if (!STATUS_CODE_SUCCEEDED(rc) || !func)
    {
        SPDLOG_INFO(L"Failure: functionFactory_(NativeHostMain)");
        return nullptr;
    }
    return func;
}

void* ManagedHost::LoadLib(const char_t* path) const
{
#ifdef _WIN32
    HMODULE h = ::LoadLibraryW(path);
    return (void*)h;
#else
    void* h = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    return h;
#endif
}

void* ManagedHost::GetExport(void* h, const char* name) const
{
#ifdef _WIN32
    void* f = ::GetProcAddress((HMODULE)h, name);
    return f;
#else
    void* f = dlsym(h, name);
    return f;
#endif
}

#if INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_CMDLINE
bool ManagedHost::InitHostContext(int argc, const char_t** argv)
{
    int rc = initFromCmdline_(argc, argv, nullptr, &hostContext_);
    if (!STATUS_CODE_SUCCEEDED(rc) || hostContext_ == nullptr)
    {
        SPDLOG_INFO(L"initFromCmdline_ failed: {:#x}", rc);
        return false;
    }

#    if 0
    const size_t  count    = 10;
    size_t        countVar = count;
    const char_t* keys[count];
    const char_t* values[count];
    rc = getAllRuntimeProperties_(hostContext_, &countVar, (const char_t**)&keys, (const char_t**)&values);
    if (STATUS_CODE_SUCCEEDED(rc))
    {
        for (size_t n = 0; n < countVar; ++n)
        {
            SPDLOG_INFO(L"RT-Prop: {}: {}", keys[n], values[n]);
        }
    }
#    endif
    return true;
}

bool ManagedHost::InitFunctionPointerFactory()
{
    int rc = getFunctionPointerFactory_(hostContext_, hdt_get_function_pointer, (void**)&functionFactory_);
    if (!STATUS_CODE_SUCCEEDED(rc) || functionFactory_ == nullptr)
    {
        SPDLOG_INFO(L"getFunctionPointerFactory_ failed: {:#x}", rc);
        return false;
    }
    return true;
}
#elif INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_RUNTIMECONFIG
bool ManagedHost::InitHostContext(std::filesystem::path runtimeconfigPath)
{
    // This only resolves the framework from given config but doesn't actually load the runtime.
    int rc = initFromRuntimeconfig_(runtimeconfigPath.c_str(), nullptr, &hostContext_);
    if (!STATUS_CODE_SUCCEEDED(rc) || hostContext_ == nullptr)
    {
        SPDLOG_INFO(L"initFromRuntimeconfig_ failed: {:#x}", rc);

        return false;
    }
    return true;
}

// Load and initialize .NET Core and get desired function pointer for scenario
bool ManagedHost::InitFunctionPointerFactory()
{
    // Load the .Net runtime.
    // Get the factory through which we can load the runtime (if not done before)
    // and retrieve native function pointers for managed public static functions.
    int rc =
        getFunctionPointerFactory_(hostContext_, hdt_load_assembly_and_get_function_pointer, (void**)&functionFactory_);
    if (!STATUS_CODE_SUCCEEDED(rc) || functionFactory_ == nullptr)
    {
        SPDLOG_INFO(L"getFunctionPointerFactory_ failed: {:#x}", rc);
        return false;
    }
    return true;
}
#endif
