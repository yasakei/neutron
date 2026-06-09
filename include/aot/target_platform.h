#ifndef NEUTRON_TARGET_PLATFORM_H
#define NEUTRON_TARGET_PLATFORM_H

namespace neutron {
namespace aot {

enum class TargetPlatform {
    NATIVE,
    LINUX_X64,
    LINUX_ARM64,
    MACOS_X64,
    MACOS_ARM64,
    WINDOWS_X64,
    WINDOWS_X86
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_TARGET_PLATFORM_H
