# Dinosaur Planet: Recompiled
Dinosaur Planet: Recompiled is a project that uses [N64: Recompiled](https://github.com/N64Recomp/N64Recomp) to **statically recompile** [Dinosaur Planet](https://www.rarewiki.com/wiki/Dinosaur_Planet) into a native port with enhancements and extensive mod support. This project uses [RT64](https://github.com/rt64/rt64) as the rendering engine to provide graphical enhancements.

> **[Check out the latest release here](https://github.com/DinosaurPlanetRecomp/dino-recomp/releases).**

> **This repository and its releases do not contain game assets. The original game is required to build or run this project.**

## Table of Contents
* [System Requirements](#system-requirements)
* [Known Issues](#known-issues)
* [FAQ](#faq)
* [Modding](#modding)
* [Building](#building)
* [Credits](#credits)

## System Requirements
A GPU supporting Direct3D 12.0 (Shader Model 6) or Vulkan 1.2 is required to run this project. The oldest GPUs that should be supported for each vendor are:
* GeForce GT 630
* Radeon HD 7750 (the one from 2012, not to be confused with the RX 7000 series) and newer
* Intel HD 510 (Skylake)

A CPU supporting the SSE4.1 instruction set is also required (Intel Core 2 Penryn series or AMD Bulldozer and newer).

If you have issues with crashes on startup, make sure your graphics drivers are fully up to date. 

## Known Issues
Please see [the GitHub issues](https://github.com/DinosaurPlanetRecomp/dino-recomp/issues) for a list of known issues with the project. Given the early state of Dinosaur Planet reverse engineering and the uniqueness of the game's code, this recomp has some notable issues and missing enhancements compared to other recomp projects.

## FAQ

#### What is static recompilation?
Static recompilation is the process of automatically translating an application from one platform to another. For more details, check out the full description of how this project's recompilation works here: [N64: Recompiled](https://github.com/N64Recomp/N64Recomp).

#### How is this related to the decompilation project?
Unlike traditional N64 ports, recompilation projects are not based on the source code of a related decompilation. However, the [Dinosaur Planet decompilation](https://github.com/zestydevy/dinosaur-planet) project plays a very important role in this recomp, by providing symbols, knowledge about game systems, and to a lesser extent actual decompiled code used in some patches.

If you would like to see this recomp improve, consider contributing to the decompilation! Progress made in the decompilation project opens the door for more and better enhancements/mods.

#### Where is the savefile stored?
- Windows: `%LOCALAPPDATA%\DinoPlanetRecompiled\saves`
- Linux: `~/.config/DinoPlanetRecompiled/saves`

#### How do I choose a different ROM?
**You don't.** This project is **only** a port of the beta ROM released by Forest of Illusion on Feb. 20, 2021. **It is not an emulator and it cannot run any arbitrary ROM.**

If you are looking for game patches commonly used when playing Dinosaur Planet in an emulator or on console, those are instead provided by recomp mods. **You cannot use a patched ROM with this project**. Recomp mods recreating these patches are developed outside of this project and can be downloaded on their respective project pages. These are [installed like any other recomp mod](#modding).

#### Can you run this project as a portable application?
Yes, if you place a file named `portable.txt` in the same folder as the executable then this project will run in portable mode. In portable mode, the save files, config files, and mods are placed in the same folder as the executable.

## Modding
Install community made mods and texture packs! Mods can change any part of the game, including adding completely new features and content. You can install mods by either dragging the mod files onto the game window before starting the game, by clicking the **Install Mods** button in the mod menu, or by placing the `.nrm` file [into the mods directory](#where-are-mods-installed-to). Mods can be toggled in the mod menu, and some mods can be configured there as well.

> [!WARNING]
> Mods do not run in a sandbox! Please only install mods from trusted sources.

#### Where are mods installed to?
- Windows: `%LOCALAPPDATA%\DinoPlanetRecompiled\mods`
- Linux: `~/.config/DinoPlanetRecompiled/mods`

#### Finding mods
Check out the official [Dinosaur Planet: Recompiled Thunderstore page](https://thunderstore.io/c/dinosaur-planet-recompiled/) to discover existing mods by the community!

#### Creating mods
If you would like to create your own mods, please see the following resources:
- [N64 Recomp Modding documentation](https://hackmd.io/fMDiGEJ9TBSjomuZZOgzNg)
- [RT64 Texture Pack documentation](https://github.com/rt64/rt64/blob/main/TEXTURE-PACKS.md)
- [Dinosaur Planet: Recompiled - Mod Template](https://github.com/DinosaurPlanetRecomp/dino-recomp-mod-template)
- [Dinosaur Planet: Recompiled - Mod API](https://github.com/DinosaurPlanetRecomp/dino-recomp-mod-api)
- [Dinosaur Planet: Recompiled - Decomp Bridge](https://github.com/DinosaurPlanetRecomp/dino-recomp-decomp-bridge)
- [Example mods](https://github.com/Francessco121/dino-recomp-mods)
- [N64Recomp](https://github.com/N64Recomp/N64Recomp) for the source code of `RecompModTool`.

## Building
For developers, please see [BUILDING.md](./BUILDING.md) for build and development environment setup instructions.

## Credits
* [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp) for the base project. Much of this project is a direct fork of the Majora's Mask recomp.
* [N64Recomp](https://github.com/N64Recomp/N64Recomp) for static recompilation.
* [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) for a reimplementation of libultra and general handling of common recomp tasks.
* [RT64](https://github.com/rt64/rt64) for the project's rendering engine.
* [Dinosaur Planet Decompilation](https://github.com/zestydevy/dinosaur-planet) for symbols, headers, and general knowledge about the game's internals.
* [RmlUi](https://github.com/mikke89/RmlUi) for building the menus and launcher.
* [lunasvg](https://github.com/sammycage/lunasvg) for SVG rendering, used by RmlUi.
* [FreeType](https://freetype.org/) for font rendering, used by RmlUi  .
* [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue) for semaphores and fast, lock-free MPMC queues.
* [Gamepad Motion Helpers](https://github.com/JibbSmart/GamepadMotionHelpers) for sensor fusion and calibration algorithms to implement gyro aiming.
