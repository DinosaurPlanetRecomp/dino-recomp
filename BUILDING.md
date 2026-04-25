# Building Guide

This guide will help you build the project on your local machine. The process will require you to provide the 2000 prototype ROM released by Forest of Illusion on Feb. 20, 2021.

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
