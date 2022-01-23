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
            // based on AnsiConsoleThemes.Code:
            // https://github.com/serilog/serilog-sinks-console/blob/dev/src/Serilog.Sinks.Console/Sinks/SystemConsole/Themes/AnsiConsoleThemes.cs#L63
            // https://www.ditig.com/256-colors-cheat-sheet

            var theme = new TemplateTheme(
                new Dictionary<TemplateThemeStyle, string>
                {
                    [TemplateThemeStyle.Text] = "\x1b[38;5;0253m",
                    [TemplateThemeStyle.SecondaryText] = "\x1b[38;5;0246m",
                    [TemplateThemeStyle.TertiaryText] = "\x1b[38;5;0242m",
                    [TemplateThemeStyle.Invalid] = "\x1b[33;1m",
                    [TemplateThemeStyle.Null] = "\x1b[38;5;0038m",
                    [TemplateThemeStyle.Name] = "\x1b[38;5;0081m",
                    [TemplateThemeStyle.String] = "\x1b[38;5;0216m",
                    [TemplateThemeStyle.Number] = "\x1b[38;5;151m",
                    [TemplateThemeStyle.Boolean] = "\x1b[38;5;0038m",
                    [TemplateThemeStyle.Scalar] = "\x1b[38;5;0079m",
                    [TemplateThemeStyle.LevelVerbose] = "\x1b[37m",
                    [TemplateThemeStyle.LevelDebug] = "\x1b[37m",
                    [TemplateThemeStyle.LevelInformation] = "\x1b[38;5;0034m",//"\x1b[37;1m",
                    [TemplateThemeStyle.LevelWarning] = "\x1b[38;5;0229m",
                    [TemplateThemeStyle.LevelError] = "\x1b[38;5;0197m\x1b[48;5;0238m",
                    [TemplateThemeStyle.LevelFatal] = "\x1b[38;5;0197m\x1b[48;5;0238m",
                });

            // https://github.com/serilog/serilog/wiki/Configuration-Basics
            string template = "[{@t:yyyy-MM-dd HH:mm:ss.fff zzz}] [m{@l:u3}] {@m,-65}[{ProcessId}='{ProcessName}'/{ThreadId}='{ThreadName}'][{SourceContext}]\n";
            var formatterConsole = new ExpressionTemplate(template, theme: theme);
            var formatterFile = new ExpressionTemplate(template);

            var logFile = config.GetValue<string>("Runtime:LogOutputFile");

            Log.Logger = new LoggerConfiguration()
                .Enrich.WithThreadId()
                .Enrich.WithThreadName()
                .Enrich.WithProcessId()
                .Enrich.WithProcessName()
                .WriteTo.Console(formatter: formatterConsole)
                .WriteTo.File(path: logFile,
                    formatter: formatterFile,
                    rollingInterval: RollingInterval.Day,
                    rollOnFileSizeLimit: true)
                .MinimumLevel.Verbose()
                .ReadFrom.Configuration(config)
                .CreateLogger();

            Log.Information($"You may find a log file here: {logFile}");
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
