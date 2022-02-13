using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Threading;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Serilog;
using Serilog.Templates;
using SharedManagedUtils;

namespace ManagedHost
{
    class Program
    {
        private const int FailureExitCode = 213;
        private static readonly ManualResetEvent terminated_ = new(false);
        private static ModuleHost _moduleHost;

        private static int Main(string[] args)
        {
            //while (!Debugger.IsAttached)
            //{
            //    Thread.Sleep(1000);
            //}
            //Debugger.Break();

            // Strange that the thread name as set on native side is not visible here,
            // but setting it here also makes it visible in any callback into native code.
            Thread.CurrentThread.Name = "UMH-NetMain";

            AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;

            // https://dev.to/ballcapz/adding-dependency-injection-to-net-core-console-applications-on-windows-3pm0
            var services = ConfigureServices();

            var serviceProvider = services.BuildServiceProvider();

            _moduleHost = serviceProvider.GetService<ModuleHost>();
            Ipc.Init(OnMessageFromHost, OnTerminate);

            terminated_.WaitOne();

            Log.Information($"Exiting managed Main()");
            return 0;
        }

        private static int OnMessageFromHost(string msg, string service, uint session)
        {
            Log.Information($"MessageFromHostToModule: '{msg}' '{service}' {session}");

            if (service == Ipc.ManagedHost)
            {
                var hostCmdMsg = JsonSerializer.Deserialize<HostCmdMsg>(msg);
                if (hostCmdMsg.Cmd == HostCmdMsg.ECmd.CtrlModule)
                {
                    var args = JsonSerializer.Deserialize<HostCtrlModuleArgs>(hostCmdMsg.Args);
                    if (args.Cmd == HostCtrlModuleArgs.ECmd.Load)
                    {
                        _moduleHost.LoadModule(args.Module);
                    }
                    else if (args.Cmd == HostCtrlModuleArgs.ECmd.Unload)
                    {
                        _moduleHost.UnloadModule(args.Module);
                    }
                }
            }
            return 0;
        }

        private static void OnTerminate()
        {
            terminated_.Set();
        }

        private static void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            Log.Fatal($"ManagedHost FAIL! Exiting due to unhandled exception: {e.ExceptionObject}");
            Environment.Exit(FailureExitCode);
        }

        private static IServiceCollection ConfigureServices()
        {
            IServiceCollection services = new ServiceCollection();

            var config = LoadConfiguration();
            ConfigureLogging(config);

            services.AddSingleton(config);
            services.AddSingleton<IConfigurationRoot>((serviceProvider) => config);
            services.AddSingleton<Microsoft.Extensions.Logging.ILogger>(new SerilogAsMicrosoftLogger());
            services.AddTransient<ModuleHost>();

            return services;
        }

        private static IConfigurationRoot LoadConfiguration()
        {
            using var curProc = Process.GetCurrentProcess();

            var builder = new ConfigurationBuilder()
                .SetBasePath(Path.GetDirectoryName(curProc.MainModule.FileName))
                .AddJsonFile("appsettings.json", optional: true, reloadOnChange: true)
                .AddEnvironmentVariables("UMH_CFG_");

            return builder.Build();
        }

        private static void ConfigureLogging(IConfigurationRoot config)
        {
            // https://github.com/serilog/serilog/wiki/Configuration-Basics
            var formatterToNative = new ExpressionTemplate("{@m,-65}[{ThreadId}='{ThreadName}']");

            Log.Logger = new LoggerConfiguration()
                .Enrich.WithThreadId()
                .Enrich.WithThreadName()
                .Enrich.WithProcessId()
                .Enrich.WithProcessName()
                .WriteTo.NativeSink(formatter: formatterToNative)
                .MinimumLevel.Verbose()
                .ReadFrom.Configuration(config)
                .CreateLogger();
        }

#if FALSE
see cpp code under #if INIT_HOSTFXR_FROM == INIT_HOSTFXR_FROM_RUNTIMECONFIG

        [StructLayout(LayoutKind.Sequential)]
        public struct MainArgs
        {
            public IntPtr ThisHost;
            public IntPtr Commandline;
        }

        static IntPtr _thisHost { get; set; }

        [UnmanagedCallersOnly]
        public static int NativeHostMain(MainArgs args)
        {
            Console.WriteLine($"({Thread.CurrentThread.ManagedThreadId}) Entering managed NativeHostMain({args.ThisHost:X})");

            _thisHost = args.ThisHost;

            string commandline = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
                ? Marshal.PtrToStringUni(args.Commandline)
                : Marshal.PtrToStringUTF8(args.Commandline);

            int res = Run(commandline.Split(' '));

            Console.WriteLine("Exiting managed NativeHostMain()");

            return res;
        }
#endif
    }

    internal static class NativeMethods
    {
        [DllImport("UniversalModuleHost64.exe", EntryPoint = "OnMessageFromModule", CharSet = CharSet.Unicode)]
        static extern int MessageFromModuleToHost64(string msg, string service, int session);

        [DllImport("UniversalModuleHost32.exe", EntryPoint = "OnMessageFromModule", CharSet = CharSet.Unicode)]
        static extern int MessageFromModuleToHost32(string msg, string service, int session);

        public static int MessageFromModuleToHost(string msg, string service, int session)
        {
            if (IntPtr.Size == 4)
                return MessageFromModuleToHost32(msg, service, session);
            else
                return MessageFromModuleToHost64(msg, service, session);
        }
    }
}
