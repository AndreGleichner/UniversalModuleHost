using System;

namespace ManagedModuleContract
{
    public interface IModule
    {
        bool Initialize(IModuleHost host);
        bool Uninitialize();
        bool DispatchEvent(ulong id, byte[] data);
    }
}
