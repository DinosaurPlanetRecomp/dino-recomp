# Building Guide

This guide will help you build the project on your local machine. The process will require you to provide the ROM released by Forest of Illusion on Feb. 20, 2021.

These steps cover: patching the ROM, running the recompiler, and finally building the project.

## 1. Clone the dino-recomp Repository
This project makes use of submodules so you will need to clone the repository with the `--recurse-submodules` flag.

```bash
git clone --recurse-submodules
# if you forgot to clone with --recurse-submodules
# cd /path/to/cloned/repo && git submodule update --init --recursive
```

## 2. Install Dependencies

### Linux
For Linux the instructions for Ubuntu are provided, but you can find the equivalent packages for your preferred distro.

```bash
# For Ubuntu, simply run:
sudo apt-get install cmake ninja-build libsdl2-dev libdbus-1-dev libfreetype-dev lld llvm clang
```

### macOS
Building on macOS requires Homebrew (a package manager for macOS), a special MIPS-capable version of Clang, and Xcode for Metal shader support. Follow these steps:

1. **Install Homebrew** (if not already installed):
   - Open Terminal and run:
     ```bash
     /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
     ```
   - Follow the on-screen instructions to complete the installation.

2. **Install build dependencies via Homebrew**:
   ```bash
   brew install cmake ninja sdl2 freetype llvm lld
   ```
   - This installs CMake (build system), Ninja (build tool), SDL2 (for graphics/input), Freetype (for fonts), and LLVM tools (including LLD linker).

