using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace IpcMonitor
{
    public record MessageItem(string Timestamp, string Message, string TopicId, string Session);
}
