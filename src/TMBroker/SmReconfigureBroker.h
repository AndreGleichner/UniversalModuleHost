#pragma once

#include "SmEvents.h"
#include "SmStates.h"

class Orchestrator;

class SmReconfigureBroker
{
    using self = SmReconfigureBroker;

public:
    SmReconfigureBroker(Orchestrator* orchestrator) : o_(orchestrator)
    {
    }

    auto operator()() const noexcept
    {
        using namespace sml;

        auto const calcProc        = wrap(&self::ACalcDesiredProcesses);
        auto const startTermThread = wrap(&self::AStartProcTermThread);
        auto const launchMissing   = wrap(&self::ALaunchMissingProcesses);
        auto const loadMod         = wrap(&self::ALoadModules);
        auto const storeModMeta    = wrap(&self::AStoreModMetaInProcInstance);

        auto const bootAndModMetaConfStore = wrap(&self::GIsBootstrappingAndModMetaConfstore);
        auto const isModMeta               = wrap(&self::GIsModMeta);

        // clang-format off
        return make_transition_table(
           *state<sms::CalculatingDesiredProcesses>   + event<sme::CalculatedDesiredProcesses>                                  = state<sms::TerminatingUndesiredProcesses>,
            state<sms::CalculatingDesiredProcesses>   + sml::on_entry<_>                                    / calcProc,
            state<sms::TerminatingUndesiredProcesses> + sml::on_entry<_>                                    / startTermThread,
            state<sms::TerminatingUndesiredProcesses> + event<sme::AllUndesiredProcessesTerminated>                             = state<sms::LaunchMissingProcesses>,
            state<sms::LaunchMissingProcesses>        + sml::on_entry<_>                                    / launchMissing,
            state<sms::LaunchMissingProcesses>        + event<sme::AllDesiredProcessesLaunched>                                 = state<sms::LoadingModules>,
            state<sms::LoadingModules>                + event<sme::AllModulesLoaded>                                            = X,
            state<sms::LoadingModules>                + sml::on_entry<_>                                    / loadMod,
            state<sms::LoadingModules>                + event<sme::ReceivedMessage>[bootAndModMetaConfStore]/ defer,
            state<sms::LoadingModules>                + event<sme::ReceivedMessage>[isModMeta]              / storeModMeta
        );
        // clang-format on
    }

private:
    Orchestrator* o_;

#pragma region Actions
    void ACalcDesiredProcesses();
    void ALaunchMissingProcesses();
    void ALoadModules();
    void AStoreModMetaInProcInstance();
    void AStartProcTermThread();
#pragma endregion

#pragma region Guards
    bool GIsBootstrapping() const noexcept;
    bool GIsCurMsgModMetaConfstore() const noexcept;
    bool GIsBootstrappingAndModMetaConfstore() const noexcept;
    bool GIsModMeta() const noexcept;
#pragma endregion
};
