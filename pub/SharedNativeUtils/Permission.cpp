#include "pch.h"
#include "Permission.h"

namespace Permission
{
bool IsDirectoryOnlyWriteableByElevatedUser(const std::filesystem::path& path)
{
    return true;
}
}
