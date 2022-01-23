#pragma once

#include <thread>

namespace ModuleHostApp
{
bool StartAsync();
}

class ModuleHostThread final : public std::thread
{
public:
    ModuleHostThread() : std::thread()
    {
    }
};
