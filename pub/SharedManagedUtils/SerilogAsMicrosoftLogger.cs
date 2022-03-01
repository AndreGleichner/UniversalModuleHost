using System;

namespace SharedManagedUtils
{
    class SerilogAsMicrosoftLogger : Microsoft.Extensions.Logging.ILogger
    {

        //
        // Summary:
        //     Begins a logical operation scope.
        //
        // Parameters:
        //   state:
        //     The identifier for the scope.
        //
        // Type parameters:
        //   TState:
        //     The type of the state to begin scope for.
        //
        // Returns:
        //     An System.IDisposable that ends the logical operation scope on dispose.
        public IDisposable BeginScope<TState>(TState state)
        {
            return null;
        }
        //
        // Summary:
        //     Checks if the given logLevel is enabled.
        //
        // Parameters:
        //   logLevel:
        //     level to be checked.
        //
        // Returns:
        //     true if enabled.
        public bool IsEnabled(Microsoft.Extensions.Logging.LogLevel logLevel)
        {
            if (logLevel == Microsoft.Extensions.Logging.LogLevel.None)
                return false;
            return Serilog.Log.IsEnabled(ToSerilogLevel(logLevel));
        }
        //
        // Summary:
        //     Writes a log entry.
        //
        // Parameters:
        //   logLevel:
        //     Entry will be written on this level.
        //
        //   eventId:
        //     Id of the event.
        //
        //   state:
        //     The entry to be written. Can be also an object.
        //
        //   exception:
        //     The exception related to this entry.
        //
        //   formatter:
        //     Function to create a System.String message of the state and exception.
        //
        // Type parameters:
        //   TState:
        //     The type of the object to be written.
        public void Log<TState>(Microsoft.Extensions.Logging.LogLevel logLevel, Microsoft.Extensions.Logging.EventId eventId, TState state, Exception exception, Func<TState, Exception, string> formatter)
        {
            Serilog.Log.Write(ToSerilogLevel(logLevel), exception, state.ToString());
        }

        private static Serilog.Events.LogEventLevel ToSerilogLevel(Microsoft.Extensions.Logging.LogLevel logLevel)
        {
            if (logLevel == Microsoft.Extensions.Logging.LogLevel.None)
                throw new ArgumentOutOfRangeException(nameof(logLevel), "There's no equivalent to None in Serilog");

            return (Serilog.Events.LogEventLevel)(int)logLevel;
        }
    }
}
