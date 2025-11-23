# Compiling on Windows using MSYS2

# Requirements

- [MSYS2](https://msys2.org)


## Installing dependencies

Open MSYS2 CLANG64

Update MSYS2
```bash
pacman -Syu
```

Install the dependencies
```bash
pacman -S git mingw-w64-clang-x86_64-{clang,cmake,ninja,curl-winssl,libpng,libjpeg-turbo,freetype,libogg,libvorbis,sqlite3,openal,zstd,gettext,luajit,SDL2}
```
## Prepare to build

Go to the desktop folder
```bash
cd /c/Users/$USER/Desktop
```

Clone Luanti
```bash
git clone --depth 1 https://github.com/luanti-org/luanti.git
cd luanti
```
## Compile

Compile Luanti
```bash
mkdir build
cd build
cmake .. -G Ninja
ninja
cd ..
```

Bundle the required DLLs
```bash
curl https://raw.githubusercontent.com/rollerozxa/msys2-bundledlls/master/bundledlls > bundledlls.sh
./bundledlls.sh bin/luanti.exe bin/
```

Run Luanti
```bash
./bin/luanti.exe
```

# Compiling on Windows using MSVC

## Requirements

- [Visual Studio 2015 or newer](https://visualstudio.microsoft.com)
- [CMake](https://cmake.org/download/)
- [vcpkg](https://github.com/Microsoft/vcpkg)
- [Git](https://git-scm.com/downloads)


## Compiling and installing the dependencies

It is highly recommended to use vcpkg as package manager.

After you successfully built vcpkg you can easily install the required libraries:
```powershell
vcpkg install zlib zstd curl[ssl] openal-soft libvorbis libogg libjpeg-turbo sqlite3 freetype luajit gmp jsoncpp gettext[tools] sdl2 --triplet x64-windows
```

- `curl` is optional, but required to read the serverlist, `curl[ssl]` is required to use the content store.
- `openal-soft`, `libvorbis` and `libogg` are optional, but required to use sound.
- `luajit` is optional, it replaces the integrated Lua interpreter with a faster just-in-time interpreter.
- `gmp` and `jsoncpp` are optional, otherwise the bundled versions will be compiled
- `gettext` is optional, but required to use translations.

There are other optional libraries, but they are not tested if they can build and link correctly.

Use `--triplet` to specify the target triplet, e.g. `x64-windows` or `x86-windows`.


## Compile Luanti

### a) Using the vcpkg toolchain and CMake GUI

1. Start up the CMake GUI
2. Select **Browse Source...** and select DIR/luanti
3. Select **Browse Build...** and select DIR/luanti-build
4. Select **Configure**
5. Choose the right visual Studio version and target platform. It has to match the version of the installed dependencies
6. Choose **Specify toolchain file for cross-compiling**
7. Click **Next**
8. Select the vcpkg toolchain file e.g. `D:/vcpkg/scripts/buildsystems/vcpkg.cmake`
9. Click Finish
10. Wait until cmake have generated the cash file
11. If there are any errors, solve them and hit **Configure**
12. Click **Generate**
13. Click **Open Project**
14. Compile Luanti inside Visual studio.

### b) Using the vcpkg toolchain and the commandline

Run the following script in PowerShell:

```powershell
cmake . -G"Visual Studio 16 2019" -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_CURSES=OFF
cmake --build . --config Release
```
Make sure that the right compiler is selected and the path to the vcpkg toolchain is correct.


## Windows Installer using WiX Toolset

Requirements:
* [Visual Studio 2017](https://visualstudio.microsoft.com/)
* [WiX Toolset](https://wixtoolset.org/)

In the Visual Studio 2017 Installer select **Optional Features -> WiX Toolset**.

Build the binaries as described above, but make sure you unselect `RUN_IN_PLACE`.

Open the generated project file with Visual Studio. Right-click **Package** and choose **Generate**.
It may take some minutes to generate the installer.
