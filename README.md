# Universal Module Host

* My playground for some custom .Net hosting experiments.
* My intend is to have finegrained control over hosting multiple native and managed modules together in a single process.
* There should be a bidirectional communication possible between native and managed modules, preferrably some pub/sub.
* That process may run as commandline app or Windows service.
* It should be supported to run many such services in parallel each with a different set of loaded modules. 

This repository is using [vcpkg](https://github.com/microsoft/vcpkg) for native dependencies.
To get started you somehow need to get vcpkg onto your machine and integrate with MSBuild.
This may help: https://devblogs.microsoft.com/cppblog/vcpkg-artifacts/

 E.g. on a Powershell prompt:
```
> iex (iwr -useb https://aka.ms/vcpkg-init.ps1)
> vcpkg integrate install
```
This installs vcpkg into ~\.vcpkg
