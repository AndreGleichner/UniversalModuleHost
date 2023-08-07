using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Threading;

namespace SharedManagedUtils
{
    partial class Ipc
    {
        public const string ShellExecTopic = "{BEA684E7-697F-4201-844F-98224FA16D2F}";

        public delegate int OnMessageFromHost(string msg, string topicId, int session);
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

        [DllImport("TMHost64.exe", EntryPoint = "MessageFromModuleToHost", CharSet = CharSet.Unicode)]
        static extern int MessageFromModuleToHost64(string msg, string topicId, int session);

        [DllImport("TMHost32.exe", EntryPoint = "MessageFromModuleToHost", CharSet = CharSet.Unicode)]
        static extern int MessageFromModuleToHost32(string msg, string topicId, int session);

        public static int SendMessage(string msg, string topicId, int session = -1)
        {
            try
            {
                var image = Process.GetCurrentProcess().MainModule.ModuleName;
                if (!image.Equals("TMHost32.exe", StringComparison.OrdinalIgnoreCase) && !image.Equals("TMHost64.exe", StringComparison.OrdinalIgnoreCase))
                    return 0;

                if (IntPtr.Size == 4)
                    return MessageFromModuleToHost32(msg, topicId, session);
                else
                    return MessageFromModuleToHost64(msg, topicId, session);
            }
            catch
            {
                Debugger.Break();
                return 1;
            }
        }

        [UnmanagedCallersOnly]
        public static int MessageFromHostToModule(IntPtr msg, IntPtr topicId, int session)
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
                    ? Marshal.PtrToStringUni(topicId)
                    : Marshal.PtrToStringUTF8(topicId);

                if (s == ManagedHostTopic)
                {
                    var hostCmdMsg = JsonSerializer.Deserialize<HostCmd>(m);
                    if (hostCmdMsg.Cmd == HostCmd.ECmd.Terminate)
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
