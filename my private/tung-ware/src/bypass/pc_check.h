#pragma once
#include <Windows.h>
#include <string>
#include <cstdint>

namespace tungware::bypass::pc_check
{
    // Spoof HKLM MachineGuid so fingerprinting reads a randomised value
    void spoof_machine_guid() noexcept;

    // Spoof the volume serial number returned by GetVolumeInformation
    void spoof_volume_serial() noexcept;

    // Randomise the CPUID brand string in the registry
    void spoof_cpu_registry() noexcept;

    // Patch NtQuerySystemInformation timing side-channel
    void patch_timing_checks() noexcept;

    // Flush suspicious environment variables used for fingerprinting
    void clean_environment() noexcept;

    // Main entry – call once before attaching to Roblox
    void run_pc_bypass() noexcept;

    // Continuous watcher thread – call in a detached thread
    void watch_thread() noexcept;
}
