using System;
using System.Collections.Generic;
using System.Text;

namespace SharedManagedUtils
{
    partial class Ipc
    {
        public const string ConfStoreTopic = "{8583CDC9-DB92-45BE-90CE-4D3AA4CD14F5}";
        public class ConfStore
        {
            public enum ECmd
            {
                Query, // => Args = Module name
                Update // => Args = JSON MergePatch (https://datatracker.ietf.org/doc/html/rfc7386)
            };
            public ECmd Cmd { get; set; }
            public string Args { get; set; }
        }

        public const string ConfTopic = "{8ED3A4D7-7C78-4B88-A547-A4D87A9DDC35}";
        public struct Conf
        {
            string Val { get; set; }
        }
    }
}
