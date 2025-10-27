#include "gfx_patches.h"

#include "patches.h"

#include "sys/gfx/gx.h"
#include "types.h"

s32 snowbike30FPS = FALSE;

static u16 hack_480pFramebuffers[2][640 * 480];

RECOMP_PATCH void vi_init_framebuffers(int someBool, s32 width, s32 height) {
    VideoResolution *resPtr;
    u32 hRes;
    u32 vRes;

    recomp_printf("vi_init_framebuffers(%d, %d, %d)\n", someBool, width, height);

    // Get resolution by current video mode
    resPtr = &gResolutionArray[gVideoMode & 0x7];
    hRes = resPtr->h;
    vRes = resPtr->v;

    // Set current resolution
    gCurrentResolutionH[0] = hRes;
    gCurrentResolutionH[1] = hRes;
    gCurrentResolutionV[0] = vRes;
    gCurrentResolutionV[1] = vRes;

    if (osMemSize != 0x800000) {
        // No expansion pack detected
        gFramebufferPointers[0] = (u16*)(FRAMEBUFFER_ADDRESS_NO_EXP_PAK);
        gFramebufferPointers[1] = (u16*)(FRAMEBUFFER_ADDRESS_NO_EXP_PAK + ((width * height) * 2));
        
        gFramebufferStart = (u16*)(FRAMEBUFFER_ADDRESS_NO_EXP_PAK);
        return;
    }
    
    if (height == 480) {
        // PAL framebuffer height
        // gFramebufferPointers[0] = (u16*)(FRAMEBUFFER_ADDRESS_EXP_PAK);
        // gFramebufferPointers[1] = (u16*)(FRAMEBUFFER_ADDRESS_EXP_PAK + ((width * height) * 2));
        
        // gFramebufferStart = (u16*)(FRAMEBUFFER_ADDRESS_EXP_PAK);
        
        // @recomp: Use custom addresses for the 640x480 framebuffers to avoid triggering a bug in RT64.
        // If the 640x480 and 320x260 framebuffers share the same address space, RT64 gets confused and thinks
        // the old 640x480 framebuffer still exists when the game switches to 260p. This causes a memory leak
        // that eventually crashes the game. Separating these resolutions avoids the issue.
        // HACK: remove this when RT64 is patched
        gFramebufferPointers[0] = (u16*)hack_480pFramebuffers[0];
        gFramebufferPointers[1] = (u16*)hack_480pFramebuffers[1];
        
        gFramebufferStart = gFramebufferPointers[0];
    } else {
        // NTSC/M-PAL framebuffer height
        gFramebufferPointers[0] = (u16*)(FRAMEBUFFER_ADDRESS_EXP_PAK);
        gFramebufferPointers[1] = (u16*)(FRAMEBUFFER_ADDRESS_EXP_PAK + ((width * height) * 2));
        
        gFramebufferEnd = (u16*)(((int) (FRAMEBUFFER_ADDRESS_EXP_PAK + ((width * height) * 2))) + ((width * height) * 2));
        gFramebufferStart = (u16*)0x80200000;
    }
}

extern Gfx *gMainGfx[2];
extern Gfx *gCurGfx;
extern Mtx *gMainMtx[2];
extern Mtx *gCurMtx;
extern Vertex *gMainVtx[2];
extern Vertex *gCurVtx;
extern Triangle *gMainPol[2];
extern Triangle *gCurPol;

// @recomp: Move graphics buffers into patch memory to save heap memory
static Gfx recompMainGfx[2][RECOMP_MAIN_GFX_BUF_SIZE / sizeof(Gfx)]; 
static Mtx recompMainMtx[2][RECOMP_MAIN_MTX_BUF_SIZE / sizeof(Mtx)]; 
static Vertex recompMainVtx[2][RECOMP_MAIN_VTX_BUF_SIZE / sizeof(Vertex)]; 
static Triangle recompMainPol[2][RECOMP_MAIN_POL_BUF_SIZE / sizeof(Triangle)]; 

RECOMP_PATCH void alloc_frame_buffers(void) {
    // @recomp: Use larger buffer sizes

    // in default.dol these have names as well.
    // alloc graphic display list command buffers. ("main:gfx" in default.dol)
    gMainGfx[0] = recompMainGfx[0];
    gMainGfx[1] = recompMainGfx[1];

    // matrix buffers ("main:mtx")
    gMainMtx[0] = recompMainMtx[0];
    gMainMtx[1] = recompMainMtx[1];

    // polygon buffers? ("main:pol")
    gMainPol[0] = recompMainPol[0];
    gMainPol[1] = recompMainPol[1];

    // vertex buffers ("main:vtx")
    gMainVtx[0] = recompMainVtx[0];
    gMainVtx[1] = recompMainVtx[1];
}

RECOMP_PATCH void vi_set_update_rate_target(u32 target) {
    // @recomp: Don't let the snowbike race cap the framerate
    if (snowbike30FPS) {
        target = 1;
    }

    gViUpdateRateTarget = target;
}
