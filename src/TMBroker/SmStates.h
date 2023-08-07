#pragma once

namespace sms
{
// clang-format off

// SmOrchestrator
struct Launched {};
struct Active {};

// SmReconfigureBroker
struct CalculatingDesiredProcesses {};
struct TerminatingUndesiredProcesses {};
struct LaunchMissingProcesses {};
struct LoadingModules {};

// clang-format on
}
