# Universal Module Host

* My playground for some custom .Net hosting experiments.
* My intend is to have finegrained control over hosting multiple native and managed modules together in a single process.
* There should be a bidirectional communication possible between native and managed modules, preferrably some pub/sub.
* That process may run as commandline (for debugging only) app or Windows service.
* It should be supported to run many such processes in parallel each with a different set of loaded modules. 

# Overview

![BrokerProcess](./img/BrokerProcess.drawio.png)

Currently it works like this:
* There is a single broker process (UniversalModuleBroker64/32.exe) running as a Windows service. It may be run as AM-PPL when there's a respective ELAM driver.
* The broker launches one or multiple child processes (UniversalModuleHost64/32.exe) each of which may host multiple native and/or managed modules (DLLs).
* Child processes may be launched automatically in all currently active sessions. (TODO: better handling of logon/-off)
* Child processes are laucnhed as protected processes in case the broker itself is running as PPL.
* Child processes are put into job objects (one per session) which are configured to kill child processes as soon as the broker dies.
* The broker monitors all child processes and relaunches any died child process.
* There's a broker.json declaring child processes and modules to be loaded as well as certain properties.
* Modules may be unloaded/reloaded to e.g. update some on demand.
* Processes communicate via stdin/stdout for message broadcasting and stderr for logging. See [IPC](#ipc) below.
* Messages are send to the brower which dispatches to everyone.
* Managed modules are hosted in a custom host instead of the standard apphost, comhost, muxer, etc. This is mainly to have full control and tweak certain [security](#security) properties.
* Managed modules are orchestrated by a ManagedHost.dll utilizing AssemblyLoadContext's to somewhat isolate modules and allow dynamic load/unload. This uses the great [McMaster.NETCore.Plugins library](https://github.com/natemcmaster/DotNetCorePlugins).
* Managed UI modules (e.g. WPF apps) usually have assembly DLLs which are actually marked as EXE in PE header. A UniversalModuleHost may only have a single such UI module which is loaded directly instead of ManagedHost.dll.
* The broker should run in native bitness, while even on 64bit OS host child processes may be configured to run 32bit.
* UI child processes may be configured to run with increased integrity level (see [Integrity Level](#integrity-level-ui)).


## IPC
Processes communicate via stdin/stdout for message broadcasting and stderr for logging.
There're 2 reasons for this choice: simplicity and [security](#communication).

Communication is just sending a UTF8 string to a target service-GUID / WTS session.
Messages are always send to the broker which dispatches them to every module (including sender).
Module DLLs are responsible to handle certain service-GUID.
The broker itself as well as the host processes themselves also have service-GUIDs to e.g. perform init, module (un-)load.

### Init
![Init](./img/ipc-page1.svg)

### Load module
![Load module](./img/ipc-page2.svg)

### Send diagnostic output
![Send diagnostic output](./img/ipc-page3.svg)

### Send messages
![Send messages](./img/ipc-page4.svg)


## Security

### Communication

Alternatives like named pipes are easily attacked, e.g. by guessing the name and Mallory creating such pipe befor our processes do. This can only be mitigated with the help of a npfs filter driver protecting certain names. Even worse in case of multiple processes attaching to the same named pipe communication can not just be blocked but also changed.

In former times anonymous pipes were implemented with normal named pipes. Today (Win7+ see [CreatePipeAnonymousPair7](https://stackoverflow.com/questions/60645/overlapped-i-o-on-anonymous-pipe)) they are named pipes w/o a name. So there's no usermode way to intercept or block anonymous pipes, at least none I may imagine ;)

Getting local socket stuff safe is IMHO almost impossible.

RPC:

COM:



### Process Mitigations
Using quite some flags with UpdateProcThreadAttribute()
* Inherit only defined handles (currently only pipe)
* ASLR: High entropy, Bottom up
* DEP:
* Control flow guard
* Somehow prevent DLL issues by e.g preferring System32, doing static linking libs whenever possible. Using delay-load otherwise so we can pre-load DLLs from correct paths.
* CETCompat doesn't seem to work together with .Net 6
* Filtering environment variables.
* Ensure child processes are launched by the broker

### Integrity Level (UI)
To prevent unelevated Mallory from controlling our UI we may run UI processes at an increased integrity level, e.g. Medium-Plus. (0x2100).
Windows messages can only be send to same or lower integrity level processes.

# Build

This repository is using [vcpkg](https://github.com/microsoft/vcpkg) for native dependencies.
To get started you somehow need to get vcpkg onto your machine and integrate with MSBuild.
This may help: https://devblogs.microsoft.com/cppblog/vcpkg-artifacts/

Just recently Microsoft has screwed up this download link: https://aka.ms/vcpkg-init.ps1
So you may need to use this instead below:
https://github.com/microsoft/vcpkg-tool/raw/main/vcpkg-init/vcpkg-init.ps1

 E.g. on a Powershell prompt:
```
> iex (iwr -useb https://aka.ms/vcpkg-init.ps1)
> vcpkg integrate install
```
This installs vcpkg into ~\.vcpkg

TODO: Need some Build.ps1

# Run

Just start the UniversalModuleBroker64.exe
