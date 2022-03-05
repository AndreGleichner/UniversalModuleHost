using System.Globalization;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
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
                token = JToken.Parse(mi.Message);
            }
            catch
            {
            }

            if (token != null)
            {
                //string msg = token.ToString();
                using var sw = new StringWriter(CultureInfo.InvariantCulture);
                var jw = new JsonTextWriter(sw);
                jw.Formatting = Formatting.Indented;
                jw.Indentation = 4;
                token.WriteTo(jw);
                string msg = sw.ToString();

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
