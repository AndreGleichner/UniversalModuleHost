using System;

namespace ManagedModuleContract
{
    public interface IModule : IDisposable
    {
        bool Initialize(IModuleHost moduleHost);
        bool OnMessageFromHost(string msg, Guid service, int session);
    }
}
