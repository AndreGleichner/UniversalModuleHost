using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
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

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            Ipc.SendMessage("https://www.heise.de/", "{BEA684E7-697F-4201-844F-98224FA16D2F}");
        }

        private void Button_Click_1(object sender, RoutedEventArgs e)
        {
            var rnd = new Random();
            string msg = $@"{{
""Prop1"": {rnd.Next(1, 100)},
""Prop2"": ""Hello World""
}}";
            Ipc.SendMessage(msg, "{60DE68BB-50FD-4CB8-A808-1CEBEE3B034E}", 1);
        }
    }
}
