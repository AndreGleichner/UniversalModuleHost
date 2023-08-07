#pragma once
#include <Windows.h>
#include <functional>
#include <string_view>
#include <format>
#include <thread>
#include "guid.h"

#pragma warning(push)
#pragma warning(disable : 4554)
#include <concurrentqueue/concurrentqueue.h>
#pragma warning(pop)

namespace ipc
{
namespace KnownSession
{
const DWORD Any {(DWORD)-1};
}

// A module may indicate that it wants to subscribe to all topics
static const Guid AllTopics {GUID_NULL};

static const Guid BrokerTopic {L"{92D627A3-6C62-4C5B-8477-484A34ED3B82}"};

static const Guid ShellExecTopic {L"{BEA684E7-697F-4201-844F-98224FA16D2F}"};

struct Topic final
{
    Guid  TopicId;
    DWORD Session = KnownSession::Any;

    Topic() = default;

    // You may pass ::WTSGetActiveConsoleSessionId()
    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-wtsgetactiveconsolesessionid
    Topic(const Guid& topicId, DWORD session = KnownSession::Any)
    {
        TopicId = topicId;
        Session = session;
    }

    std::wstring ToString() const
    {
        return std::format(L"{} @ {}", TopicId.ToUtf16(), (int)Session);
    }

    bool Equals(const Topic& rhs) const
    {
        return Session == rhs.Session && TopicId == rhs.TopicId;
    }
    bool operator==(const Topic& rhs) const
    {
        return Equals(rhs);
    }
};

struct MsgItem final
{
    std::string Msg;
    Topic       Topic;

    MsgItem() = default;

    MsgItem(const std::string_view msg, const ipc::Topic& topic) : Msg(msg), Topic(topic)
    {
    }
};

struct SubscriberContext final
{
    moodycamel::ConcurrentQueue<MsgItem> Queue;
    moodycamel::ProducerToken            Producer {Queue};

    std::jthread ProducerThread;

    static const int ConsumerCount = 1;
    std::jthread     ConsumerThreads[ConsumerCount];

    // Host processes require serialized messages until the host is fully initialized.
    void AllowAllConsumers()
    {
        allowedConsumers_ = ConsumerCount;
    }

    bool IsConsumerAllowed(int idx) const
    {
        return idx < allowedConsumers_;
    }

    void Restart()
    {
        if (ProducerThread.joinable())
            ProducerThread.join();
    }
    void Terminate()
    {
        if (terminated_)
            return;

        terminated_ = true;

        // This will run in context of one of the ConsumerThreads, so we must not join that thread here.
        // Instead it should get a request to stop and then let it run free
        // so it doesn't throw a deadlock assertion in dtor.

        // not launched
        if (ProducerThread.get_id() == std::jthread::id())
            return;

        ProducerThread.request_stop();
        ProducerThread.detach();

        for (int c = 0; c < ConsumerCount; ++c)
        {
            ConsumerThreads[c].request_stop();
            ConsumerThreads[c].detach();
        }

        // Drain the queue just to be nice...
        MsgItem mi;
        while (Queue.try_dequeue_from_producer(Producer, mi))
            ;
    }

    ~SubscriberContext()
    {
        Terminate();
    }

private:
    mutable bool terminated_       = false;
    int          allowedConsumers_ = 1;
};

class PublisherContext final
{
public:
    PublisherContext(bool autoStart);
    void StartProcessing(HANDLE out = nullptr, DWORD pid = 0, bool isOwnPid = false) noexcept;

    void Terminate()
    {
        if (terminated_)
            return;

        terminated_ = true;

        // not launched
        if (consumerThread_.get_id() == std::jthread::id())
            return;

        consumerThread_.request_stop();
        consumerThread_.detach();
    }

    HRESULT Enqueue(const std::string_view msg, const Topic& topic) noexcept;

private:
    mutable bool                         terminated_ = false;
    moodycamel::ConcurrentQueue<MsgItem> queue_;
    moodycamel::ProducerToken            producer_ {queue_};

    std::jthread consumerThread_;
};


// Usually be used with host
HRESULT Publish(const std::string_view msg, const Topic& topic) noexcept;

// Usually be used with broker
HRESULT Publish(PublisherContext& ctx, const std::string_view msg, const Topic& topic) noexcept;


using OnMessage = std::function<bool(const MsgItem& msgItem)>;
// Usually be used with host
HRESULT StartMsgQueueConsumption(SubscriberContext& ctx, OnMessage onMessage) noexcept;

// Usually be used with broker
HRESULT StartMsgQueueConsumption(
    HANDLE in, SubscriberContext& ctx, OnMessage onMessage, DWORD pid, bool isOwnPid = false) noexcept;

// Message sender passed to InitModule() in a module DLL so that it may send messages to its host.
typedef HRESULT(CALLBACK* Pub)(void* mod, PCSTR msg, const Guid* topicId, DWORD session);
}
