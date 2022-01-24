using System;

namespace ManagedModuleContract
{
    public interface IModule : IDisposable
    {
        bool Initialize(IModuleHost moduleHost);
        bool DispatchEvent(ulong id, byte[] data);
    }
}
