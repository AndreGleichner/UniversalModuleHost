using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using ManagedModuleContract;

namespace ManagedHost
{
    public class ModuleHost : IModuleHost
    {
        public bool Subscribe(ulong stateName)
        {
            throw new NotImplementedException();
        }

        public bool Unsubscribe(ulong stateName)
        {
            throw new NotImplementedException();
        }

        public bool PublishEvent(ulong stateName, byte[] data)
        {
            throw new NotImplementedException();
        }
    }
}
