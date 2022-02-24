using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using SharedManagedUtils;

namespace IpcMonitor
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            Ipc.Init(OnMessageFromHost, OnTerminate);

            var mm = new ModuleMeta { Pid = Process.GetCurrentProcess().Id, Name = "IpcMonitor", Services = { Guid.Empty.ToString() } };
            string msg = JsonSerializer.Serialize(mm);

            Ipc.SendMessage(msg, Ipc.ModuleMetaConsumer);
        }

        private static int OnMessageFromHost(string msg, string service, uint session)
        {
            //Log.Verbose($"MessageFromHostToModule: '{msg.Replace("\r", "").Replace("\n", "")}' '{service}' {session}");
            var mi = new MessageItem(Timestamp: DateTime.Now.ToString("HH:mm:ss"), Message: msg.Replace("\r\n", ""), Service: service, Session: ((int)session).ToString());

            Application.Current.Dispatcher.BeginInvoke(() =>
                {
                    var messages = ((MainWindow)Application.Current.MainWindow).Messages;
                    messages.Items.Add(mi);
                    //messages.ScrollIntoView(messages.Items[messages.Items.Count - 1]);
                });

            return 0;
        }

        private static void OnTerminate()
        {
            Application.Current.Shutdown();
        }
    }
}
