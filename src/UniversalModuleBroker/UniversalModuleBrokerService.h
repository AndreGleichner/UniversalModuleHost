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

class UniversalModuleBrokerService : public ServiceBase
{
public:
    static inline ServiceBase::ServiceTraits UMHTraits {L"UniversalModuleBroker", L"RPCSS",
        ResString(IDS_SERVICE_DISPLAY_NAME).c_str(), ResString(IDS_SERVICE_COMMENT).c_str(),
        AcceptControls::STOP | AcceptControls::SESSIONCHANGE, EVENT_CAT_ID_GEN, EVENT_MSG_ID_INFO};

    UniversalModuleBrokerService() : ServiceBase(UMHTraits)
    {
    }

protected:
    HRESULT OnStart(DWORD dwArgc, LPTSTR* lpszArgv) override;
    void    OnStop() override;
    void    OnSessionChange(DWORD dwEventType, DWORD dwSessionId) override;
};
