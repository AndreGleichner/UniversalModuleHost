#pragma once
#include <string>
#include <string_view>
#include <unordered_set>
#include <filesystem>

#include <absl/hash/hash.h>
#include <wil/result.h>

#include "ipc.h"
#include "guid.h"

class ModuleBase
{
public:
    ModuleBase(std::unordered_set<Guid, absl::Hash<Guid>> services) : services_(services)
    {
    }
    virtual ~ModuleBase()
    {
        (void)OnTerminate();
    }

    HRESULT Initialize(void* mod, ipc::SendMsg sendMsg, ipc::SendDiag sendDiag) noexcept;
    HRESULT Terminate() noexcept;
    HRESULT HandleMessage(std::string_view msg, const ipc::Target& target) noexcept;

    static std::filesystem::path PathFor(std::wstring_view moduleName, bool bitnessSpecific);

protected:
    virtual HRESULT OnInitialize() noexcept
    {
        return S_OK;
    }

    virtual HRESULT OnTerminate() noexcept
    {
        return S_OK;
    }

    virtual HRESULT OnMessage(std::string_view msg, const ipc::Target& target) noexcept
    {
        return S_OK;
    }

    HRESULT SendMsg(std::string_view msg, const ipc::Target& target) noexcept;
    HRESULT SendDiag(std::string_view msg) noexcept;

private:
    void*         mod_      = nullptr;
    ipc::SendMsg  sendMsg_  = nullptr;
    ipc::SendDiag sendDiag_ = nullptr;

    std::unordered_set<Guid, absl::Hash<Guid>> services_;
};
