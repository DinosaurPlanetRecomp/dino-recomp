#include "librecomp/helpers.hpp"
#include "recomp.h"

#include "config/config.hpp"

// From N64ModernRuntime
extern "C" void load_overlay_by_id(uint32_t id, uint32_t ram_addr);
extern "C" void unload_overlay_by_id(uint32_t id);

extern "C" void recomp_on_dll_load(uint8_t* rdram, recomp_context* ctx) {
    u32 dllno = _arg<0, u32>(rdram, ctx);
    PTR(void) ramAddr = _arg<1, PTR(void)>(rdram, ctx);

    load_overlay_by_id(dllno - 1, ramAddr);

    if (dino::config::get_debug_stdout_enabled() && dino::config::get_debug_dll_logging_enabled()) {
        printf("Loaded DLL %u to address 0x%08X\n", dllno, ramAddr);
    }
}

extern "C" void recomp_on_dll_unload(uint8_t* rdram, recomp_context* ctx) {
    u32 dllno = _arg<0, u32>(rdram, ctx);

    unload_overlay_by_id(dllno - 1);

    if (dino::config::get_debug_stdout_enabled() && dino::config::get_debug_dll_logging_enabled()) {
        printf("Unloaded DLL %u\n", dllno);
    }
}
