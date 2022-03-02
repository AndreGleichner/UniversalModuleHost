#pragma once
#include <string>
#include <vector>

struct ChildProcessConfig final
{
    bool                            AllUsers;
    bool                            Wow64;
    bool                            HigherIntegrityLevel;
    bool                            Ui;
    const std::string               GroupName;
    const std::vector<std::wstring> Modules;
};
