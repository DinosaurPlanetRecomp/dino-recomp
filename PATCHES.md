# Patch Changelist
The full list of game patches applied by base Dinosaur Planet: Recompiled.

## Misc
- Disable mips3 float mode for non-handwritten asm code. Mips3 float mode is now only turned on temporarily for the few handwritten asm code segments that need it. The C code in Dinosaur Planet was compiled with mips2 mode in mind, which causes various issues with the vanilla behavior.

## Core

### libultra/bcopy
- Runs reimplemented version on the C++ side for improved performance.

### libultra/bzero
- Runs reimplemented version on the C++ side for improved performance.

### libultra/syncprintf
- Redirect `proutSyncPrintf` to recomp stdout.

### mp3/mp3
- Allow MP3 volume to be adjusted as a game setting.

### audio
- Increase audio command list buffer size to avoid crashes.
- Allow DMA to be redirected to ReAsset.

### camera
- Submit view and projection matrices separately to RT64 to avoid float precision issue when RT64 decomposes the matrix.
- Remove hardcoded -6/+6 screen scissor offsets.
- Remove ROM read that triggers the "anti-piracy viewport".
- Matrix group patches for frame interpolation.
- Don't store object parent model matrices in the RSP projection matrix slot. Instead, parent matrices are factored into child object model matrices before being submitted to the RSP. This patch is necessary for frame interpolation support and is disabled when playing at the default framerate.
- Increase size of matrix pool. 

### dll
- The maximum number of loaded *vanilla* DLLs is increased to 256 (from 128).
- Support for custom DLLs. A maximum of 256 custom DLLs can be loaded, bringing the technical maximum number of loaded DLLs to 512.

### fbfx
- Disable original framebuffer FX code as it does not work with RT64. Framebuffer FX are reimplemented for recomp using GBI commands in another patch.

### filesystem
- ReAsset redirects.

### main
- Move main graphics buffers into static patch memory to save on vanilla pool memory.
- Double the size of the gfx, mtx, pol, and vtx buffers.
- Draw invisible fullscreen rect at the end of the display list to prevent RT64 from stretching out the final framebuffer in certain scenarios and not applying aspect ratio correction in widescreen.
- Frame interpolation related patches.

### map
- Adjust frustrum culling calculation for widescreen.
- Increase size of render list.
- Support loading uncompressed blocks.
- Matrix group patches for frame interpolation.
- Replace CPU backface culling with RSP backface culling (fixes vanilla issues, is more accurate, and is necessary for frame interpolation).
- Adjusted render list bitfield to support up to 511 visible objects (up from 127).
- Increased max visible objects from 180 -> 500 (matches object.c patch).

### memory
- Add new 4th memory pool that takes up the remainder of recomp patch memory. Allocations fallback to this.
- Fail predictably on negative allocations and when all pools are full.
- Increase free queue size.
- If the free queue is full, free the address immediately instead of triggering UB.

### menu
- Support for custom menus.

### model
- Support loading uncompressed models.
- Matrix group patches for frame interpolation.

### object
- Matrix group patches for frame interpolation.
- Increased max objects from 180 -> 500.

### objprint
- Matrix group patches for frame interpolation.

### print
- Restore diPrintf functionality.

### rarezip
- Overflow checking.

### rcp
- Remove hardcoded -1/+1 screen scissor offsets.

### scheduler
- Remove RSP/RDP stall check.
- Allow 60 fps when 60 hz gameplay is enabled.

### texture
- Support loading uncompressed textures.

### vi
- Use custom addresses for the 640x480 framebuffers to avoid an RT64 bug.
- Adjust screen coordinate culling for widescreen.

## DLLs - Engine

### 1 - cmdmenu
- Widescreen HUD patches.

### 2 - camcontrol
- Letterboxing adjustment to account for camera scissor patch.

### 3 - ANIM
- Remove security dongle check.
- Hook sequence keyframes to pass on interpolation skips to RT64 for frame interpolation.

### 5 - AMSEQ
- Fix volume option setter export not affecting the 4th music player.

### 13 - expgfx
- Matrix group patches for frame interpolation.

### 20 - screens
- Center screen texture to prevent it from being stretched in widescreen.

### 24 - waterfx
- Matrix group patches for frame interpolation.

### 29 - gplay
- Support custom save data.

### 59 - minimap
- Widescreen HUD patches.

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
