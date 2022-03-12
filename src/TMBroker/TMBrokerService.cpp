#include "pch.h"

#include "TMBrokerService.h"
#include "Orchestrator.h"

extern Orchestrator g_orchestrator;

HRESULT TMBrokerService::OnStart(DWORD dwArgc, LPTSTR* lpszArgv)
{
    UNREFERENCED_PARAMETER(dwArgc);
    UNREFERENCED_PARAMETER(lpszArgv);

    FAIL_FAST_IF_FAILED(g_orchestrator.Init());

    return S_OK;
}

void TMBrokerService::OnStop()
{
    g_orchestrator.ShuttingDown();
    g_orchestrator.Release();
}

void TMBrokerService::OnSessionChange(DWORD dwEventType, DWORD dwSessionId)
{
    SPDLOG_INFO(L"{} {}", dwEventType, dwSessionId);

    g_orchestrator.OnSessionChange(dwEventType, dwSessionId);
}
