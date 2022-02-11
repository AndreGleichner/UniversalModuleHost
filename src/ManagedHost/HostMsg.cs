using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ManagedHost
{
    class HostCmdMsg
    {
        public enum ECmd
        {
            Terminate,
            CtrlModule
        };
        public ECmd Cmd { get; set; }
        public string Args { get; set; } // e.g. CtrlModule => HostCtrlModuleArgs as JSON
    }

    class HostCtrlModuleArgs
    {
        public enum ECmd
        {
            Load,
            Unload
        };
        public ECmd Cmd { get; set; }
        public string Module { get; set; }
    }
}
