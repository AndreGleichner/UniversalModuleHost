using System;
using System.Collections.Generic;
using System.Text;

namespace SharedManagedUtils
{
    class ModuleMeta
    {
        public int Pid { get; set; }
        public string Name { get; set; }
        public List<string> Services { get; set; } = new List<string>();
    }
}
