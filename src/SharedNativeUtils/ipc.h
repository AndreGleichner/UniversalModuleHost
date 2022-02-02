#pragma once
#include <Windows.h>
#include <functional>
#include <string_view>
#include "guid.h"

namespace ipc
{
namespace KnownSession
{
const DWORD Any {(DWORD)-1};

// Will use WTSGetActiveConsoleSessionId() to determine the target session id.
// Returns KnownSession::Any (-1) if there's no active session.
// https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-wtsgetactiveconsolesessionid
const DWORD Active {(DWORD)-2};
}

namespace KnownService
{
static const Guid Any {GUID_NULL};
static const Guid WebBrowser {L"{BEA684E7-697F-4201-844F-98224FA16D2F}"};

// {BEA684E7-697F-4201-844F-98224FA16D2F}
static const GUID xxx = {0xbea684e7, 0x697f, 0x4201, {0x84, 0x4f, 0x98, 0x22, 0x4f, 0xa1, 0x6d, 0x2f}};

}

struct Target final
{
    DWORD Session = KnownSession::Any;
    Guid  Id      = KnownService::Any;

    Target() = default;

    Target(DWORD session, const Guid& id = KnownService::Any)
    {
        Session = session;
        Id      = id;
    }
    Target(const Guid& id, DWORD session = KnownSession::Any)
    {
        Session = session;
        Id      = id;
    }
};

namespace KnownTarget
{
static const Target World {KnownSession::Any, KnownService::Any};
}

HRESULT Send(const std::string& msg, const Target& target = KnownTarget::World) noexcept;
HRESULT StartRead(std::function<void(const std::string_view msg, const Target& target)> onRead) noexcept;

}
