#include <map>

#include "ultramodern/ultramodern.hpp"
#include "recomp.h"
#include "config/config.hpp"

static std::map<int, const char*> dllNames = {
    { 1, "cmdmenu" },
    { 2, "camcontrol" },
    { 3, "ANIM" },
    { 4, "Race" },
    { 5, "AMSEQ1" },
    { 5, "AMSEQ2" },
    { 6, "AMSFX" },
    { 7, "newday" },
    { 8, "newfog" },
    { 9, "newclouds" },
    { 10, "newstars" },
    { 11, "newlfx" },
    { 12, "minic" },
    { 13, "expgfx" },
    { 14, "modgfx" },
    { 15, "projgfx" },
    { 17, "partfx" },
    { 18, "objfsa" },
    { 20, "SCREENS" },
    { 21, "text" },
    { 22, "subtitles" },
    { 24, "waterfx" },
    { 25, "paths" },
    { 26, "CURVES" },
    { 28, "clrscr" },
    { 29, "gplay" },
    { 30, "tasktext" },
    { 31, "flash" },
    { 54, "pickup" },
    { 56, "putdown" },
    { 74, "picmenu" },
    { 75, "frontend" },
    { 223, "cannon_claw" },
};

extern "C" void load_overlay_by_id(uint32_t id, uint32_t ram_addr);
extern "C" void unload_overlay_by_id(uint32_t id);

extern "C" void recomp_on_dll_load(uint32_t id, uint32_t ram_addr) {
    load_overlay_by_id(id - 1, ram_addr);

    if (dino::config::get_debug_stdout_enabled()) {
        auto dllNameIt = dllNames.find(id);
        if (dllNameIt == dllNames.end()) {
            printf("Loaded DLL %u to address 0x%08X\n", id, ram_addr);
        } else {
            printf("Loaded DLL %u \"%s\" to address 0x%08X\n", id, dllNameIt->second, ram_addr);
        }
    }
}

extern "C" void recomp_on_dll_unload(uint32_t id) {
    unload_overlay_by_id(id - 1);

    if (dino::config::get_debug_stdout_enabled()) {
        auto dllNameIt = dllNames.find(id);
        if (dllNameIt == dllNames.end()) {
            printf("Unloaded DLL %u\n", id);
        } else {
            printf("Unloaded DLL %u \"%s\"\n", id, dllNameIt->second);
        }
    }
}

extern "C" void recomp_on_dll_load_too_few_exports(uint32_t id, uint32_t exports, uint32_t expectedExports) {
    fprintf(stderr, "Failed to load DLL %u! Expected at least %u exports, found %u.\n", 
        id, expectedExports, exports);
}

extern "C" void recomp_on_dll_load_too_many_dlls(uint32_t id) {
    fprintf(stderr, "Failed to load DLL %u! Too many DLLs are already loaded.\n", id);
}
