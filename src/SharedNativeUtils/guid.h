#pragma once

#include <string>
#include <string_view>
#include <guiddef.h>
#include <wil/resource.h>
#include <nlohmann/json.hpp>

struct Guid final : GUID
{
    static Guid CreateNew()
    {
        Guid    newGuid;
        HRESULT _ = ::CoCreateGuid(&newGuid);
        return newGuid;
    }

    Guid(bool createNew = false)
    {
        if (createNew)
            (void)::CoCreateGuid(this);
    }

    Guid(const GUID& guid)
    {
        *(GUID*)this = guid;
    }

    Guid(PCWSTR guid)
    {
        FAIL_FAST_IF_FAILED_MSG(Parse(guid), "Failed to parse guid '%ls'", guid);
    }

    // formatted as "{e27bf98a-dfae-41f1-8e92-1319fd2c6424}"
    std::wstring ToString() const
    {
        wil::unique_cotaskmem_string str;
        THROW_IF_FAILED(::StringFromCLSID(*this, &str));
        return str.get();
    }

    HRESULT Parse(const std::wstring_view guid)
    {
        CLSID clsid;
        RETURN_IF_FAILED(::CLSIDFromString(guid.data(), &clsid));
        *this = *(Guid*)&clsid;
        return S_OK;
    }

    bool Equals(const Guid& rhs) const
    {
        return !!::IsEqualGUID(*this, rhs);
    }
    bool operator==(const Guid& rhs) const
    {
        return Equals(rhs);
    }
};
