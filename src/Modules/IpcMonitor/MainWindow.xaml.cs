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
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace IpcMonitor
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

        private void Messages_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var mi = ((sender as ListView).SelectedItem as MessageItem);
            if (mi == null)
                return;

            JToken token = null;
            try
            {
                token = JValue.Parse(mi.Message);
            }
            catch
            {
            }

            if (token != null)
            {
                string msg = token.ToString(Formatting.Indented);
                var doc = new FlowDocument(new Paragraph(new Run(msg)));
                MessageDetails.Document = doc;
            }
            else
            {
                var doc = new FlowDocument(new Paragraph(new Run(mi.Message)));
                MessageDetails.Document = doc;
            }

        }
    }
}
