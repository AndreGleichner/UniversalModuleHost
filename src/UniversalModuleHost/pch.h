#pragma once

#define _WIN32_WINNT _WIN32_WINNT_WIN8

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // einige CString-Konstruktoren sind explizit

#define WIN32_LEAN_AND_MEAN
#define _SECURE_ATL 1
#include <atlbase.h>

#include "UndefWinMacros.h"

#include <atltime.h>
#include <atlsecurity.h>

#define SECURITY_WIN32
#include <Security.h>
#pragma comment(lib, "Secur32.lib")

#include <WtsApi32.h>
#pragma comment(lib, "Wtsapi32.lib")

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <TlHelp32.h>

#include <list>
#include <vector>
#include <deque>
#include <string>
#include <string_view>
#include <sstream>
#include <algorithm>
#include <stack>
#include <set>
#include <map>
#include <queue>
#include <memory>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <math.h>
#include <time.h>
#include <ntsecapi.h>
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <filesystem>
#include <thread>
#include <iostream>
#include <format>
#include <regex>

#pragma warning(push)
#pragma warning(disable : 6001 6031 6387 26451 28196)
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/win32_helpers.h>
#include <wil/filesystem.h>
#pragma warning(pop)

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#define SPDLOG_FMT_EXTERNAL
#define SPDLOG_WCHAR_FILENAMES
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#define SPDLOG_PREVENT_CHILD_FD
#define SPDLOG_LEVEL_NAMES                              \
    {                                                   \
        "TRC", "DBG", "INF", "WRN", "ERR", "CRT", "OFF" \
    }
#define SPDLOG_SHORT_LEVEL_NAMES          \
    {                                     \
        "T", "D", "I", "W", "E", "C", "O" \
    }
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

#include <magic_enum.hpp>

namespace magic_enum::bitwise_operators
{
template <typename T>
using IsEnum = std::enable_if_t<std::is_enum_v<T>>;

template <typename E, typename = IsEnum<E>>
constexpr bool AnyBitSet(E lhs, E rhs) noexcept
{
    return static_cast<underlying_type_t<E>>(0) !=
           (static_cast<underlying_type_t<E>>(lhs) & static_cast<underlying_type_t<E>>(rhs));
}
}
using namespace magic_enum::bitwise_operators;

#include "resource.h"
#include "HResult.h"
