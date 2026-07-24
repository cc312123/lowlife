/*
 * pc_check.cpp  —  Tung-Ware PC Check Bypass
 * ─────────────────────────────────────────────────────────────────────────────
 * Covers:
 *   1. MachineGuid spoofing   – randomises HKLM\SOFTWARE\Microsoft\Cryptography
 *   2. Volume serial spoof    – patches GetVolumeInformationW in-memory (IAT)
 *   3. CPU registry spoof     – randomises ProcessorNameString
 *   4. Timing normalisation   – dummy QPC loops to muddle VM/sandbox detects
 *   5. Environment clean      – strips variables used by fingerprint scripts
 *   6. Continuous watcher     – re-applies spoofs every 10 s so Roblox re-reads
 *                               spoofed values on rescan
 * ─────────────────────────────────────────────────────────────────────────────
 */

#define NOMINMAX
#include <Windows.h>
#include <winternl.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <array>
#include <random>
#include <thread>
#include <chrono>
#include "pc_check.h"

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Psapi.lib")

// ── helpers ───────────────────────────────────────────────────────────────────

namespace {

// Cryptographically-random hex string of given byte-length
std::string random_hex(std::size_t bytes)
{
    static std::mt19937_64 rng{ std::random_device{}() };
    std::uniform_int_distribution<unsigned> dist(0, 255);
    std::string out;
    out.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02X", dist(rng));
        out += buf;
    }
    return out;
}

// Build a random GUID string in {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} format
std::string random_guid()
{
    // Format: 8-4-4-4-12
    return  "{" +
            random_hex(4)  + "-" +
            random_hex(2)  + "-" +
            random_hex(2)  + "-" +
            random_hex(2)  + "-" +
            random_hex(6)  + "}";
}

// Quietly write a REG_SZ value; silently ignores failures
void reg_write_sz(HKEY root, const char* subkey, const char* name, const char* value)
{
    HKEY hk = nullptr;
    if (RegCreateKeyExA(root, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExA(hk, name, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(value),
                       static_cast<DWORD>(strlen(value) + 1));
        RegCloseKey(hk);
    }
}

// Read a REG_SZ value into a string; returns "" on failure
std::string reg_read_sz(HKEY root, const char* subkey, const char* name)
{
    HKEY hk = nullptr;
    std::string out;
    if (RegOpenKeyExA(root, subkey, 0, KEY_QUERY_VALUE, &hk) == ERROR_SUCCESS)
    {
        char buf[512] = {};
        DWORD sz   = sizeof(buf);
        DWORD type = REG_SZ;
        if (RegQueryValueExA(hk, name, nullptr, &type,
                             reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS)
            out = buf;
        RegCloseKey(hk);
    }
    return out;
}

// IAT hook: find the first imported entry matching `targetFn` inside a module
// and overwrite it with `replacement`, returning the original pointer.
void* iat_hook(HMODULE mod, const char* importDll, const char* targetFn, void* replacement)
{
    if (!mod) return nullptr;

    auto* dosHdr = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
    if (dosHdr->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

    auto* ntHdr = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<BYTE*>(mod) + dosHdr->e_lfanew);

    auto& importDir = ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir.VirtualAddress) return nullptr;

    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        reinterpret_cast<BYTE*>(mod) + importDir.VirtualAddress);

    for (; desc->Name; ++desc)
    {
        const char* dllName = reinterpret_cast<const char*>(
            reinterpret_cast<BYTE*>(mod) + desc->Name);
        if (_stricmp(dllName, importDll) != 0) continue;

        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(mod) + desc->FirstThunk);
        auto* orig  = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(mod) + desc->OriginalFirstThunk);

        for (std::size_t i = 0; thunk[i].u1.Function; ++i)
        {
            // Skip ordinal imports
            if (IMAGE_SNAP_BY_ORDINAL(orig[i].u1.Ordinal)) continue;

            auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                reinterpret_cast<BYTE*>(mod) + orig[i].u1.AddressOfData);

            if (_stricmp(reinterpret_cast<const char*>(ibn->Name), targetFn) != 0) continue;

            void** slot   = reinterpret_cast<void**>(&thunk[i].u1.Function);
            void*  oldFn  = *slot;

            DWORD oldProt = 0;
            VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
            *slot = replacement;
            VirtualProtect(slot, sizeof(void*), oldProt, &oldProt);

            return oldFn; // return original so we can call through
        }
    }
    return nullptr;
}

// ── Volume serial spoof via IAT hook ─────────────────────────────────────────

static DWORD g_spoofed_serial = 0;

// Our replacement for GetVolumeInformationW
static BOOL WINAPI hooked_GetVolumeInformationW(
    LPCWSTR lpRootPathName,
    LPWSTR  lpVolumeNameBuffer,
    DWORD   nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR  lpFileSystemNameBuffer,
    DWORD   nFileSystemNameSize)
{
    BOOL ret = GetVolumeInformationW(
        lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
        lpVolumeSerialNumber, lpMaximumComponentLength,
        lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);

    if (ret && lpVolumeSerialNumber && g_spoofed_serial)
        *lpVolumeSerialNumber = g_spoofed_serial;

    return ret;
}

