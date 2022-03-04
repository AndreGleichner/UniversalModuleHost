#pragma once
#include <filesystem>
#include <sddl.h>

#include <wil/win32_helpers.h>

namespace env
{
inline std::filesystem::path PrivateDataDir(const std::wstring_view subDir = L"")
{
    auto                  programData = wil::GetEnvironmentVariableW(L"ProgramData");
    std::filesystem::path dir(programData.get());
    auto                  pdd = dir / L"UniversalModuleHost";

    PSECURITY_DESCRIPTOR sd     = nullptr;
    auto                 freeSD = wil::scope_exit([&] {
        if (sd)
            ::LocalFree(sd);
    });

    // DWORD secInfo = DACL_SECURITY_INFORMATION /*| GROUP_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION |
    //                 OWNER_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION*/
    //     ;
    // DWORD len = 0;
    // BOOL  res = ::GetFileSecurityW(pdd.c_str(), secInfo, sd, 0, &len);
    // sd        = (PSECURITY_DESCRIPTOR)::LocalAlloc(LPTR, len);
    // res       = ::GetFileSecurityW(pdd.c_str(), secInfo, sd, len, &len);

    // PWSTR str    = nullptr;
    // ULONG strLen = 0;
    // res          = ::ConvertSecurityDescriptorToStringSecurityDescriptorW(sd, SDDL_REVISION_1, secInfo, &str,
    // &strLen);
    //  L"D:AI(A;OICIID;FA;;;SY)(A;OICIID;FA;;;BA)(A;OICIIOID;GA;;;CO)(A;OICIID;0x1200a9;;;BU)(A;CIID;DCLCRPCR;;;BU)"

    //PCWSTR sds = L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;CO)";
    PCWSTR sds = L"D:AI(A;OICIID;FA;;;SY)(A;OICIID;FA;;;BA)";

    FAIL_FAST_IF_WIN32_BOOL_FALSE(::ConvertStringSecurityDescriptorToSecurityDescriptorW(
        sds, SDDL_REVISION_1, &sd, nullptr));

    std::filesystem::create_directories(pdd);

    BOOL res = ::SetFileSecurityW(pdd.c_str(), DACL_SECURITY_INFORMATION, sd);

    if (!subDir.empty())
    {
        pdd /= subDir;
        std::filesystem::create_directories(pdd);
    }

    return pdd;
}
}
