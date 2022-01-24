using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using ManagedHost.Utils;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Serilog;
using Serilog.Templates;
using Serilog.Templates.Themes;

namespace ManagedHost
{
    class Program
    {
        private const int FailureExitCode = 213;

        private static int Main(string[] args)
        {
            // Strange that the thread name as set on native side is not visible here,
            // but setting it here also makes it visible in any callback into native code.
            Thread.CurrentThread.Name = "UMH-NetMain";

            AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;

            // https://dev.to/ballcapz/adding-dependency-injection-to-net-core-console-applications-on-windows-3pm0
            var services = ConfigureServices();

            var serviceProvider = services.BuildServiceProvider();

            int res = serviceProvider.GetService<App>().Run(args);

            Log.Information($"Exiting managed Main() with {res}");
            return res;
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
            services.AddTransient<App>();

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
}
