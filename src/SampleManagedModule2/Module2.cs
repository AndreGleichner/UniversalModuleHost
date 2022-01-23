using System;
using ManagedModuleContract;

namespace SampleManagedModule1
{
    public class Module2 : IModule
    {
        public bool Initialize(IModuleHost host)
        {
            throw new NotImplementedException();
        }

        public bool Uninitialize()
        {
            throw new NotImplementedException();
        }

        public bool DispatchEvent(ulong id, byte[] data)
        {
            throw new NotImplementedException();
        }
    }
}
