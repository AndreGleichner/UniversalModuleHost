#pragma once
#include "UmhProcess.h"

class ThreadnameFlagFormatter : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override
    {
        std::wstring threadDetails;
        auto         threadname = Process::ThreadName();
        if (threadname.empty())
            threadDetails = std::format(L"{}", ::GetCurrentThreadId());
        else
            threadDetails = std::format(L"{}='{}'", ::GetCurrentThreadId(), threadname);
        spdlog::memory_buf_t buf;
        spdlog::details::os::wstr_to_utf8buf(threadDetails, buf);
        dest.append(buf);
    }

    std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return spdlog::details::make_unique<ThreadnameFlagFormatter>();
    }
};
class ProcessnameFlagFormatter : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override
    {
        spdlog::memory_buf_t buf;
        spdlog::details::os::wstr_to_utf8buf(
            std::format(L"{}='{}'", ::GetProcessId(::GetCurrentProcess()), Process::Name()), buf);
        dest.append(buf);
    }

    std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return spdlog::details::make_unique<ProcessnameFlagFormatter>();
    }
};
