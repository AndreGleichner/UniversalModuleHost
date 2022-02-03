#pragma once
#include <Windows.h>
#include <wil/resource.h>

class ChildProcess final
{
public:
    ChildProcess()
    {
    }

    HRESULT Create(PCWSTR commandline);
    HRESULT SendMsg();

private:
    void StartForwardStderr() noexcept;

    wil::unique_process_handle process_;
    wil::unique_handle         inRead_;
    wil::unique_handle         inWrite_;
    wil::unique_handle         outRead_;
    wil::unique_handle         outWrite_;
    wil::unique_handle         errRead_;
    wil::unique_handle         errWrite_;
    std::thread                stderrForwarder_;
};
