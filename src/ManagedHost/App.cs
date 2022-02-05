using System;
using System.Runtime.InteropServices;
using System.Threading;
using Microsoft.Extensions.Configuration;
using McMaster.NETCore.Plugins;
using System.Collections.Generic;
using System.IO;
using ManagedModuleContract;
using System.Linq;
using Microsoft.Extensions.Logging;

namespace ManagedHost
{
    internal class App : IDisposable
    {
        private bool _disposed;
        private readonly IConfigurationRoot _config;
        private readonly ILogger _logger;
        private readonly List<ModuleHost> _moduleHosts = new();

        record ModuleDetail(PluginLoader Loader, string Path);
        List<ModuleDetail> _moduleDetails = new();
        public App(IConfigurationRoot config, ILogger logger)
        {
            _config = config;
            _logger = logger;
        }

        public int Run(string[] args)
        {
            _logger.LogInformation("-> App.Run()");

            // create module loaders
            string modulesDir = Path.Combine(AppContext.BaseDirectory, "modules");
            foreach (string dir in Directory.GetDirectories(modulesDir))
            {
                string dirName = Path.GetFileName(dir);
                string moduleDll = Path.Combine(dir, dirName + ".dll");
                if (File.Exists(moduleDll))
                {
                    var loader = PluginLoader.CreateFromAssemblyFile(
                        moduleDll,
                        sharedTypes: new[] { typeof(IModule), typeof(IConfigurationRoot), typeof(ILogger) });
                    _moduleDetails.Add(new ModuleDetail(loader, moduleDll));
                }
            }

            // Create an instance of module types
            foreach (var moduleDetail in _moduleDetails)
            {
                foreach (var moduleType in moduleDetail.Loader
                    .LoadDefaultAssembly()
                    .GetTypes()
                    .Where(t => typeof(IModule).IsAssignableFrom(t) && !t.IsAbstract))
                {
                    var module = Activator.CreateInstance(moduleType, _config, _logger) as IModule;
                    if (module != null)
                    {
                        _logger.LogInformation($"Initialize module {moduleDetail.Path}");

                        var moduleHost = new ModuleHost(module, _config, _logger);
                        if (module.Initialize(moduleHost))
                        {
                            _moduleHosts.Add(moduleHost);
                        }
                        else
                        {
                            moduleHost.Dispose();
                            _logger.LogError($"Failed to initialize module {moduleDetail.Path}");
                        }
                    }
                    else
                    {
                        _logger.LogError($"Failed to activate module {moduleDetail.Path}");
                    }
                }
            }

            foreach (var moduleHost in _moduleHosts)
            {
                moduleHost.DispatchEvent(42, Array.Empty<byte>());
            }


            //_logger.LogInformation($"Having {args.Length} args");
            //int n = 0;
            //foreach (string arg in args)
            //{
            //    _logger.LogInformation($"Args {n++}: '{arg}'");
            //}
            //for (int i = 0; i < 10; ++i)
            //{
            //    int res = NativeMethods.OnProgressFromManaged(i);
            //    _logger.LogInformation($"OnProgressFromManaged({i}) res={res}");
            //    Thread.Sleep(TimeSpan.FromSeconds(1));
            //}

            return 0;
        }

        #region Dispose
        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (disposing)
                {
                    // dispose managed state (managed objects)
                    foreach (var moduleDetail in _moduleDetails)
                        moduleDetail.Loader.Dispose();
                    foreach (var moduleHost in _moduleHosts)
                        moduleHost.Dispose();

                    _moduleHosts.Clear();
                }

                // TODO: free unmanaged resources (unmanaged objects) and override finalizer
                // TODO: set large fields to null
                _disposed = true;
            }
        }

        // // TODO: override finalizer only if 'Dispose(bool disposing)' has code to free unmanaged resources
        // ~App()
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

    internal static class NativeMethods
    {
        #region OnProgress
        [DllImport("UniversalModuleHost64.exe", EntryPoint = "OnProgressFromManaged")]
        static extern int OnProgressFromManaged64(int progress);

        [DllImport("UniversalModuleHost32.exe", EntryPoint = "OnProgressFromManaged")]
        static extern int OnProgressFromManaged32(int progress);

        public static int OnProgressFromManaged(int progress)
        {
            if (IntPtr.Size == 4)
                return OnProgressFromManaged32(progress);
            else
                return OnProgressFromManaged64(progress);
        }
        #endregion

        #region OnLog
        [DllImport("UniversalModuleHost64.exe", EntryPoint = "OnLog", CharSet = CharSet.Unicode)]
        static extern int OnLog64(int level, string message);

        [DllImport("UniversalModuleHost32.exe", EntryPoint = "OnLog", CharSet = CharSet.Unicode)]
        static extern int OnLog32(int level, string message);

        public static int OnLog(int level, string message)
        {
            if (IntPtr.Size == 4)
                return OnLog32(level, message);
            else
                return OnLog64(level, message);
        }
        #endregion
    }
}
