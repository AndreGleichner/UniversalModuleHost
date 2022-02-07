#include "pch.h"

#include "UniversalModuleBrokerService.h"
#include "BrokerInstance.h"

extern BrokerInstance g_broker;

HRESULT UniversalModuleBrokerService::OnStart(DWORD dwArgc, LPTSTR* lpszArgv)
{
    UNREFERENCED_PARAMETER(dwArgc);
    UNREFERENCED_PARAMETER(lpszArgv);

    FAIL_FAST_IF_FAILED(g_broker.Init());

    return S_OK;
}

void UniversalModuleBrokerService::OnStop()
{
    g_broker.ShuttingDown();
    g_broker.Release();
}

void UniversalModuleBrokerService::OnSessionChange(DWORD dwEventType, DWORD dwSessionId)
{
    SPDLOG_INFO(L"{} {}", dwEventType, dwSessionId);

    g_broker.OnSessionChange(dwEventType, dwSessionId);
}