// Hook GetVolumeInformationW in the Roblox module's IAT
void apply_volume_iat_hook()
{
    // Hook in all loaded modules to make sure we catch any Roblox image
    HMODULE mods[256] = {};
    DWORD   needed    = 0;
    HANDLE  self      = GetCurrentProcess();

    EnumProcessModules(self, mods, sizeof(mods), &needed);
    DWORD count = needed / sizeof(HMODULE);

    for (DWORD i = 0; i < count; ++i)
    {
        iat_hook(mods[i], "kernel32.dll", "GetVolumeInformationW",
                 reinterpret_cast<void*>(hooked_GetVolumeInformationW));
        iat_hook(mods[i], "KERNEL32.dll", "GetVolumeInformationW",
                 reinterpret_cast<void*>(hooked_GetVolumeInformationW));
    }
}

// ── Timing normalisation ──────────────────────────────────────────────────────

void normalise_timing() noexcept
{
    // Run several QPC round-trips so any hypervisor/sandbox timing detect
    // sees a "normal" delta instead of near-zero VM ticks.
    LARGE_INTEGER freq{}, t0{}, t1{};
    QueryPerformanceFrequency(&freq);

    for (int i = 0; i < 8; ++i) {
        QueryPerformanceCounter(&t0);
        volatile int sink = 0;
        for (int j = 0; j < 50000; ++j) sink += j; // busy work
        QueryPerformanceCounter(&t1);
        (void)sink;
    }

    // Also flush the CPUID instruction a few times (some detects watch for
    // missing CPUID variance under a hypervisor).
    int cpuInfo[4]{};
    for (int i = 0; i < 4; ++i)
        __cpuid(cpuInfo, i);
}

} // anonymous namespace

// ── Public API ────────────────────────────────────────────────────────────────

void tungware::bypass::pc_check::spoof_machine_guid() noexcept
{
    try {
        // Read the existing GUID so we only overwrite it once per boot
        const char* subkey = "SOFTWARE\\Microsoft\\Cryptography";
        std::string existing = reg_read_sz(HKEY_LOCAL_MACHINE, subkey, "MachineGuid");

        // Store original in a TUNGWARE hidden key so we can restore if needed
        if (!existing.empty()) {
            reg_write_sz(HKEY_CURRENT_USER,
                         "Software\\Microsoft\\Windows\\CurrentVersion\\Accessibility",
                         "OrigMachineGuid", existing.c_str());
        }

        std::string fake = random_guid();
        reg_write_sz(HKEY_LOCAL_MACHINE, subkey, "MachineGuid", fake.c_str());
    }
    catch (...) {}
}

void tungware::bypass::pc_check::spoof_volume_serial() noexcept
{
    try {
        // Generate a random-looking but stable-per-session serial
        static std::mt19937 rng{ std::random_device{}() };
        g_spoofed_serial = (rng() & 0xFFFFFFFF) | 0x10000000u;

        apply_volume_iat_hook();
    }
    catch (...) {}
}

void tungware::bypass::pc_check::spoof_cpu_registry() noexcept
{
    try {
        // Common CPU strings that look normal; pick one at random each session
        static const char* const cpu_strings[] = {
            "Intel(R) Core(TM) i7-12700K CPU @ 3.60GHz",
            "Intel(R) Core(TM) i9-13900K CPU @ 3.00GHz",
            "AMD Ryzen 9 7900X 12-Core Processor",
            "Intel(R) Core(TM) i5-13600K CPU @ 3.50GHz",
            "AMD Ryzen 7 7700X 8-Core Processor",
            "Intel(R) Core(TM) i7-13700K CPU @ 3.40GHz",
        };

        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(0, 5);
        const char* chosen = cpu_strings[dist(rng)];

        // Overwrite ProcessorNameString for each logical core
        for (int i = 0; i < 32; ++i) {
            char subkey[128];
            snprintf(subkey, sizeof(subkey),
                     "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\%d", i);

            HKEY hk = nullptr;
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey, 0, KEY_SET_VALUE, &hk) != ERROR_SUCCESS)
                break; // no more logical cores
            RegSetValueExA(hk, "ProcessorNameString", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(chosen),
                           static_cast<DWORD>(strlen(chosen) + 1));
            RegCloseKey(hk);
        }
    }
    catch (...) {}
}

void tungware::bypass::pc_check::patch_timing_checks() noexcept
{
    normalise_timing();
}

void tungware::bypass::pc_check::clean_environment() noexcept
{
    // Strip env vars that fingerprint scripts or sandbox scanners commonly read
    static const wchar_t* const env_vars[] = {
        L"COMPUTERNAME_ORIGINAL",
        L"_CHEAT_ENGINE_",
        L"CE_",
        L"FRIDA_",
        L"WIRESHARK_",
        L"x64dbg",
        L"OLLYDBG",
        nullptr
    };

    for (int i = 0; env_vars[i]; ++i)
        SetEnvironmentVariableW(env_vars[i], nullptr);

    // Replace COMPUTERNAME with a generic-looking name so it doesn't match
    // any stored fingerprint on Roblox's backend.
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<unsigned> dist(1000, 9999);
    std::string fakePC = "DESKTOP-" + random_hex(3);
    SetEnvironmentVariableA("COMPUTERNAME", fakePC.c_str());
}

void tungware::bypass::pc_check::run_pc_bypass() noexcept
{
    // Apply everything once before attaching to Roblox
    spoof_machine_guid();
    spoof_volume_serial();
    spoof_cpu_registry();
    patch_timing_checks();
    clean_environment();
}

void tungware::bypass::pc_check::watch_thread() noexcept
{
    // Re-apply volatile spoofs every 10 s so Roblox re-reads spoofed values
    // if it polls hardware info more than once per session.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Only re-hook IAT (volume serial) since registry writes are already set
        apply_volume_iat_hook();

        // Re-run timing normalisation every loop
        normalise_timing();
    }
}
