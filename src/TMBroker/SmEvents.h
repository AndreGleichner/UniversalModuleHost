#pragma once

namespace sme
{
// clang-format off
struct Initialize {};
struct CalculatedDesiredProcesses {};
struct AllUndesiredProcessesTerminated {};
struct AllDesiredProcessesLaunched {};
struct AllModulesLoaded {};
struct WtsSessionChanged {};
struct Shutdown {};
struct AppliedBrokerConfig {};
struct ReceivedMessage {};
struct ReceivedModMetaConfstore {};
// clang-format on
}
