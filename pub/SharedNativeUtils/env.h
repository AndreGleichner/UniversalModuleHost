#pragma once
#include <filesystem>

#include <wil/win32_helpers.h>

namespace env
{
inline std::filesystem::path PrivateDataDir(const std::wstring_view subDir = L"")
{
    auto                  programData = wil::GetEnvironmentVariableW(L"ProgramData");
    std::filesystem::path dir(programData.get());
    auto                  pdd = dir / L"UniversalModuleHost";

    if (!subDir.empty())
        pdd /= subDir;

    std::filesystem::create_directories(pdd);

    return pdd;
}
}
