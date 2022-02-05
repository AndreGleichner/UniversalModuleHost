#pragma once
#include <string>
#include <string_view>

namespace Strings
{
std::wstring ToUtf16(std::string_view s);
std::string  ToUtf8(std::wstring_view w);
std::string  ToUtf8(const wchar_t* w);
std::string  ToUtf8(const wchar_t* w, size_t s);
}
