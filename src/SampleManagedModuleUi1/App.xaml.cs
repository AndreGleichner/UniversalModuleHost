using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using SharedManagedUtils;

namespace SampleManagedModuleUi1
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            Ipc.Init(OnMessageFromHost, OnTerminate);
        }

        private static int OnMessageFromHost(string msg, string service, int session)
        {
            //Log.Verbose($"MessageFromHostToModule: '{msg.Replace("\r", "").Replace("\n", "")}' '{service}' {session}");

            return 0;
        }

        private static void OnTerminate()
        {
            Application.Current.Shutdown();
        }
    }
}
