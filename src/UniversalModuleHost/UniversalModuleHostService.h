#pragma once

#include <string>
#include <atlstr.h>

#include "ServiceBase.h"
#include "MsgID.h"

namespace
{
std::wstring ResString(UINT id)
{
    ATL::CString s;
    FAIL_FAST_IF_WIN32_BOOL_FALSE(s.LoadString(id));
    return std::wstring(s);
}
}

class UniversalModuleHostService : public ServiceBase
{
public:
    static inline ServiceBase::ServiceTraits UMHTraits{L"UniversalModuleHost", L"RPCSS",
        ResString(IDS_SERVICE_DISPLAY_NAME).c_str(), ResString(IDS_SERVICE_COMMENT).c_str(),
        AcceptControls::STOP | AcceptControls::SESSIONCHANGE, EVENT_CAT_ID_GEN, EVENT_MSG_ID_INFO};

    UniversalModuleHostService(PCWSTR commandLine) : ServiceBase(UMHTraits, commandLine)
    {
    }

protected:
    HRESULT OnStart(DWORD dwArgc, LPTSTR* lpszArgv) override;
    void    OnStop() override;
    void    OnSessionChange(DWORD dwEventType, DWORD dwSessionId) override;
};
