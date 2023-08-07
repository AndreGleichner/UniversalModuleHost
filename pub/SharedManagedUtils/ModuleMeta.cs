using System;
using System.Collections.Generic;
using System.Text;

namespace SharedManagedUtils
{
    partial class Ipc
    {
        public const string ModuleMetaTopic = "{6E6A094C-839F-4EAF-BD22-08CB9E1A318F}";
        public class ModuleMeta
        {
            public int Pid { get; set; }
            public string Name { get; set; }
            public List<string> TopicIds { get; set; } = new List<string>();
        }
    }
}
