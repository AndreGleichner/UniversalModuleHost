#include "pch.h"

#include "UniversalModuleBrokerService.h"

HRESULT UniversalModuleBrokerService::OnStart(DWORD dwArgc, LPTSTR* lpszArgv)
{
    UNREFERENCED_PARAMETER(dwArgc);
    UNREFERENCED_PARAMETER(lpszArgv);



    return S_OK;
}

void UniversalModuleBrokerService::OnStop()
{
}

void UniversalModuleBrokerService::OnSessionChange(DWORD dwEventType, DWORD dwSessionId)
{
    SPDLOG_INFO(L"{} {}", dwEventType, dwSessionId);

    if (dwEventType == WTS_SESSION_LOGON)
    {
    }
}
