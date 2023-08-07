#pragma once
#include <Windows.h>
#include <unordered_set>
#include <wil/resource.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include "ipc.h"

#include <boost/sml.hpp>
namespace sml = boost::sml;

class Orchestrator;
struct ChildProcessConfig;

class ChildProcessInstance final
{
    using self = ChildProcessInstance;
    friend Orchestrator;
    friend SmReconfigureBroker;

public:
    ChildProcessInstance(Orchestrator* orchestrator, std::shared_ptr<ChildProcessConfig> childProcessConfig,
        DWORD session = ipc::KnownSession::Any)
        : orchestrator_(orchestrator)
        , childProcessConfig_(childProcessConfig)
        , topic_(Guid(true), session)
        , publisherContext_(false)
    {
    }

    enum class LaunchReason
    {
        ApplyConfig,
        Restart
    };

    HRESULT Launch(LaunchReason launchReason) noexcept;
    HRESULT Terminate() noexcept;
    HRESULT LoadModules() noexcept;
    HRESULT UnloadModules() noexcept;

    HRESULT Publish(const std::string_view msg, const ipc::Topic& topic);

private:
#pragma region States
    // clang-format off
    struct SWaitTermination {};
    struct SWaitThreadsJoined {};
    // clang-format on
#pragma endregion

#pragma region Events
    // clang-format off
    struct EProcessTerminated {};
    struct EThreadsJoined {};
    // clang-format on
#pragma endregion

#pragma region Actions
    void APublishTerminationEventToProcess()
    {
    }
    void AThreadsRequestStopAndJoin()
    {
    }
#pragma endregion

    auto operator()() const
    {
        using namespace sml;
        // clang-format off
        return make_transition_table(
           *state<SWaitTermination>     + event<EProcessTerminated>                                             = state<SWaitThreadsJoined>,
            state<SWaitTermination>     + sml::on_entry<_>          / &self::APublishTerminationEventToProcess,
            state<SWaitThreadsJoined>   + event<EThreadsJoined>                                                 = X,
            state<SWaitThreadsJoined>   + sml::on_entry<_>          / &self::AThreadsRequestStopAndJoin
        );
        // clang-format on
    }

    void StartForwardStderr() noexcept;

    bool ShouldBreakAwayFromJob() const;

    bool operator==(const ChildProcessInstance& rhs) const;

    bool OnMessage(const ipc::MsgItem& msgItem);

    Orchestrator*                       orchestrator_;
    std::shared_ptr<ChildProcessConfig> childProcessConfig_;
    ipc::Topic                          topic_;
    wil::unique_process_information     processInfo_;

    // stdin from host point of view
    wil::unique_handle inRead_;
    wil::unique_handle inWrite_;
    // stdout from host point of view
    wil::unique_handle outRead_;
    wil::unique_handle outWrite_;
    // stderr from host point of view
    wil::unique_handle errRead_;
    wil::unique_handle errWrite_;

    std::jthread                               stderrForwarder_;
    ipc::SubscriberContext                     subscriberContext_;
    std::jthread                               keepAlive_;
    std::unordered_set<Guid, absl::Hash<Guid>> topicIds_;
    ipc::PublisherContext                      publisherContext_;
};
