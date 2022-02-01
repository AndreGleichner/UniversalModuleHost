#pragma once

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
void                  Enumerate(std::function<EnumerateCallbackResult(PPROCESSENTRY32W)> callback);
bool                  IsWindowsService();
std::filesystem::path ImagePath();
std::wstring          Name();
void                  SetThreadName(PCWSTR name);
std::wstring          ThreadName();
}
