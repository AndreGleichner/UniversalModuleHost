#pragma once

#include <Windows.h>
#include <filesystem>
#include <functional>
#include <TlHelp32.h>

namespace Process
{
enum class EnumerateCallbackResult
{
    Continue,
    Cancel
};

struct ParentProcessInfo
{
    DWORD        Pid;
    DWORD        ParentPid;
    std::wstring ExePath;
};

void                  Enumerate(std::function<EnumerateCallbackResult(PPROCESSENTRY32W)> callback);
bool                  IsWindowsService();
bool                  IsProtectedService();
std::filesystem::path ImagePath(); // Only call in exe but not in a DLL
std::filesystem::path ImagePath(DWORD pid);
std::wstring          Name();
void                  SetThreadName(PCWSTR name);
std::wstring          ThreadName();
ParentProcessInfo     ParentProcess();
}
