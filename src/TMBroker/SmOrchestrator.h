#pragma once

#include <boost/sml.hpp>
namespace sml = boost::sml;

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "ipc.h"
#include "SmEvents.h"
#include "SmStates.h"
#include "SmReconfigureBroker.h"

class Orchestrator;

class SmOrchestrator
{
    using self = SmOrchestrator;

public:
    SmOrchestrator(Orchestrator* orchestrator) : o_(orchestrator)
    {
    }

    auto operator()() const noexcept
    {
        using namespace sml;

        auto const init            = wrap(&self::AInitialize);
        auto const queryBrokerConf = wrap(&self::AQueryBrokerConfig);
        auto const emptyBrokerConf = wrap(&self::AStoreEmptyBrokerConfigForShutdown);
        auto const storeBrokerConf = wrap(&self::AStoreBrokerConfig);
        auto const dispMsg         = wrap(&self::ADispatchMsgToHosts);
        auto const err             = wrap(&self::AException);

        // clang-format off
        return make_transition_table(
           *state<sms::Launched>       + event<sme::Initialize>                                     / init              = state<SmReconfigureBroker>,
            state<SmReconfigureBroker> + event<sme::ReceivedModMetaConfstore>                       / queryBrokerConf,
            state<SmReconfigureBroker> + event<sme::Shutdown>                                       / defer,
            state<SmReconfigureBroker> + event<sme::AppliedBrokerConfig>[&self::GHasAnyChildProcs]                      = state<sms::Active>,
            state<SmReconfigureBroker> + event<sme::AppliedBrokerConfig>[&self::GHasNoChildProcs]                       = X,
            state<SmReconfigureBroker> + exception<_>                                               / err               = state<sms::Launched>,
            state<sms::Active>         + event<sme::WtsSessionChanged>                                                  = state<SmReconfigureBroker>,
            state<sms::Active>         + event<sme::Shutdown>                                       / emptyBrokerConf   = state<SmReconfigureBroker>,
            state<sms::Active>         + event<sme::ReceivedMessage>[&self::GIsCurMsgBrokerConfig]  / storeBrokerConf   = state<SmReconfigureBroker>,
            state<sms::Active>         + event<sme::ReceivedMessage>[&self::GDispCurMsgToHosts]     / dispMsg,
            state<sms::Active>         + exception<_>                                               / err               = state<sms::Launched>
        );
        // clang-format on
    }

private:
    Orchestrator* o_;

#pragma region Actions
    void AInitialize();
    void AQueryBrokerConfig();
    void ALaunchMissingProcesses();
    void ALoadModules();
    void AStoreModMetaInProcInstance();
    void ABootstrappingFinished();
    void AStoreEmptyBrokerConfigForShutdown();
    void AStoreBrokerConfig();
    void ADispatchMsgToHosts();
    void AStartProcTermThread();
    void AException();
#pragma endregion

#pragma region Guards
    bool GIsBootstrapping() const noexcept;
    bool GIsCurMsgModMetaConfstore() const noexcept;
    bool GIsCurMsgBrokerConfig() const noexcept;
    bool GDispCurMsgToHosts() const noexcept;
    bool GHasAnyChildProcs() const noexcept;
    bool GHasNoChildProcs() const noexcept;
#pragma endregion
};
