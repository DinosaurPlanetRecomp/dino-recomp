# Patch Changelist
The full list of game patches applied by base Dinosaur Planet: Recompiled.

## Misc
- Disable mips3 float mode for non-handwritten asm code. Mips3 float mode is now only turned on temporarily for the few handwritten asm code segments that need it. The C code in Dinosaur Planet was compiled with mips2 mode in mind, which causes various issues with the vanilla behavior.

## Core

### libnaudio/mp3
- Allow MP3 volume to be adjusted as a game setting.

### libultra/syncprintf
- Redirect `proutSyncPrintf` to recomp stdout.

### audio
- Increase audio command list buffer size to avoid crashes.

### camera
- Submit view and projection matrices separately to RT64 to avoid float precision issue when RT64 decomposes the matrix.
- Remove hardcoded -6/+6 screen scissor offsets.
- Remove ROM read that triggers the "anti-piracy viewport".

### main
- Move main graphics buffers into static patch memory to save on vanilla pool memory.
- Double the size of the gfx, mtx, pol, and vtx buffers.
- Draw invisible fullscreen rect at the end of the display list to prevent RT64 from stretching out the final framebuffer in certain scenarios and not applying aspect ratio correction in widescreen.

### map
- Adjust frustrum culling calculation for widescreen.

### memory
- Add new 4th memory pool that takes up the remainder of recomp patch memory. Allocations fallback to this.
- Fail predictably on negative allocations and when all pools are full.
- Increase free queue size.
- If the free queue is full, free the address immediately instead of triggering UB.

### print
- Restore diPrintf functionality.

### rsp
- Remove hardcoded -1/+1 screen scissor offsets.

### scheduler
- Remove RSP/RDP stall check.
- Allow 60 fps when 60 hz gameplay is enabled.

### vi
- Use custom addresses for the 640x480 framebuffers to avoid an RT64 bug.
- Adjust screen coordinate culling for widescreen.

## DLLs - Engine

### 3 - ANIM
- Remove security dongle check.

### 5 - AMSEQ
- Fix volume option setter export not affecting the 4th music player.

### 20 - screens
- Center screen texture to prevent it from being stretched in widescreen.

### 60 - post
- Allow skipping copyright screen with the A button.

### 61 - rareware
- Allow skipping Rareware screen with the A button.

### 63 - gameselect
- Remove conditional redraw behavior to avoid RT64 bug when in high resolution or widescreen.
- Fix memory leak with task strings.
- Fix text wrap for dropshadow.

### 64 - nameentry
- Remove conditional redraw behavior to avoid RT64 bug when in high resolution or widescreen.

## DLLs - Objects

### 210 - player
- Remove security dongle check.

### 408 - IMIceMountain
- Don't force 20 FPS during bike race if 60 hz gameplay is enabled.
