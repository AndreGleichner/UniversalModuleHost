#pragma once
#include <Windows.h>
#include <string>

inline std::wstring HResultToString(HRESULT hres)
{
    wchar_t szErrorText[256];
    szErrorText[0] = L'\0';

    ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, hres,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), szErrorText, ARRAYSIZE(szErrorText), nullptr);

    return std::wstring(szErrorText);
}

inline std::wstring LastErrorToString()
{
    return HResultToString(::GetLastError());
}
