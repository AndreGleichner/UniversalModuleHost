using System.IO;
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

            ManagedHost.NativeMethods.OnLog((int)logEvent.Level, message);
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
}
