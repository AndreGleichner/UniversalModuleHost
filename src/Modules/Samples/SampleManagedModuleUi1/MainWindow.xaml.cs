using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using SharedManagedUtils;

namespace SampleManagedModuleUi1
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void Button_Click_Browse(object sender, RoutedEventArgs e)
        {
            Ipc.SendMessage(tbBrowse.Text, Ipc.ShellExec);
        }

        private void Button_Click_SendMsg(object sender, RoutedEventArgs e)
        {
            Ipc.SendMessage(tbSendMsg.Text, "{60DE68BB-50FD-4CB8-A808-1CEBEE3B034E}");
        }

        private void Button_Click_StoreConf(object sender, RoutedEventArgs e)
        {
            var cs = new ConfStore { Cmd = ConfStore.ECmd.Update, Args = tbStoreConf.Text };
            string msg = JsonSerializer.Serialize(cs);

            Ipc.SendMessage(msg, Ipc.ConfStore);
        }

        private void Button_Click_QueryConf(object sender, RoutedEventArgs e)
        {
            var cs = new ConfStore { Cmd = ConfStore.ECmd.Query, Args = tbQueryConf.Text };
            string msg = JsonSerializer.Serialize(cs);

            Ipc.SendMessage(msg, Ipc.ConfStore);
        }
    }
}
