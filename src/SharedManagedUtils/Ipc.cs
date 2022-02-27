using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Threading;

namespace SharedManagedUtils
{
    class Ipc
    {
        public const string ManagedHost = "{7924FE60-C967-449C-BA5D-2EBAA7D16024}";
        public const string ModuleMetaConsumer = "{6E6A094C-839F-4EAF-BD22-08CB9E1A318F}";
        public const string ShellExec = "{BEA684E7-697F-4201-844F-98224FA16D2F}";
        public const string ConfStore = "{8583CDC9-DB92-45BE-90CE-4D3AA4CD14F5}";
        public const string ConfConsumer = "{8ED3A4D7-7C78-4B88-A547-A4D87A9DDC35}";

        public delegate int OnMessageFromHost(string msg, string service, int session);
        public delegate void OnTerminate();

        private static readonly ManualResetEvent initialized_ = new(false);
        private static OnMessageFromHost _onMessageFromHost;
        private static OnTerminate _onTerminate;

        public static void Init(OnMessageFromHost onMessageFromHost, OnTerminate onTerminate)
        {
            _onMessageFromHost = onMessageFromHost;
            _onTerminate = onTerminate;

            initialized_.Set();
        }

        [DllImport("UniversalModuleHost64.exe", EntryPoint = "MessageFromModuleToHost", CharSet = CharSet.Unicode)]
        static extern int MessageFromModuleToHost64(string msg, string service, int session);

        [DllImport("UniversalModuleHost32.exe", EntryPoint = "MessageFromModuleToHost", CharSet = CharSet.Unicode)]
        static extern int MessageFromModuleToHost32(string msg, string service, int session);

        public static int SendMessage(string msg, string service, int session = -1)
        {
            try
            {
                var image = Process.GetCurrentProcess().MainModule.ModuleName;
                if (!image.Equals("UniversalModuleHost32.exe", StringComparison.OrdinalIgnoreCase) && !image.Equals("UniversalModuleHost64.exe", StringComparison.OrdinalIgnoreCase))
                    return 0;

                if (IntPtr.Size == 4)
                    return MessageFromModuleToHost32(msg, service, session);
                else
                    return MessageFromModuleToHost64(msg, service, session);
            }
            catch
            {
                Debugger.Break();
                return 1;
            }
        }

        [UnmanagedCallersOnly]
        public static int MessageFromHostToModule(IntPtr msg, IntPtr service, int session)
        {
            try
            {
                initialized_.WaitOne();

                //while (!Debugger.IsAttached)
                //{
                //    Thread.Sleep(1000);
                //}
                //Debugger.Break();

                string m = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
                    ? Marshal.PtrToStringUni(msg)
                    : Marshal.PtrToStringUTF8(msg);

                string s = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
                    ? Marshal.PtrToStringUni(service)
                    : Marshal.PtrToStringUTF8(service);

                if (s == ManagedHost)
                {
                    var hostCmdMsg = JsonSerializer.Deserialize<HostCmdMsg>(m);
                    if (hostCmdMsg.Cmd == HostCmdMsg.ECmd.Terminate)
                    {
                        _onTerminate();
                        return 0;
                    }
                }
                return _onMessageFromHost(m, s, session);
            }
            catch
            {
                Debugger.Break();
                return 1;
            }
        }
    }
}
