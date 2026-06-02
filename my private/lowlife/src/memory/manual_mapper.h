#pragma once
#include <Windows.h>
#include <vector>
#include <cstdint>

namespace memory::pe {
    /**
     * @brief Manually maps and loads a 64-bit PE DLL from a raw memory buffer.
     *        This executes completely in RAM, resolving relocations, imports, and TLS callbacks.
     * 
     * @param pe_data Pointer to the raw bytes of the PE DLL.
     * @param pe_size Size of the raw PE DLL buffer in bytes.
     * @return HMODULE Handle to the loaded base address on success, or nullptr on failure.
     */
    HMODULE manual_map(const uint8_t* pe_data, size_t pe_size);

    /**
     * @brief Safely unmaps and unloads a manually mapped PE DLL, calling its DllMain
     *        with DLL_PROCESS_DETACH and releasing its virtual memory allocation.
     * 
     * @param module_base The base address of the mapped module.
     * @return true on success, false if the module is invalid.
     */
    bool free_mapped_module(HMODULE module_base);
}
