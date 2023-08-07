#include "pch.h"

#include "Orchestrator.h"

#pragma region Actions

void SmReconfigureBroker::ACalcDesiredProcesses()
{
    o_->UpdateChildProcessConfig();
    o_->CalcDesiredChildProcesses();
    o_->sm_.process_event(sme::CalculatedDesiredProcesses {});
}

void SmReconfigureBroker::ALaunchMissingProcesses()
{
}

void SmReconfigureBroker::ALoadModules()
{
}

void SmReconfigureBroker::AStoreModMetaInProcInstance()
{
}

void SmReconfigureBroker::AStartProcTermThread()
{
    std::vector<std::unique_ptr<ChildProcessInstance>> processesToTerminate;
    // Check whether running processes match desired set of processes.
    // Terminate any non-desired.
    for (auto pi = o_->childProcesses_.begin(); pi != o_->childProcesses_.end();)
    {
        auto p = pi->get();

        bool stillDesired = false;
        for (auto dpi = o_->desiredChildProcesses_.cbegin(); dpi != o_->desiredChildProcesses_.cend(); ++dpi)
        {
            auto dp = dpi->get();
            if (*p == *dp)
            {
                stillDesired = true;
                (void)o_->desiredChildProcesses_.erase(dpi);
                break;
            }
        }

        if (stillDesired)
        {
            ++pi;
        }
        else
        {
            // No longer desired, so terminate (deferred in a thread below) and remove.
            processesToTerminate.emplace_back(std::move(*pi));
            pi = o_->childProcesses_.erase(pi);
        }
    }

    if (!processesToTerminate.empty())
    {
        std::jthread terminator([&] {
            for (auto& process : processesToTerminate)
            {
                process.get()->Terminate();
            }
        });
    }
    o_->process_event(sme::AllUndesiredProcessesTerminated {});
}
#pragma endregion

#pragma region Guards

bool SmReconfigureBroker::GIsBootstrapping() const noexcept
{
    return true;
}
bool SmReconfigureBroker::GIsCurMsgModMetaConfstore() const noexcept
{
    return true;
}
bool SmReconfigureBroker::GIsBootstrappingAndModMetaConfstore() const noexcept
{
    return GIsBootstrapping() && GIsCurMsgModMetaConfstore();
}
bool SmReconfigureBroker::GIsModMeta() const noexcept
{
    return true;
}

#pragma endregion
