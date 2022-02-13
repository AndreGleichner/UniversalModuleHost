#pragma once
#include <filesystem>

namespace Permission
{
bool IsDirectoryOnlyWriteableByElevatedUser(const std::filesystem::path& path);
}
