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

// Used in host initialization when sending HostInitMsg
const DWORD HostInit {(DWORD)-3};
}

namespace KnownService
{
static const Guid Any {GUID_NULL};
static const Guid Broker {L"{92D627A3-6C62-4C5B-8477-484A34ED3B82}"};
static const Guid WebBrowser {L"{BEA684E7-697F-4201-844F-98224FA16D2F}"};
}

struct Target final
{
    DWORD Session = KnownSession::Any;
    Guid  Service = KnownService::Any;

    Target() = default;

    Target(DWORD session, const Guid& service = KnownService::Any)
    {
        Session = session;
        Service = service;
    }
    Target(const Guid& service, DWORD session = KnownSession::Any)
    {
        Session = session;
        Service = service;
    }

    std::wstring ToString() const
    {
        return std::format(L"{} @ {}", Service.ToString(), (int)Session);
    }

    bool Equals(const Target& rhs) const
    {
        return Session == rhs.Session && Service == rhs.Service;
    }
    bool operator==(const Target& rhs) const
    {
        return Equals(rhs);
    }
};

namespace KnownTarget
{
static const Target World {KnownSession::Any, KnownService::Any};
static const Target Broker {KnownSession::Any, KnownService::Broker};
}

// Usually be used with host
HRESULT Send(const std::string& msg, const Target& target = KnownTarget::World) noexcept;

// Usually be used with broker
HRESULT Send(HANDLE out, const std::string& msg, const Target& target = KnownTarget::World) noexcept;

// Usually be used with host
HRESULT StartRead(
    std::thread& reader, std::function<void(const std::string_view msg, const Target& target)> onRead) noexcept;

// Usually be used with broker
HRESULT StartRead(HANDLE in, std::thread& reader,
    std::function<void(const std::string_view msg, const Target& target)> onRead) noexcept;

}
