#pragma once

#include <boost/sml.hpp>
namespace sml = boost::sml;

#include "spdlog_headers.h"

namespace sml_ext
{
struct logger
{
    auto stripped(const char* s)
    {
        const char* sp = strchr(s, ' ');
        if (sp)
        {
            ++sp;

            if (strstr(sp, "boost::") == sp)
            {
                const char* sp2 = strchr(sp, ' ');
                if (sp2)
                    sp = sp2 + 1;
            }
            return sp;
        }

        return s;
    }

    template <class T>
    auto name()
    {
        return stripped(sml::aux::get_type_name<T>());
    }

    template <class SM, class TEvent>
    void log_process_event(const TEvent&)
    {
        SPDLOG_DEBUG("[{}][process_event] %s", name<SM>(), name<TEvent>());
    }

    template <class SM, class TGuard, class TEvent>
    void log_guard(const TGuard&, const TEvent&, bool result)
    {
        SPDLOG_DEBUG(
            "[{}][guard] {} {} {}", name<SM>(), name<TGuard>(), name<TEvent>(), (result ? "[OK]" : "[Reject]"));
    }

    template <class SM, class TAction, class TEvent>
    void log_action(const TAction&, const TEvent&)
    {
        SPDLOG_DEBUG("[{}][action] {} {}", name<SM>(), name<TAction>(), name<TEvent>());
    }

    template <class SM, class TSrcState, class TDstState>
    void log_state_change(const TSrcState& src, const TDstState& dst)
    {
        SPDLOG_DEBUG("[{}][transition] {} -> {}", name<SM>(), stripped(src.c_str()), stripped(dst.c_str()));
    }
};
}
