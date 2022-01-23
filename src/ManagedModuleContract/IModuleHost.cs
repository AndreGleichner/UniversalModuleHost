using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ManagedModuleContract
{
    public interface IModuleHost
    {
        bool Subscribe(ulong id);
        bool Unsubscribe(ulong id);
        bool PublishEvent(ulong id, byte[] data);
    }
}
