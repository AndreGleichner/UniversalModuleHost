using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ManagedModuleContract
{
    public interface IModuleHost : IDisposable
    {
        bool SendMsgToHost(IModule module, string msg, Guid topicId, int session);
    }
}
