using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using ManagedModuleContract;
using McMaster.NETCore.Plugins;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

namespace ManagedHost
{
    public class ModuleHost : IModuleHost, IDisposable
    {
        private readonly IConfigurationRoot _config;
        private readonly ILogger _logger;

        record ModuleDetail(PluginLoader Loader, IModule Module);
        private readonly List<ModuleDetail> _modules = new();
        private bool _disposed;

        public ModuleHost(IConfigurationRoot config, ILogger logger)
        {
            _config = config;
            _logger = logger;
        }

        public bool LoadModule(string path)
        {
            if (!File.Exists(path))
            {
                return false;
            }

            var loader = PluginLoader.CreateFromAssemblyFile(
                path,
                sharedTypes: new[] { typeof(IModule), typeof(IModuleHost), typeof(IConfigurationRoot), typeof(ILogger) });

            //foreach (var moduleType in loader
            //        .LoadDefaultAssembly()
            //        .GetTypes()
            //        .Where(t => typeof(IModule).IsAssignableFrom(t) && !t.IsAbstract))

            var defAss = loader.LoadDefaultAssembly();
            Type[] types;

            try
            {
                types = defAss.GetTypes();
            }
            catch (Exception ex)
            {
                types = null;
            }

            foreach (var moduleType in types
                .Where(t => typeof(IModule).IsAssignableFrom(t) && !t.IsAbstract))

            {
                var module = Activator.CreateInstance(moduleType, _config, _logger) as IModule;
                if (module != null)
                {
                    _logger.LogInformation($"Initialize module {path}");

                    if (module.Initialize(this))
                    {
                        _modules.Add(new ModuleDetail(loader, module));
                    }
                    else
                    {
                        module.Dispose();
                        _logger.LogError($"Failed to initialize module {path}");
                        return false;
                    }
                }
                else
                {
                    _logger.LogError($"Failed to activate module {path}");
                    return false;
                }
            }

            return true;
        }

        public bool UnloadModule(string path)
        {
            return true;
        }

        public bool SendMsgToModule(string msg, Guid service, int session)
        {
            if (_disposed)
                return false;

            foreach (var module in _modules)
            {
                module.Module.OnMessageFromHost(msg, service, session);
            }
            return true;
        }

        #region IModuleHost
        public bool SendMsgToHost(IModule module, string msg, Guid service, int session)
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
                    foreach (var module in _modules)
                    {
                        module.Loader.Dispose();
                        module.Module.Dispose();
                    }
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
