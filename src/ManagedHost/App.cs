using System;
using System.Runtime.InteropServices;
using System.Threading;
using Microsoft.Extensions.Configuration;
using Serilog;

namespace ManagedHost
{
    internal class App
    {
        private readonly IConfigurationRoot _config;

        public App(IConfigurationRoot config)
        {
            _config = config;
        }

        public int Run(string[] args)
        {
            Log.Information("-> App.Run()");

            Log.Information($"Having {args.Length} args");
            int n = 0;
            foreach (string arg in args)
            {
                Log.Information($"Args {n++}: '{arg}'");
            }
            for (int i = 0; i < 10; ++i)
            {
                int res = NativeMethods.OnProgressFromManaged(IntPtr.Zero, i);
                Log.Information($"OnProgressFromManaged({i}) res={res}");
                Thread.Sleep(TimeSpan.FromSeconds(1));
            }

            return 42;
        }
    }

    internal static class NativeMethods
    {
        [DllImport("UniversalModuleHost64.exe", EntryPoint = "OnProgressFromManaged")]
        static extern int OnProgressFromManaged64(IntPtr thisHost, int progress);

        [DllImport("UniversalModuleHost32.exe", EntryPoint = "OnProgressFromManaged")]
        static extern int OnProgressFromManaged32(IntPtr thisHost, int progress);

        public static int OnProgressFromManaged(IntPtr thisHost, int progress)
        {
            if (IntPtr.Size == 4)
                return OnProgressFromManaged32(thisHost, progress);
            else
                return OnProgressFromManaged64(thisHost, progress);
        }
    }
}
