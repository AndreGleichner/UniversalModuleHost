#include "stdafx.h"

#include "UniversalModuleHostService.h"
#include "UniversalModuleHost.h"

HRESULT UniversalModuleHostService::OnStart(DWORD dwArgc, LPTSTR* lpszArgv)
{
    UNREFERENCED_PARAMETER(dwArgc);
    UNREFERENCED_PARAMETER(lpszArgv);

    if (!ModuleHostApp::StartAsync())
        return E_FAIL;

    return S_OK;
}

void UniversalModuleHostService::OnStop()
{
}

void UniversalModuleHostService::OnSessionChange(DWORD dwEventType, DWORD dwSessionId)
{
    SPDLOG_INFO(L"{} {}", dwEventType, dwSessionId);

    if (dwEventType == WTS_SESSION_LOGON)
    {
    }
}