3. **Download and install MIPS-capable Clang**:
   - The standard Clang on macOS doesn't support MIPS assembly, which is needed for recompiling N64 patches.
   - Go to the [n64recomp-clang releases page](https://github.com/LT-Schmiddy/n64recomp-clang/releases).
   - Download the latest release for your Mac's architecture:
     - For Apple Silicon (M1/M2/M3 chips): `Darwin-arm64-ClangEssentialsAndN64Recomp-ClangVersion...-MipsOnly.tar.xz`
     - For Intel Macs: `Darwin-x86_64-ClangEssentialsAndN64Recomp-ClangVersion...-MipsOnly.tar.xz`
   - Extract the downloaded archive to a location like `~/n64recomp-clang` (or any directory you prefer).
   - Note the path to the `bin` folder inside the extracted directory (e.g., `~/n64recomp-clang/bin`).

4. **Install Xcode**:
   - Metal shaders (used for graphics) require Xcode's command-line tools.
   - Open the Mac App Store and search for "Xcode".
   - Download and install Xcode (it's a large download, ~10-15 GB).
   - After installation, open Xcode once to accept the license agreement.

### Windows
You will need to install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/).
In the setup process you'll need to select the following options and tools for installation:
- Desktop development with C++
- C++ Clang Compiler for Windows
- C++ CMake tools for Windows

> [!WARNING]
> If you installed Clang 19 or newer through the Visual Studio Installer you will not be able to build the `patches` library without also installing a build of Clang supporting MIPS. Newer versions of Clang supporting MIPS can be downloaded from [n64recomp-clang](https://github.com/LT-Schmiddy/n64recomp-clang/releases).

> [!NOTE]
> You do not necessarily need the Visual Studio UI to build/debug the project but the above installation is still required. More on the different build options below. 

The other tool necessary will be `make` which can be installed via [Chocolatey](https://chocolatey.org/):
```bash
choco install make
```

## 3. Patching the target ROM
You will need to patch the ROM (md5: 49f7bb346ade39d1915c22e090ffd748) before running the recompiler.

This can be done by using the `tools/recomp_rom_patcher.py` script found in the [Dinosaur Planet Decompilation repository](https://github.com/zestydevy/dinosaur-planet). You can either clone the repository or use the submodule provided by this repository at `lib/dino-recomp-decomp-bridge/dinosaur-planet`.

For example, using the decomp submodule, run:
```bash
python3 ./lib/dino-recomp-decomp-bridge/dinosaur-planet/tools/recomp_rom_patcher.py -o baserom.patched.z64 baserom.z64
```

Once done, place the patched ROM in the root of this repository and rename it to `baserom.patched.z64` if necessary.

## 4. Generating the C code

Now that you have the required files, you must build [N64Recomp](https://github.com/N64Recomp/N64Recomp) and run it to generate the C code to be compiled. The building instructions can be found [here](https://github.com/N64Recomp/N64Recomp?tab=readme-ov-file#building). That will build the executables: `N64Recomp` and `RSPRecomp` which you should copy to the root of this repository.

After that, go back to the repository root, and run the following commands:
```bash
./N64Recomp dino.toml
./RSPRecomp aspMain.toml
```

## 5. Building the Project

This project uses CMake for builds. There's a few ways to run this depending on your preferences.

### Visual Studio (recommended)

Open the repository as a folder with Visual Studio. In the top toolbar, select `DinosaurPlanetRecompiled` as the target. You can now simply build and debug like normal.

Builds will be output to `out/build/x64-[Configuration]`.

### Visual Studio Code

#### Building
The extension [ms-vscode.cmake-tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) is needed to build this project in Visual Studio Code.

Builds will be output to `build`.

##### Linux
1. Add the following to your **workspace** configuration: `"cmake.generator": "Ninja"`
    - If the CMake extension automatically configured CMake before adding this, you will need to delete the build folder and reconfigure.
2. In the CMake tab, under Configure, select Clang as the compiler.
3. In the CMake tab, under Build (and Debug/Launch), set the build target to `DinosaurPlanetRecompiled`.
4. Using the CMake extension, you now should be able to build the project!

##### macOS
1. **Install the CMake Tools extension**:
   - In VS Code, go to the Extensions view (View > Extensions or Cmd+Shift+X).
   - Search for "CMake Tools" and install the extension by Microsoft (`ms-vscode.cmake-tools`).

2. **Configure VS Code workspace settings**:
   - Open the Command Palette (Cmd+Shift+P) and select "Preferences: Open Workspace Settings (JSON)".
   - Add the following settings to the JSON file:
     ```json
     {
       "cmake.generator": "Ninja",
       "cmake.configureArgs": ["-DPATCHES_C_COMPILER=/path/to/n64recomp-clang/bin/clang", "-DPATCHES_LD=/path/to/n64recomp-clang/bin/ld.lld"]
     }
     ```
     - Replace `/path/to/n64recomp-clang` with the actual path to your extracted MIPS Clang directory (e.g., `~/n64recomp-clang`).
     - If the CMake extension has already configured the project, delete the `build` folder in your workspace and restart VS Code.

3. **Configure the build**:
   - In the VS Code status bar at the bottom, click on the CMake kit selector (it might say "Unspecified" or show a compiler).
   - Select "Clang" as the compiler.
   - In the CMake panel (View > Command Palette > CMake: Focus on CMake View), under "Configure", ensure it's set up.
   - Under "Build", set the target to `DinosaurPlanetRecompiled`.

4. **Build the project**:
   - Click the build button in the CMake panel or use the Command Palette: "CMake: Build".
   - The build output will appear in the `build` folder.

##### Windows
1. Add the following to your **workspace** configuration:
    - `"cmake.generator": "Ninja"`
        - If the CMake extension automatically configured CMake before adding this, you will need to delete the build folder and reconfigure.
    - `"cmake.useVsDeveloperEnvironment": "always"`
        - Allows non-Visual Studio versions of clang-cl to be used.
    - `"cmake.configureArgs": ["-DPATCHES_C_COMPILER=<path to clang.exe>"]`
        - Replacing `<path to clang.exe>` with an absolute path to a version of clang with MIPS support. Not necessary if you installed a Clang version earlier than version 19 in the Visual Studio Installer as those versions include MIPS support.
2. In the CMake tab, under Configure, select clang-cl as the compiler.
    - Can either be the version included with Visual Studio ("Clang (MSVC CLI)") or another version of clang-cl you have installed.
3. In the CMake tab, under Build (and Debug/Launch), set the build target to `DinosaurPlanetRecompiled`.
6. Using the CMake extension, you now should be able to build the project!

#### Debugging
The project can be launched/debugged with your extension of preference, such as [ms-vscode.cpptools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) or [vadimcn.vscode-lldb](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb).

To set up a `launch.json` file, see https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/debug-launch.md#debug-using-a-launchjson-file, which explains how to combine the debugger extensions and the CMake extension.

#### Intellisense
C++ intellisense can be provided by extensions such as [ms-vscode.cpptools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) and [llvm-vs-code-extensions.vscode-clangd](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd).

- If using the Microsoft C++ Tools extension, you will likely need to manually configure `.vscode/c_cpp_properties.json` with appropriate include directories and defines.
- If using the clangd extension, intellisense will work out of the box, **however** if you also have the Microsoft C++ Tools extension installed, you will need to disable the intellisense provided by the Microsoft extension with the following configuration: `"C_Cpp.intelliSenseEngine": "disabled"`.

### CLI

If you prefer the command line you can build the project using CMake directly.

#### Linux
```bash
cmake -S . -B build-cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja -DCMAKE_BUILD_TYPE=Release # or Debug if you want to debug
cmake --build build-cmake --target DinosaurPlanetRecompiled -j$(nproc) --config Release # or Debug
```

#### macOS
macOS support is experimental and requires additional setup compared to Linux. Follow these steps carefully:

1. **Open Terminal**:
   - Press Cmd+Space, type "Terminal", and press Enter to open the Terminal app.

2. **Navigate to the project directory**:
   ```bash
   cd /path/to/dino-recomp
   ```
   - Replace `/path/to/dino-recomp` with the actual path where you cloned the repository.

3. **Install dependencies**:
   ```bash
   brew install cmake ninja sdl2 freetype llvm lld
   ```
   - This may take a few minutes. If you encounter errors, ensure Homebrew is installed and up to date.

4. **Download MIPS Clang**:
   - Open your web browser and go to https://github.com/LT-Schmiddy/n64recomp-clang/releases.
   - Download the appropriate file for your Mac:
     - Apple Silicon (M1/M2/M3): `Darwin-arm64-ClangEssentialsAndN64Recomp-ClangVersion...-MipsOnly.tar.xz`
     - Intel Mac: `Darwin-x86_64-ClangEssentialsAndN64Recomp-ClangVersion...-MipsOnly.tar.xz`
   - Save the file to your Downloads folder.

5. **Extract MIPS Clang**:
   ```bash
   cd ~/Downloads
   tar -xf Darwin-arm64-ClangEssentialsAndN64Recomp-ClangVersion*.tar.xz  # Adjust filename as needed
   mv ClangEssentialsAndN64Recomp-ClangVersion* ~/n64recomp-clang  # Rename and move to home directory
   ```
   - This creates a folder `~/n64recomp-clang` with the MIPS Clang tools.

6. **Install Xcode**:
   - Open the App Store app.
   - Search for "Xcode" and install it (free, but large download).
   - After installation, open Xcode once to accept the license.

7. **Configure the build**:
   ```bash
   cd /path/to/dino-recomp  # Back to project directory
   cmake -S . -B build-cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DPATCHES_C_COMPILER=~/n64recomp-clang/bin/clang -DPATCHES_LD=~/n64recomp-clang/bin/ld.lld -G Ninja -DCMAKE_BUILD_TYPE=Release
   ```
   - If the path to MIPS Clang is different, adjust accordingly.

8. **Build the project**:
   ```bash
   cmake --build build-cmake --target DinosaurPlanetRecompiled -j$(sysctl -n hw.ncpu) --config Release
   ```
   - This will compile the project. It may take 10-30 minutes depending on your Mac.

Note: If you encounter issues, ensure all paths are correct and that Xcode is fully installed.

#### Windows
> [!IMPORTANT]  
> The following *must* be ran from the "x64 Native Tools Command Prompt for VS 2022" (vcvars64.bat). `clang-cl` will not be able to compile the project without a Visual Studio environment. Make sure to `cd` to this repository before running the command!

```batch
REM Replace the PATCHES_C_COMPILER path below with a path to your copy of MIPS clang
REM Also change Release to Debug in both commands if you want to debug

cmake -S . -B build-cmake -DCMAKE_CXX_COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe" -DCMAKE_C_COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe" -DPATCHES_C_COMPILER="path\to\clang-mips\bin\clang.exe" -G Ninja -DCMAKE_BUILD_TYPE=Release

cmake --build build-cmake --target DinosaurPlanetRecompiled -j%NUMBER_OF_PROCESSORS% --config Release
```

> [!TIP]
> `clang-cl` from a normal LLVM build can also be used instead of the one bundled with Visual Studio. Just point the `CMAKE_CXX_COMPILER` and `CMAKE_C_COMPILER` paths to the one you want. The command must still be ran from within `vcvars64.bat` however.

Builds will be output to `build-cmake`.

## 6. Success

Voilà! You should now have a `DinosaurPlanetRecompiled` executable in the build directory! You will need to run the executable out of the root folder of this project or copy the assets folder to the build folder to run it.

> [!IMPORTANT]  
> In the game itself, you should be using a standard ROM, not the patched one.
