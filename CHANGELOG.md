## v0.3.1
- Ambience now has its own volume slider, rather than being grouped with music.
- Added an experimental crash handler. Crash logs can now be found at:
    - Windows: `%LOCALAPPDATA%\DinoPlanetRecompiled\crash.log`
    - Linux: `~/.config/DinoPlanetRecompiled/crash.log`
- New UI scale option for the debug UI.
- The pause screen background is no longer black when standing in certain interiors like Volcano Force Point.
- Fixed foodbag inventory cheat menu.
- Fixed some jank with the mod configuration menus.
- Updated to the latest version of N64RecompRuntime.
    - Control stick inputs now use N64-Style Octagon Input Scaling.
- Updated to the latest version of RT64.
    - New fallbacks for certain AMD/Intel driver versions to use Vulkan, to avoid crashes and other issues.
- Updated SDL GameControllerDB (support for additional controllers).
- The `--skip-launcher` command line argument has been replaced with `--game dino-planet`.


## v0.3.0
#### New
- Dinosaur Planet can now be experienced at any framerate you want without affecting gameplay! This setting can be found under `Graphics -> Framerate`. **Please note:** While the majority of the game now supports this, the setting is still a work in progress and you will experience minor graphical glitches when active.
- The HUD can now be adjusted for widescreen display. This setting can be found under `Graphics -> HUD Placement`.
- Widescreen is no longer considered experimental. Feel free to set `Graphics -> Aspect Ratio` to Expand if it's not already set. **Note:** Playing at aspect ratios wider than 16:9 is not recommended. Wider aspects may encounter graphical issues.
- HUD and minimap visibility can now be adjusted in the General settings tab.
- The launcher background has been redesigned, thanks to [@rustyspottedcats](https://github.com/rustyspottedcats)!

#### Changes
- The cheats debug UI menu has been overhauled and now includes an inventory editor.
- The game's icon is now higher quality.
- Removed the '60 Hz Gameplay' option. Mods now have control over this.

#### Bug Fixes
- Motion blur and framebuffer effects will now display correctly.
- Fixed glitchy font glyph rendering when text was display along the edge of a scissor.
- Screens ("Krystal/Sabre's Adventure") now correctly fill the whole screen in widescreen.
- Letterboxing animations now correctly reach the intended height.
- Toggling fullscreen via a hotkey will now correctly save the adjusted fullscreen setting.

#### Patches
- Main graphics buffers are now 8x in size instead of just 2x.
- Block shape backface culling is now done on the RSP (RT64) instead of on the CPU.

#### For Mod Developers
- New "ReAsset" API (beta) to allow mods to patch existing assets and contribute new assets in a way that helps avoid mod conflicts.
- New API for registering custom extensions to save game data.
- Fixed crash that occurred when hooking a DLL function that contained a jump table.

#### Tech
- Updated SDL GameControllerDB (support for additional controllers).
- RT64 Plume update.
- NativeFileDialog errors will now correctly be displayed.


## v0.2.0
#### New Features
- New "60 Hz Gameplay" option to let the game simulate and render at 60 FPS without frame interpolation.
    - Note: This option is experimental and will need to be temporarily disabled to complete certain parts of the game.
- New sound options for: music volume, SFX volume, dialog volume, and toggling subtitles (#13).
- Joystick input range can now be configured as a way of adjusting sensitivity (#14).

#### Bug Fixes
- Subtitles no longer get stretched out when playing in widescreen (#11).
- Water rendering no longer gets cutoff at the bottom of the screen at some camera angles.
- The letterbox animation no longer jumps at the start/end of the animation.
- The pause screen is no longer locked to a super low resolution.
- Fixed crash that could sometimes happen when refreshing the mod list after deleting mods.
- The game no longer locks up after long periods of lag (#24, fixed in N64ModernRuntime).
- Fixed crash that occurred when closing the game with the debug UI open.
- Debug UI no longer renders game state a frame early.
- Removed forced framerate cap that was active while the debug UI was open.

#### Patches
- The Ice Mountain snow bike race is no longer capped to 20 FPS if 60 Hz gameplay is enabled.
- The mips3 float mode bit is now unset for most code. The bit is now only set when running mips3 handwritten asm code since all other code was compiled as mips2. This fixes a crash in some collision-detection related code that happened in many places and possibly fixes some other issues.
- Removed xldtob patch (no longer needed with the mips3 float mode change).
- Added a new 4th memory pool for the game to use (the game now has access to ~6 MiB more than normal).
- Memory allocation failures now display an error message.
- Fixed dropshadow text wrap on the game select screen.

#### For Mod Developers
- Support for custom DLLs.
- Support for custom menus.
- Removed the `recomp_on_game_tick_end` event (use a return hook instead).
- Reduced `recomp_vsprintf_helper` buffer to 8 KiB.
- Added a debug option to turn off logging for the DLL loader.
- RT64 extended GBI mode is now enabled for the entire display list.

#### Tech
- All error messages now print to `stderr`.


## v0.1.3-beta3
- Debug UI no longer renders game state a frame early.
- Fixed crash that occurred when closing the game with the debug UI open.
- Remove forced framerate cap that was active while the debug UI was open.


## v0.1.3-beta2
- New "60 Hz Gameplay" option to let the game simulate and render at 60 FPS without frame interpolation.


## v0.1.3-beta1
- Fixed crash that could sometimes happen when refreshing the mod list after deleting mods.
- Added a new 4th memory pool for the game to use (the game now has access to ~6 MiB more than normal).
- Memory allocation failures now display an error message.
- Added a debug option to turn off logging for the DLL loader.
- The mips3 float mode bit is now unset for most code. The bit is now only set when running mips3 handwritten asm code since all other code was compiled as mips2. This fixes a crash in some hit-detection related code that happened in many places and possibly fixes some other issues.

#### For Mod Developers
- Reduced `recomp_vsprintf_helper` buffer to 8 KiB.

#### Tech
- All error messages now print to `stderr`.
- Remove xldtob patch (no longer needed with the mips3 float mode change).


## v0.1.2
- Widescreen culling support for billboard sprites.
- Fixed mouse cursor flickering.
- The mouse cursor is now always visible when the debug UI is open.
- The Flatpak and AppImage builds now use the native file picker (via XDG Desktop Portal).
- The Flatpak build now requires fewer permissions.
- Added the `--show-console` command line option to launch with a console open on Windows.
- N64ModernRuntime, N64Recomp, RT64 updates:
     - Improved frame pacing when using the "Manual" framerate option (changing the framerate in Dino Planet is **still experimental** at this time!).
     - Improved consistency of input latency.
     - Fixed a memory leak that occurred when the program was minimized.

#### For Mod Developers
- Updated to a new version of N64ModernRuntime/N64Recomp:
    - Mods can now declare optional dependencies.
    - New exports to get the return value of a function from within a return hook.
    - Hooks and callbacks now respect the order of mods in the mod menu.
    - RT64 texture packs can now control [half pixel shift behavior](https://github.com/rt64/rt64/blob/main/TEXTURE-PACKS.md#shift) on a per-texture basis.
- Fixed recomputil memory slotmaps.
- Hooks for DLL functions now work correctly.

#### Tech
- X11 and GTK are no longer required to build.
- Releases no longer ship with debug symbols (smaller download/file size!).


## v0.1.1
- The debug UI is now accessible with the <kbd>F9</kbd> key, making it available for users with non-US keyboard layouts.
- Selecting a ROM/starting the game for the first time no longer has a chance to hardlock/crash.
- Playing at original aspect ratio no longer renders more than it should (as if widescreen was always enabled).
- Widescreen is now significantly more stable. Recomp-specific crashes caused by widescreen are fixed.


## v0.1.0
- Initial release.
- Added debug UI (ImGui).

#### Patches
- Skippable splash screen with the A button.
- Increased audio command list buffer size (prevents crashes when many sounds are playing at once).
