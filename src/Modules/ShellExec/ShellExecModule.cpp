#include "pch.h"
#include <format>

#include <wil/result.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "string_extensions.h"
using namespace Strings;

#include "ShellExecModule.h"

HRESULT ShellExecModule::OnMessage(std::string_view msg, const ipc::Target& target) noexcept
try
{
    if (target.Equals(ipc::Target(ipc::KnownService::ShellExec)))
    {
        // Instead of directly calling ShellExecute() we have to execute this from within a process not being part of
        // the "kill on job close" job object. Otherwise stopping the broker would kill all its child processes
        // including a freshly started browser (e.g. Chrome). If the browser at the time of calling ShellExecute() is
        // already running everything is ok, as in that case the browser porcesses wont become part of the "kill on job
        // close" job object. If the only thing we wanna do is launching a web site we can run: CreateProcess("cmd /c
        // start https://www.heise.de")

        wil::unique_process_information processInfo;
        STARTUPINFO                     startInfo {sizeof(startInfo)};

        const DWORD creationFlags =
            CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS | CREATE_DEFAULT_ERROR_MODE | CREATE_BREAKAWAY_FROM_JOB;

        std::wstring cmdline = std::format(L"cmd /c start {}", ToUtf16(msg));

        RETURN_IF_WIN32_BOOL_FALSE(::CreateProcessW(nullptr, // applicationName
            const_cast<PWSTR>(cmdline.data()),               // commandLine shall be non-const
            nullptr,                                         // process security attributes
            nullptr,                                         // primary thread security attributes
            FALSE,                                           // handles are inherited
            creationFlags,                                   // creation flags
            nullptr,                                         // use parent's environment
            nullptr,                                         // use parent's current directory
            (LPSTARTUPINFOW)&startInfo,                      // STARTUPINFO pointer
            &processInfo));                                  // receives PROCESS_INFORMATION
    }
    return S_OK;
}
CATCH_RETURN()
