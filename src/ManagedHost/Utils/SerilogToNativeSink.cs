using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Serilog;
using Serilog.Configuration;
using Serilog.Core;
using Serilog.Events;
using Serilog.Formatting;

namespace ManagedHost.Utils
{
    public class SerilogToNativeSink : ILogEventSink
    {
        private readonly ITextFormatter _formatter;
        private readonly LogEventLevel _restrictedToMinimumLevel;

        public SerilogToNativeSink(ITextFormatter formatter, LogEventLevel restrictedToMinimumLevel)
        {
            _formatter = formatter;
            _restrictedToMinimumLevel = restrictedToMinimumLevel;
        }

        public void Emit(LogEvent logEvent)
        {
            if (logEvent.Level < _restrictedToMinimumLevel)
                return;

            var stringWriter = new StringWriter(new StringBuilder(256));
            _formatter.Format(logEvent, stringWriter);
            string message = stringWriter.ToString();

            NativeMethods.OnLog((int)logEvent.Level, message);
        }
    }

    public static class NativeSinkExtensions
    {
        public static LoggerConfiguration NativeSink(
                  this LoggerSinkConfiguration loggerConfiguration,
                  ITextFormatter formatter,
                  LogEventLevel restrictedToMinimumLevel = LogEventLevel.Verbose)
        {
            return loggerConfiguration.Sink(new SerilogToNativeSink(formatter, restrictedToMinimumLevel));
        }
    }

    internal static class NativeMethods
    {
        #region OnLog
        [DllImport("UniversalModuleHost64.exe", EntryPoint = "OnLog", CharSet = CharSet.Unicode)]
        static extern int OnLog64(int level, string message);

        [DllImport("UniversalModuleHost32.exe", EntryPoint = "OnLog", CharSet = CharSet.Unicode)]
        static extern int OnLog32(int level, string message);

        public static int OnLog(int level, string message)
        {
            if (IntPtr.Size == 4)
                return OnLog32(level, message);
            else
                return OnLog64(level, message);
        }
        #endregion
    }
}
