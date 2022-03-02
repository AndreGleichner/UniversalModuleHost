#pragma once
#include <Windows.h>
#include <unordered_set>
#include <wil/resource.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include "ipc.h"

class Orchestrator;
struct ChildProcessConfig;

class ChildProcessInstance final
{
    friend Orchestrator;

public:
    ChildProcessInstance(Orchestrator* orchestrator, std::shared_ptr<ChildProcessConfig> childProcessConfig,
        DWORD session = ipc::KnownSession::Any)
        : orchestrator_(orchestrator), childProcessConfig_(childProcessConfig), target_(Guid(true), session)
    {
    }

    HRESULT Launch(bool keepAlive = true) noexcept;
    HRESULT Terminate() noexcept;
    HRESULT LoadModules() noexcept;
    HRESULT UnloadModules() noexcept;

    HRESULT SendMsg(const std::string_view msg, const ipc::Target& target);

private:
    void StartForwardStderr() noexcept;

    bool ShouldBreakAwayFromJob() const;

    Orchestrator*                                orchestrator_;
    std::shared_ptr<ChildProcessConfig>          childProcessConfig_;
    ipc::Target                                  target_;
    wil::unique_process_information              processInfo_;
    wil::unique_handle                           inRead_;
    wil::unique_handle                           inWrite_;
    wil::unique_handle                           outRead_;
    wil::unique_handle                           outWrite_;
    wil::unique_handle                           errRead_;
    wil::unique_handle                           errWrite_;
    std::thread                                  stderrForwarder_;
    std::thread                                  reader_;
    std::thread                                  keepAlive_;
    std::unordered_set<Guid, Guid::HashFunction> services_;
};
