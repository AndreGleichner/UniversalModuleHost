#pragma once
#include <Windows.h>
#include <functional>
#include <string_view>
#include <format>
#include <thread>
#include "guid.h"

namespace ipc
{
namespace KnownSession
{
const DWORD Any {(DWORD)-1};
}

namespace KnownService
{
static const Guid None {GUID_NULL};

static const Guid Broker {L"{92D627A3-6C62-4C5B-8477-484A34ED3B82}"};

// ipc::HostInitMsg
static const Guid HostInit {L"{AA810FBD-B33C-4895-8E82-8814EE849E02}"};
static const Guid ManagedHost {L"{7924FE60-C967-449C-BA5D-2EBAA7D16024}"};

static const Guid ShellExec {L"{BEA684E7-697F-4201-844F-98224FA16D2F}"};
static const Guid ConfStore {L"{8583CDC9-DB92-45BE-90CE-4D3AA4CD14F5}"};
}

struct Target final
{
    Guid  Service = KnownService::None;
    DWORD Session = KnownSession::Any;

    Target() = default;

    // You may pass ::WTSGetActiveConsoleSessionId()
    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-wtsgetactiveconsolesessionid
    Target(const Guid& service, DWORD session = KnownSession::Any)
    {
        Service = service;
        Session = session;
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

// Usually be used with host
HRESULT Send(const std::string_view msg, const Target& target) noexcept;

// Usually be used with broker
HRESULT Send(HANDLE out, const std::string_view msg, const Target& target) noexcept;

HRESULT SendDiagMsg(const std::string_view msg) noexcept;

// Usually be used with host
HRESULT StartRead(
    std::thread& reader, std::function<void(const std::string_view msg, const Target& target)> onRead) noexcept;

// Usually be used with broker
HRESULT StartRead(HANDLE in, std::thread& reader,
    std::function<void(const std::string_view msg, const Target& target)> onRead, DWORD pid) noexcept;

// Message sender passed to InitModule() in a module DLL so that it may send messages to its host.
typedef HRESULT(CALLBACK* SendMsg)(void* mod, PCSTR msg, const Guid* service, DWORD session);
// Diagnostic output a spdlog logger within a module will use
typedef HRESULT(CALLBACK* SendDiag)(void* mod, PCSTR msg);
}
