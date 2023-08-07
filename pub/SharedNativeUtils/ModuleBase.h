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
    ModuleBase(std::unordered_set<Guid, absl::Hash<Guid>> topicIds) : topicIds_(topicIds)
    {
    }
    virtual ~ModuleBase()
    {
        (void)OnTerminate();
    }

    HRESULT Initialize(void* mod, ipc::Pub sendMsg) noexcept;
    HRESULT Terminate() noexcept;
    HRESULT HandleMessage(std::string_view msg, const ipc::Topic& topic) noexcept;

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

    virtual HRESULT OnMessage(std::string_view msg, const ipc::Topic& topic) noexcept
    {
        return S_OK;
    }

    HRESULT Publish(std::string_view msg, const ipc::Topic& topic) noexcept;
    HRESULT Diag(std::string_view msg) noexcept;

private:
    void*    mod_     = nullptr;
    ipc::Pub sendMsg_ = nullptr;

    std::unordered_set<Guid, absl::Hash<Guid>> topicIds_;
};
