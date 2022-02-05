#include "pch.h"
#include <Windows.h>
#include "string_extensions.h"
#include <wil/result.h>

namespace Strings
{
std::wstring ToUtf16(std::string_view s)
{
    std::wstring output;
    if (s.empty())
        return output;

    const int size = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG((BOOL)(size >= 0), "Failed to alloc in Strings::ToUtf16");

    output.resize(static_cast<size_t>(size));
    FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(
        (BOOL)(size == ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), output.data(), size)),
        "Failed to convert to UTF16");

    return output;
}

std::string ToUtf8(std::wstring_view w)
{
    return ToUtf8(w.data(), w.size());
}

std::string ToUtf8(const wchar_t* w)
{
    return ToUtf8(w, wcslen(w));
}

std::string ToUtf8(const wchar_t* w, size_t s)
{
    std::string output;
    if (s != 0)
    {
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(s), nullptr, 0, nullptr, nullptr);
        FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG((BOOL)(size >= 0), "Failed to alloc in Strings::ToUtf8");

        output.resize(static_cast<size_t>(size));
        FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG((BOOL)(size == ::WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(s),
                                                             output.data(), static_cast<int>(size), nullptr, nullptr)),
            "Failed to convert to UTF8");
    }

    return output;
}
}
