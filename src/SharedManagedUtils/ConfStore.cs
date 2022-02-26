using System;
using System.Collections.Generic;
using System.Text;

namespace SharedManagedUtils
{
    class ConfStore
    {
        public enum ECmd
        {
            Query, // => Args = Module name
            Update // => Args = JSON MergePatch (https://datatracker.ietf.org/doc/html/rfc7386)
        };
        public ECmd Cmd { get; set; }
        public string Args { get; set; }
    }
}
