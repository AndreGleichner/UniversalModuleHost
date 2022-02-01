#pragma once

#include <Windows.h>
#include <string>
#include <functional>

class ServiceBase
{
public:
#pragma region ReplicatedFromWinHeadersForLogging
    enum class State : std::uint32_t
    {
        STOPPED          = 0x00000001,
        START_PENDING    = 0x00000002,
        STOP_PENDING     = 0x00000003,
        RUNNING          = 0x00000004,
        CONTINUE_PENDING = 0x00000005,
        PAUSE_PENDING    = 0x00000006,
        PAUSED           = 0x00000007,
    };
    // https://docs.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_status
    enum class AcceptControls : std::uint32_t
    {
        NONE = 0,

        STOP                           = 0x00000001,
        PAUSE_CONTINUE                 = 0x00000002,
        SHUTDOWN                       = 0x00000004,
        PARAMCHANGE                    = 0x00000008,
        NETBINDCHANGE                  = 0x00000010,
        HARDWAREPROFILECHANGE          = 0x00000020,
        POWEREVENT                     = 0x00000040,
        SESSIONCHANGE                  = 0x00000080,
        PRESHUTDOWN                    = 0x00000100,
        TIMECHANGE                     = 0x00000200,
        TRIGGEREVENT                   = 0x00000400,
        USER_LOGOFF                    = 0x00000800,
        RESERSERVED_FOR_INTERNAL_USE_1 = 0x00001000,
        LOWRESOURCES                   = 0x00002000,
        SYSTEMLOWRESOURCES             = 0x00004000,
        UMH_DEVICEEVENT                = 0x80000000, // Universal Module Host specific, not part of Win32
    };
#pragma endregion

    struct ServiceTraits
    {
        // pass dependencies as | separated list of service names
        ServiceTraits(PCWSTR name, PCWSTR dependencies, PCWSTR displayname, PCWSTR description,
            AcceptControls acceptControls, WORD eventCategory = (WORD)-1, DWORD eventID = (DWORD)-1)
            : Name(name)
            , Dependencies(dependencies)
            , Displayname(displayname)
            , Description(description)
            , EventCategory(eventCategory)
            , EventID(eventID)
            , AcceptControls(acceptControls)
        {
        }

        bool UsingEventLog() const
        {
            return EventCategory != (WORD)-1 && EventID != (DWORD)-1;
        }

        std::wstring   Name;
        std::wstring   Dependencies;
        std::wstring   Displayname;
        std::wstring   Description;
        WORD           EventCategory;
        DWORD          EventID;
        AcceptControls AcceptControls;
    };

    ServiceBase(const ServiceTraits& traits);
    virtual ~ServiceBase() = default;

    static bool CmdlineAction(PCWSTR commandLine, const ServiceTraits& traits, int& exitCode);

    // Only returns upon service termination. Returns service exit code
    int Run();

protected:
    // In your OnStart() override you typically create your working threads.
    // The service already reported SERVICE_RUNNING to SCM.
    // Do not run lengthy operations in here so that we can ract fast to possible
    // incoming control requests.
    //
    // argc/argv are what got passed to ServiceMain() which were from StartService()
    virtual HRESULT OnStart(DWORD argc, PWSTR* argv) = 0;

#pragma region ServiceControlHandlerCallbacks
    // OnStop gets actually called not from within the service control handler HandlerEx() but
    // slightly deferred when the service terminates.
    // It gets invoked on a threadpool thread as it is called in a callback registered by RegisterWaitForSingleObject().
    // See https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-registerwaitforsingleobject
    // You need to make sure your code is threadpool safe, otherwise use CreateThread.
    virtual void OnStop()
    {
    }
    virtual void OnPause()
    {
    }
    virtual void OnContinue()
    {
    }
    virtual void OnInterrogate()
    {
    }
    // requires AcceptControls::SHUTDOWN to be passed to the ctor
    // This is a system shutdown and thus will be followed by OnStop().
    virtual void OnShutdown()
    {
    }

