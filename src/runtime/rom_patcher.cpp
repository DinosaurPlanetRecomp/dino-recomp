#include "rom_patcher.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

#ifdef _MSC_VER
inline uint32_t byteswap(uint32_t val) {
    return _byteswap_ulong(val);
}
#else
constexpr uint32_t byteswap(uint32_t val) {
    return __builtin_bswap32(val);
}
#endif

constexpr size_t dlls_rom_offset = 0x38317CC;
constexpr size_t dlls_tab_rom_offset = 0x3B04BDC;

// Patch DLL $gp prologues to a version that recomp understands. This is necessary for the live recompiler to
// work correctly at runtime. The patched ROM used to compile Dinosaur Planet recomp already includes this patch
// but recomp's live recompiler looks at the *vanilla* ROM, so we need to patch it at runtime as well.
std::vector<uint8_t> dino::runtime::patch_rom(std::span<const uint8_t> unpatched_rom) {
    // Clone ROM contents
    std::vector<uint8_t> patched_rom{};
    patched_rom.resize(unpatched_rom.size_bytes());
    memcpy(patched_rom.data(), unpatched_rom.data(), unpatched_rom.size_bytes());
    
    // Read DLLS.tab
    std::vector<uint32_t> dlls_tab{};
    size_t i = 0;
    while (true) {
        uint32_t entry;
        memcpy(&entry, unpatched_rom.data() + dlls_tab_rom_offset + (i * 8) + (4 * 4), sizeof(entry));
        entry = byteswap(entry);
        i++;

        if (entry == 0xFFFFFFFF) {
            break;
        }

        dlls_tab.push_back(entry);
    }

    // Patch each DLL
    for (size_t i = 0; i < (dlls_tab.size() - 1); i++) {
        size_t dll_rom_offset = dlls_rom_offset + dlls_tab[i];

        // Read DLL header
        uint32_t text_offset;
        memcpy(&text_offset, unpatched_rom.data() + dll_rom_offset, sizeof(text_offset));
        text_offset = byteswap(text_offset);

        uint32_t rodata_offset;
        memcpy(&rodata_offset, unpatched_rom.data() + dll_rom_offset + 0x8, sizeof(rodata_offset));
        rodata_offset = byteswap(rodata_offset);

        if (rodata_offset != 0xFFFFFFFF) {
            // Skip past GOT
            size_t k = 0;
            while (true) {
                uint32_t entry;
                memcpy(&entry, unpatched_rom.data() + dll_rom_offset + rodata_offset + (k * 4), sizeof(entry));
                entry = byteswap(entry);
                k++;

                if (entry == 0xFFFFFFFE) {
                    break;
                }
            }

            // Iterate $gp relocations to patch each prologue
            while (true) {
                uint32_t entry;
                memcpy(&entry, unpatched_rom.data() + dll_rom_offset + rodata_offset + (k * 4), sizeof(entry));
                entry = byteswap(entry);
                k++;

                if (entry == 0xFFFFFFFD) {
                    break;
                }
                
                // Dino planet's non-standard $gp prologue looks like this:
                // lui $gp, 0x0
                // ori $gp, $gp, 0x0
                // nop
                //
                // Change 'ori $gp, $gp, 0x0' -> 'addiu $gp, $gp, 0x0'
                constexpr uint8_t ori_start_byte = 0x37;
                constexpr uint8_t addiu_start_byte = 0x27;
                uint8_t *gp_ptr = patched_rom.data() + dll_rom_offset + text_offset + entry;
                assert(*(gp_ptr + 0x4) == ori_start_byte);
                *(gp_ptr + 0x4) = addiu_start_byte;
            }
        }
    }

    return patched_rom;
}
