namespace SharedManagedUtils
{
    partial class Ipc
    {
        public const string ManagedHostTopic = "{7924FE60-C967-449C-BA5D-2EBAA7D16024}";
        public class HostCmd
        {
            public enum ECmd
            {
                Terminate,
                CtrlModule
            };
            public ECmd Cmd { get; set; }
            public string Args { get; set; } // e.g. CtrlModule => HostCtrlModuleArgs as JSON
        }

        // see HostCmd.Args
        public class HostCtrlModuleArgs
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
}