    // requires AcceptControls::UMH_DEVICEEVENT to be passed to the ctor and a call to RegisterDeviceNotification()
    // lpEventData corresponds to the lParam parameter that applications receive as part of
    // a WM_DEVICECHANGE message
    virtual DWORD OnDeviceEvent(DWORD dwEventType, LPVOID lpEventData)
    {
        UNREFERENCED_PARAMETER(dwEventType);
        UNREFERENCED_PARAMETER(lpEventData);

        return NO_ERROR;
    }
    // requires AcceptControls::HARDWAREPROFILECHANGE to be passed to the ctor
    virtual DWORD OnHardwareProfileChange(DWORD dwEventType)
    {
        UNREFERENCED_PARAMETER(dwEventType);

        return NO_ERROR;
    }
    // requires AcceptControls::POWEREVENT to be passed to the ctor
    // dwEventType can be one of the values specified in the wParam parameter of the WM_POWERBROADCAST message.
    // If dwEventType is PBT_POWERSETTINGCHANGE, powerBroadcastSetting is set.
    virtual DWORD OnPowerEvent(DWORD dwEventType, POWERBROADCAST_SETTING* powerBroadcastSetting)
    {
        UNREFERENCED_PARAMETER(dwEventType);
        UNREFERENCED_PARAMETER(powerBroadcastSetting);

        return NO_ERROR;
    }
    // requires AcceptControls::SESSIONCHANGE to be passed to the ctor
    // dwEventType can be one of the values specified in the wParam parameter of the WM_WTSSESSION_CHANGE message
    virtual void OnSessionChange(DWORD dwEventType, DWORD dwSessionId)
    {
        UNREFERENCED_PARAMETER(dwEventType);
        UNREFERENCED_PARAMETER(dwSessionId);
    }
    // requires AcceptControls::PRESHUTDOWN to be passed to the ctor
    // This prepares a system shutdown and will wait for this callback to complete.
    // Only use in very specific situations.
    virtual void OnPreShutdown()
    {
    }
    // requires AcceptControls::TIMECHANGE to be passed to the ctor; Requires at least Win7
    virtual void OnTimeChange(SERVICE_TIMECHANGE_INFO* timeChangeInfo)
    {
        UNREFERENCED_PARAMETER(timeChangeInfo);
    }
#pragma endregion

    ServiceTraits         Traits;
    SERVICE_STATUS_HANDLE StatusHandle{};
    SERVICE_STATUS        Status{SERVICE_WIN32_OWN_PROCESS};

private:
    void    ServiceMain(_In_ DWORD argc, _In_ PWSTR* argv);
    DWORD   HandlerEx(_In_ DWORD dwControl, _In_ DWORD dwEventType, _In_ PVOID lpEventData);
    void    SetServiceStatus(State state);
    HRESULT LogEvent(PCWSTR format, ...);

    HDEVNOTIFY devNotify_{};
    HANDLE     stopEvent_{};
    HANDLE     waitForStop_{};

#pragma region CmdlineActions
    static HRESULT Register(const ServiceTraits& traits, const std::wstring& ppl);
    static HRESULT Unregister(const ServiceTraits& traits, const std::wstring& unused);
    static HRESULT Start(const ServiceTraits& traits, const std::wstring& unused);
    static HRESULT Stop(const ServiceTraits& traits, const std::wstring& unused);
    static HRESULT Restart(const ServiceTraits& traits, const std::wstring& unused);

    static HRESULT StopImpl(const ServiceTraits& traits, bool deleteService);

    static HRESULT AddEventSource(const ServiceTraits& traits);
    static HRESULT RemoveEventSource(const ServiceTraits& traits);
#pragma endregion

    static void WINAPI ServiceMain_(_In_ DWORD argc, _In_ PWSTR* argv);
    // Called by the SCM whenever a control code is sent to the service.
    static DWORD WINAPI HandlerEx_(
        _In_ DWORD dwControl, _In_ DWORD dwEventType, _In_ PVOID lpEventData, _In_ PVOID lpContext);
    static void ServiceStopping(_In_ PVOID lpParameter, _In_ BOOLEAN TimerOrWaitFired);

    static ServiceBase* _this;
};
