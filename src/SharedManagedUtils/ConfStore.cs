using System;
using System.Collections.Generic;
using System.Text;

namespace SharedManagedUtils
{
    class ConfStore
    {
        public enum ECmd
        {
            Query,     // => Args = JSON Pointer (https://datatracker.ietf.org/doc/html/rfc6901)
            MergePatch // => Args = JSON MergePatch (https://datatracker.ietf.org/doc/html/rfc7386)
        };
        public ECmd Cmd { get; set; }
        public string Args { get; set; }
    }
}
