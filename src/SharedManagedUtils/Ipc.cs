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

        public delegate int OnMessageFromHost(string msg, string service, uint session);
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

        [DllImport("UniversalModuleHost64.exe", EntryPoint = "OnMessageFromModule", CharSet = CharSet.Unicode)]
        static extern int MessageFromModuleToHost64(string msg, string service, int session);

        [DllImport("UniversalModuleHost32.exe", EntryPoint = "OnMessageFromModule", CharSet = CharSet.Unicode)]
        static extern int MessageFromModuleToHost32(string msg, string service, int session);

        public static int SendMessage(string msg, string service, int session)
        {
            if (IntPtr.Size == 4)
                return MessageFromModuleToHost32(msg, service, session);
            else
                return MessageFromModuleToHost64(msg, service, session);
        }

        [UnmanagedCallersOnly]
        public static int MessageFromHostToModule(IntPtr msg, IntPtr service, uint session)
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
    }
}
