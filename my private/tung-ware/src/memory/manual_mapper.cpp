#include "manual_mapper.h"
#include <cstdio>
#include <cstring>

namespace memory::pe {

    HMODULE manual_map(const uint8_t* pe_data, size_t pe_size) {
        if (!pe_data || pe_size < sizeof(IMAGE_DOS_HEADER)) {
            printf("[ManualMapper] Error: Invalid PE data pointer or size.\n");
            return nullptr;
        }

        
        auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(pe_data);
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
            printf("[ManualMapper] Error: Invalid DOS Signature (MZ missing).\n");
            return nullptr;
        }

        
        if (static_cast<size_t>(dos_header->e_lfanew) + sizeof(IMAGE_NT_HEADERS) > pe_size) {
            printf("[ManualMapper] Error: Buffer size too small to contain NT headers.\n");
            return nullptr;
        }

        auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(pe_data + dos_header->e_lfanew);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
            printf("[ManualMapper] Error: Invalid NT Headers Signature (PE missing).\n");
            return nullptr;
        }

        
        if (nt_headers->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
            printf("[ManualMapper] Error: Only 64-bit PEs are supported for x64 architecture.\n");
            return nullptr;
        }

        printf("[ManualMapper] Verification complete. Allocating image space (Size: 0x%X)...\n", 
            nt_headers->OptionalHeader.SizeOfImage);

        
        uint8_t* image_base = reinterpret_cast<uint8_t*>(VirtualAlloc(
            nullptr,
            nt_headers->OptionalHeader.SizeOfImage,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_EXECUTE_READWRITE
        ));

        if (!image_base) {
            printf("[ManualMapper] Error: Failed to allocate memory. Code: %lu\n", GetLastError());
            return nullptr;
        }

        
        std::memcpy(image_base, pe_data, nt_headers->OptionalHeader.SizeOfHeaders);

        
        auto* section_header = IMAGE_FIRST_SECTION(nt_headers);
        for (size_t i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i, ++section_header) {
            if (section_header->SizeOfRawData == 0) continue;

            
            if (static_cast<size_t>(section_header->PointerToRawData) + section_header->SizeOfRawData > pe_size) {
                printf("[ManualMapper] Error: Section data out of bounds.\n");
                VirtualFree(image_base, 0, MEM_RELEASE);
                return nullptr;
            }

            uint8_t* dest_address = image_base + section_header->VirtualAddress;
            const uint8_t* src_address = pe_data + section_header->PointerToRawData;
            
            std::memcpy(dest_address, src_address, section_header->SizeOfRawData);
        }

        
        DWORD_PTR delta = reinterpret_cast<DWORD_PTR>(image_base) - nt_headers->OptionalHeader.ImageBase;
        if (delta != 0) {
            printf("[ManualMapper] Relocating PE image base delta (Delta: 0x%I64X)...\n", delta);
            auto& reloc_dir = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
            
            if (reloc_dir.Size > 0) {
                auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(image_base + reloc_dir.VirtualAddress);
                
                while (reloc->VirtualAddress != 0) {
                    size_t entries_count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
                    auto* reloc_data = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(reloc) + sizeof(IMAGE_BASE_RELOCATION));
                    
                    for (size_t i = 0; i < entries_count; ++i) {
                        uint16_t type = reloc_data[i] >> 12;
                        uint16_t offset = reloc_data[i] & 0x0FFF;
                        
                        if (type == IMAGE_REL_BASED_DIR64) {
                            auto* patch_address = reinterpret_cast<DWORD64*>(image_base + reloc->VirtualAddress + offset);
                            *patch_address += delta;
                        }
                    }
                    reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<uint8_t*>(reloc) + reloc->SizeOfBlock);
                }
            }
        }

        
        printf("[ManualMapper] Resolving IAT imported libraries...\n");
        auto& import_dir = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        
        if (import_dir.Size > 0) {
            auto* import_desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image_base + import_dir.VirtualAddress);
            
            while (import_desc->Name != 0) {
                const char* module_name = reinterpret_cast<const char*>(image_base + import_desc->Name);
                HMODULE imported_module = LoadLibraryA(module_name);
                
                if (!imported_module) {
                    printf("[ManualMapper] Error: Failed to resolve library import: %s\n", module_name);
                    VirtualFree(image_base, 0, MEM_RELEASE);
                    return nullptr;
                }

                auto* original_thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(image_base + 
                    (import_desc->OriginalFirstThunk ? import_desc->OriginalFirstThunk : import_desc->FirstThunk));
                auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(image_base + import_desc->FirstThunk);

                while (original_thunk->u1.AddressOfData != 0) {
                    if (IMAGE_SNAP_BY_ORDINAL64(original_thunk->u1.Ordinal)) {
                        FARPROC func_addr = GetProcAddress(imported_module, 
                            reinterpret_cast<const char*>(IMAGE_ORDINAL64(original_thunk->u1.Ordinal)));
                        
                        if (!func_addr) {
                            printf("[ManualMapper] Error: Failed to resolve ordinal import from %s.\n", module_name);
                            VirtualFree(image_base, 0, MEM_RELEASE);
                            return nullptr;
                        }
                        thunk->u1.Function = reinterpret_cast<ULONGLONG>(func_addr);
                    } else {
                        auto* import_by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(image_base + original_thunk->u1.AddressOfData);
                        FARPROC func_addr = GetProcAddress(imported_module, import_by_name->Name);
                        
                        if (!func_addr) {
                            printf("[ManualMapper] Error: Failed to resolve import '%s' from %s.\n", 
                                import_by_name->Name, module_name);
                            VirtualFree(image_base, 0, MEM_RELEASE);
                            return nullptr;
                        }
                        thunk->u1.Function = reinterpret_cast<ULONGLONG>(func_addr);
                    }
                    original_thunk++;
                    thunk++;
                }
                import_desc++;
            }
        }

        
        auto& tls_dir = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
        if (tls_dir.Size > 0) {
            printf("[ManualMapper] Executing TLS Callbacks...\n");
            auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY64*>(image_base + tls_dir.VirtualAddress);
            auto* callback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(tls->AddressOfCallBacks);
            
            if (callback) {
                while (*callback != nullptr) {
                    (*callback)(image_base, DLL_PROCESS_ATTACH, nullptr);
                    callback++;
                }
            }
        }

        
        if (nt_headers->OptionalHeader.AddressOfEntryPoint != 0) {
            printf("[ManualMapper] Executing DLL entry point (DllMain)...\n");
            using DllMainFn = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
            auto entry_point = reinterpret_cast<DllMainFn>(image_base + nt_headers->OptionalHeader.AddressOfEntryPoint);
            
            BOOL success = entry_point(reinterpret_cast<HINSTANCE>(image_base), DLL_PROCESS_ATTACH, nullptr);
            if (!success) {
                printf("[ManualMapper] Warning: DllMain returned FALSE on attachment.\n");
            }
        }

        printf("[ManualMapper] Mapped PE DLL successfully at 0x%p in RAM.\n", image_base);
        return reinterpret_cast<HMODULE>(image_base);
    }

    bool free_mapped_module(HMODULE module_base) {
        if (!module_base) return false;

        auto* image_base = reinterpret_cast<uint8_t*>(module_base);
        auto* dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(image_base);
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
            printf("[ManualMapper] Error: Invalid module base for unmapping.\n");
            return false;
        }

        auto* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS64*>(image_base + dos_header->e_lfanew);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
            printf("[ManualMapper] Error: Invalid NT headers during unmapping.\n");
            return false;
        }

        
        if (nt_headers->OptionalHeader.AddressOfEntryPoint != 0) {
            printf("[ManualMapper] Executing DllMain DLL_PROCESS_DETACH for unmapping...\n");
            using DllMainFn = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
            auto entry_point = reinterpret_cast<DllMainFn>(image_base + nt_headers->OptionalHeader.AddressOfEntryPoint);
            entry_point(reinterpret_cast<HINSTANCE>(image_base), DLL_PROCESS_DETACH, nullptr);
        }

        
        bool released = VirtualFree(module_base, 0, MEM_RELEASE) != 0;
        if (released) {
            printf("[ManualMapper] Released virtual memory block successfully. Memory leak safeguarded.\n");
        } else {
            printf("[ManualMapper] Warning: Failed to release virtual memory block.\n");
        }
        
        return released;
    }
}
