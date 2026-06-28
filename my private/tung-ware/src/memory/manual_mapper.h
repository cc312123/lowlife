#pragma once
#include <Windows.h>
#include <vector>
#include <cstdint>

namespace memory::pe {
    
    HMODULE manual_map(const uint8_t* pe_data, size_t pe_size);

    
    bool free_mapped_module(HMODULE module_base);
}
