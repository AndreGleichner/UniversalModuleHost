#include "pch.h"

#include "UniversalModuleBrokerService.h"
#include "Orchestrator.h"

extern Orchestrator g_orchestrator;

HRESULT UniversalModuleBrokerService::OnStart(DWORD dwArgc, LPTSTR* lpszArgv)
{
    UNREFERENCED_PARAMETER(dwArgc);
    UNREFERENCED_PARAMETER(lpszArgv);

    FAIL_FAST_IF_FAILED(g_orchestrator.Init());

    return S_OK;
}

void UniversalModuleBrokerService::OnStop()
{
    g_orchestrator.ShuttingDown();
    g_orchestrator.Release();
}

void UniversalModuleBrokerService::OnSessionChange(DWORD dwEventType, DWORD dwSessionId)
{
    SPDLOG_INFO(L"{} {}", dwEventType, dwSessionId);

    g_orchestrator.OnSessionChange(dwEventType, dwSessionId);
}
