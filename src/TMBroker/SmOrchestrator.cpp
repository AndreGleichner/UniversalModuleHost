#include "pch.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "Orchestrator.h"
#include "ModuleMeta.h"
#include "ConfStore.h"

#pragma region Actions

void SmOrchestrator::AInitialize()
{
    o_->bootstrapping_ = true;
    o_->AssignProcessToJobObject(::GetCurrentProcess(), o_->session_);

    // Bootstrap-config by launching the ConfStore module in some process.
    o_->brokerConfig_ = R"(
{
  "Broker": {
    "ChildProcesses": [
      {
        "GroupName": "ConfStore",
        "Session": 0,
        "Modules": [
          "ConfStore"
        ]
      }
    ]
  }
}
)"_json;
}

void SmOrchestrator::AQueryBrokerConfig()
{
    // Ask the just launched ConfStore to send around the Broker config
    // so we can launch the current set of configured processes/modules.
    json msg = ipc::ConfStore {ipc::ConfStore::Cmd::Query, "Broker"};
    THROW_IF_FAILED(o_->SendToAllChildren(ipc::MsgItem(msg.dump().c_str(), ipc::Topic(ipc::ConfStoreTopic))));
}

void SmOrchestrator::ALaunchMissingProcesses()
{
}

void SmOrchestrator::ALoadModules()
{
}

void SmOrchestrator::AStoreModMetaInProcInstance()
{
}

void SmOrchestrator::ABootstrappingFinished()
{
    o_->bootstrapping_ = false;
}

void SmOrchestrator::AStoreEmptyBrokerConfigForShutdown()
{
}

void SmOrchestrator::AStoreBrokerConfig()
{
}

void SmOrchestrator::ADispatchMsgToHosts()
{
}

void SmOrchestrator::AStartProcTermThread()
{
}

void SmOrchestrator::AException()
{
}
#pragma endregion

#pragma region Guards

bool SmOrchestrator::GIsBootstrapping() const noexcept
{
    return o_->bootstrapping_;
}
bool SmOrchestrator::GIsCurMsgModMetaConfstore() const noexcept
{
    return o_->curMsg_.Topic.TopicId == ipc::ModuleMetaTopic;
}
bool SmOrchestrator::GIsCurMsgBrokerConfig() const noexcept
{
    if (o_->curMsg_.Topic.TopicId == ipc::ConfTopic)
    {
        const json conf = json::parse(o_->curMsg_.Msg);
        return conf.contains("Broker");
    }
    return false;
}
bool SmOrchestrator::GDispCurMsgToHosts() const noexcept
{
    if (o_->curMsg_.Topic.TopicId == ipc::ConfTopic)
    {
        const json conf = json::parse(o_->curMsg_.Msg);
        return !conf.contains("Broker");
    }
    return true;
}
bool SmOrchestrator::GHasAnyChildProcs() const noexcept
{
    return !o_->childProcesses_.empty();
}
bool SmOrchestrator::GHasNoChildProcs() const noexcept
{
    return o_->childProcesses_.empty();
}
#pragma endregion
