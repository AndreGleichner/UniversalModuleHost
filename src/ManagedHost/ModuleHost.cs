using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using ManagedModuleContract;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

namespace ManagedHost
{
    public class ModuleHost : IModuleHost, IDisposable
    {
        private readonly IConfigurationRoot _config;
        private readonly ILogger _logger;
        private readonly IModule _module;
        private bool _disposed;

        public ModuleHost(IModule module, IConfigurationRoot config, ILogger logger)
        {
            _module = module;
            _config = config;
            _logger = logger;
        }

        public bool DispatchEvent(ulong id, byte[] data)
        {
            if (_disposed)
                return false;

            return _module.DispatchEvent(id, data);
        }

        #region IModuleHost
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
        #endregion

        #region Dispose
        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (disposing)
                {
                    // dispose managed state (managed objects)
                    _module.Dispose();
                }

                // TODO: free unmanaged resources (unmanaged objects) and override finalizer
                // TODO: set large fields to null
                _disposed = true;
            }
        }

        // // TODO: override finalizer only if 'Dispose(bool disposing)' has code to free unmanaged resources
        // ~ModuleHost()
        // {
        //     // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
        //     Dispose(disposing: false);
        // }

        public void Dispose()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
        #endregion
    }
}
