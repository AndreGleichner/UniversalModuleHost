#include "pch.h"
#include <vector>
#include <thread>
#include "ipc.h"
#include "TMProcess.h"
#include "env.h"

namespace ipc
{
HRESULT PublisherContext::Enqueue(const std::string_view msg, const Topic& topic) noexcept
try
{
    // Needs sequential access to the pipe to not interleave messages from multiple threads.
    queue_.enqueue(producer_, std::move(MsgItem(msg, topic)));
    return S_OK;
}
CATCH_RETURN();

PublisherContext::PublisherContext(bool autoStart)
{
    if (autoStart)
    {
        StartProcessing(::GetStdHandle(STD_OUTPUT_HANDLE), ::GetCurrentProcessId(), true);
    }
}

void PublisherContext::StartProcessing(HANDLE out /*= nullptr*/, DWORD pid /*= 0*/, bool isOwnPid /*= false*/) noexcept
{
    HANDLE outDup = nullptr;

    FAIL_FAST_IF_WIN32_BOOL_FALSE(
        ::DuplicateHandle(::GetCurrentProcess(), out, ::GetCurrentProcess(), &outDup, 0, FALSE, DUPLICATE_SAME_ACCESS));

    consumerThread_ = std::jthread([this, outDup, pid, isOwnPid](std::stop_token stoken) {
        const std::wstring tname = std::format(L"TM-PublisherContext-{}({})", pid, isOwnPid ? L"own" : L"other");
        Process::SetThreadName(tname.c_str());

        MsgItem mi;
        while (1)
        {
            if (queue_.try_dequeue_from_producer(producer_, mi))
            {
                // env::WaitForDebugger();

                size_t               size = 4 /*framing length*/ + sizeof(mi.Topic) + mi.Msg.size() + 1 /*zero-term*/;
                std::vector<uint8_t> buf;
                buf.resize(size);

                size_t pos = 0;

                // store count of bytes following the length prefix
                *(DWORD*)&buf[0] = (DWORD)size - 4;
                pos += 4;

                // store topic
                *(Topic*)&buf[pos] = mi.Topic;
                pos += sizeof(mi.Topic);

                // store message
                memcpy(&buf[pos], mi.Msg.data(), mi.Msg.size() + 1);

                DWORD written = 0;

                LOG_IF_WIN32_BOOL_FALSE(::WriteFile(outDup, buf.data(), (DWORD)size, &written, nullptr));

                LOG_HR_IF_MSG(
                    E_FAIL, written != (DWORD)size, "ipc::PublisherContext::StartProcessing failed to send all bytes");
            }
            else if (stoken.stop_requested())
            {
                break;
            }
            else
            {
                ::Sleep(1);
            }
        }
    });
}

HRESULT Publish(const std::string_view msg, const Topic& topic) noexcept
{
    static PublisherContext publisherContext(true);
    return Publish(publisherContext, msg, topic);
};

HRESULT Publish(PublisherContext& ctx, const std::string_view msg, const Topic& topic) noexcept
try
{
    RETURN_HR_IF_MSG(E_FAIL, topic.TopicId == AllTopics, "Can't publish msg to 'All'");

    RETURN_IF_FAILED(ctx.Enqueue(msg, topic));

    return S_OK;
}
CATCH_RETURN();

HRESULT StartMsgQueueConsumption(SubscriberContext& ctx, OnMessage onMessage) noexcept
{
    return StartMsgQueueConsumption(::GetStdHandle(STD_INPUT_HANDLE), ctx, onMessage, ::GetCurrentProcessId(), true);
}

HRESULT StartMsgQueueConsumption(
    HANDLE in, SubscriberContext& ctx, OnMessage onMessage, DWORD pid, bool isOwnPid /*= false*/) noexcept
try
{
    /*if (!Process::IsBroker())
    {
        env::WaitForDebugger();
    }*/

    HANDLE inDup = nullptr;

    FAIL_FAST_IF_WIN32_BOOL_FALSE(
        ::DuplicateHandle(::GetCurrentProcess(), in, ::GetCurrentProcess(), &inDup, 0, FALSE, DUPLICATE_SAME_ACCESS));

    for (int c = 0; c < ctx.ConsumerCount; ++c)
    {
        ctx.ConsumerThreads[c] = std::jthread([&ctx, onMessage, c, pid, isOwnPid](std::stop_token stoken) {
            const std::wstring tname = std::format(L"TM-IpcReaderCons{}-{}({})", c, pid, isOwnPid ? L"own" : L"other");
            Process::SetThreadName(tname.c_str());

            MsgItem mi;
            while (!stoken.stop_requested())
            {
                if (ctx.IsConsumerAllowed(c) && ctx.Queue.try_dequeue_from_producer(ctx.Producer, mi))
                {
                    // env::WaitForDebugger();

                    if (onMessage(mi))
                        return;
                }
                else
                {
                    ::Sleep(1);
                }
            }
        });
    }

    ctx.ProducerThread = std::jthread([inDup, &ctx, pid, isOwnPid](std::stop_token stoken) {
        const std::wstring tname = std::format(L"TM-IpcReaderProd-{}({})", pid, isOwnPid ? L"own" : L"other");
        Process::SetThreadName(tname.c_str());

        DWORD size = 0, read = 0;
        while (!stoken.stop_requested() && ::ReadFile(inDup, &size, 4, &read, nullptr) && read == 4 &&
               size > sizeof(Topic))
        {
            std::vector<uint8_t> buf;
            buf.resize(size);

            if (!stoken.stop_requested() && ::ReadFile(inDup, buf.data(), size, &read, nullptr) && read == size)
            {
                // env::WaitForDebugger();

                MsgItem mi({(const char*)&buf[sizeof(Topic)], size - sizeof(Topic) - 1},
                    {*(Guid*)&buf[0], *(DWORD*)&buf[sizeof(Guid)]});

                (void)ctx.Queue.enqueue(ctx.Producer, std::move(mi));
            }
        }

        // env::WaitForDebugger();
    });
    return S_OK;
}
CATCH_RETURN();
}
