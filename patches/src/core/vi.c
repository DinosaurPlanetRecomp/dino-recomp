#include "patches.h"
#include "patches/vi.h"
#include "recomp_funcs.h"

#include "sys/gfx/gx.h"
#include "sys/main.h"
#include "sys/joypad.h"

extern OSIoMesg D_800BCC90;
extern OSDevMgr __osViDevMgr;
extern s8 D_80093064;

extern void vi_swap_buffers(void);
extern void vi_set_mode(s32 mode);
extern void vi_update_fb_size_from_current_mode(int framebufferIndex);
extern void vi_func_8005DEE8(void);

s32 recomp_snowbike30FPS = FALSE;

static u16 hack_480pFramebuffers[2][640 * 480];

RECOMP_PATCH void vi_init_framebuffers(int someBool, s32 width, s32 height) {
    VideoResolution *resPtr;
    u32 hRes;
    u32 vRes;

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

RECOMP_PATCH void vi_set_update_rate_target(u32 target) {
    // @recomp: Don't let the snowbike race cap the framerate
    if (recomp_snowbike30FPS) {
        target = 1;
    }

    gViUpdateRateTarget = target;
}

RECOMP_PATCH int vi_contains_point(s32 x, s32 y) {
    // @recomp: Adjust for recomp aspect ratio. The game thinks we're running at 4:3 so we need this
    // to return true for negative x values and x values greater than 320, depending on the recomp screen size.
    // TODO: doesnt seem to work for everything...
    u32 gameResWidth = gCurrentResolutionH[gFramebufferChoice];
    u32 gameResHeight = gCurrentResolutionV[gFramebufferChoice];

    s32 ulx = 0;
    s32 uly = 0;
    s32 lrx = (s32)gameResWidth;
    s32 lry = (s32)gameResHeight;

    f32 widthScale = (recomp_get_aspect_ratio() / (4.0f / 3.0f)) - 1.0f;
    s32 adjust = (s32)((gameResWidth * widthScale) / 2.0f);
    ulx -= adjust;
    lrx += adjust;

    return x >= ulx && x < lrx
        && y >= uly && y < lry;
}

RECOMP_PATCH s32 vi_frame_sync(s32 param1) {
    s32 updateRate;
    s32 vidMode;

    updateRate = 1;

    if (gViBlackTimer != 0) {
        gViBlackTimer -= 1;

        if (gViBlackTimer == 0) {
            osViBlack(FALSE);
        }
    }

    if (param1 != 8) {
        vi_swap_buffers();
    }

    while (osRecvMesg(&gVideoMesgQueue, NULL, OS_MESG_NOBLOCK) != -1) {
        updateRate += 1;
    }

    gViUpdateRate = updateRate;

    if (gViUpdateRate < gViUpdateRateTarget) {
        gViUpdateRate = gViUpdateRateTarget;
    }

    while (updateRate < gViUpdateRate) {
        osRecvMesg(&gVideoMesgQueue, NULL, OS_MESG_BLOCK);
        updateRate++;
    }

    if (D_80093060 != 0) {
        vidMode = vi_get_mode();

        if (D_80093060 == 3) {
            vi_set_mode(vidMode);
            vi_update_fb_size_from_current_mode(gFramebufferChoice);
            osViSwapBuffer(gFramebufferCurrent);
        } else if (D_80093060 == 2) {
            vi_update_fb_size_from_current_mode(gFramebufferChoice);
            osViSwapBuffer(gFramebufferCurrent);
        } else {
            D_800BCC90.hdr.type = 0x11;
            D_800BCC90.hdr.retQueue = (OSMesgQueue*)&gTvViMode;
            osSendMesg(__osViDevMgr.evtQueue, &D_800BCC90, OS_MESG_BLOCK);
            osViSwapBuffer(gFramebufferCurrent);
            D_80093064 ^= 1;
        }

        D_80093060 -= 1;
    } else {
        if (get_pause_state() == 1) {
            // Create pause screen screenshot
            set_pause_state(2);
            // @recomp: Don't copy framebuffer here, we handle this elsewhere in recomp
            //bcopy(gFramebufferNext, gFramebufferEnd, 0x25800);
        } else {
            osViSwapBuffer(gFramebufferCurrent);
        }
    }

    joy_read_nonblocking();
    vi_func_8005DEE8();
    osRecvMesg(&gVideoMesgQueue, NULL, OS_MESG_BLOCK);

    return updateRate;
}
