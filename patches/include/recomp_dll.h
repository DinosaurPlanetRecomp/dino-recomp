#pragma once

#include "PR/ultratypes.h"

#define RECOMP_MAX_LOADED_CUSTOM_DLLS 256

typedef enum {
    // 0x0-0x1000
    DLL_BANK_ENGINE = 0,
    // 0x1000-0x2000
    DLL_BANK_MODGFX = 1,
    // 0x2000-0x4000
    DLL_BANK_PROJGFX = 2,
    // 0x4000-0x8000
    //
    // This bank is unused by the game and has been repurposed for use by misc recomp DLLs.
    DLL_BANK_RECOMP = 3,
    // 0x8000-0xFFFF
    DLL_BANK_OBJECTS = 4
} RecompDLLBank;

typedef struct RecompCustomDLLState {
    s32 id;
    s32 refCount;
    void *vtblPtr;
} RecompCustomDLLState;

#define DLL_INTERFACE_TO_CUSTOM_STATE(interfacePtr) ((RecompCustomDLLState*)((u32)interfacePtr - OFFSETOF(RecompCustomDLLState, vtblPtr)))

#define DLLBANK0_LAST_VANILLA_TABIDX 103
#define DLLBANK1_LAST_VANILLA_TABIDX 185
#define DLLBANK2_LAST_VANILLA_TABIDX 209
#define DLLBANK4_LAST_VANILLA_TABIDX 796

#define DLLBANK0_LAST_VANILLA_ID 0x67
#define DLLBANK1_LAST_VANILLA_ID 0x1051
#define DLLBANK2_LAST_VANILLA_ID 0x2017
#define DLLBANK4_LAST_VANILLA_ID 0x824B

void recomp_dll_init_system(void);
RecompCustomDLLState *recomp_get_loaded_custom_dlls(s32 *outLoadedDLLCount);
void *recomp_dll_load_deferred_custom(RecompDLLBank bank, u16 id, u16 exportCount);
void *recomp_dll_load_custom(RecompDLLBank bank, u16 id, u16 exportCount, s32 runConstructor);
RecompCustomDLLState *recomp_dll_interface_to_custom_state(void *dllInterfacePtr);
s32 recomp_dll_unload_custom(RecompCustomDLLState *state);
