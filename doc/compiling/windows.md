# Compiling on Windows using MSVC

If you're just creating mods or games with Luanti, you do not need to compile Luanti. Instead, follow Ruben Wardy's [Luanti Modding Book](https://rubenwardy.gitlab.io/minetest_modding_book) to get started modding. Compiling Luanti is only required if you plan to modify the Luanti engine itself.

## Requirements

-   [Visual Studio 2015 or newer](https://visualstudio.microsoft.com) including 
"Desktop development with C++"
-   [CMake](https://cmake.org/download/)
-   [vcpkg](https://github.com/Microsoft/vcpkg) (included with Visual Studio)
-   [Git](https://git-scm.com/downloads)

### Visual Studio with C++

1. Install Visual Studio (VS) from [visualstudio.microsoft.com](https://visualstudio.microsoft.com)
1. In the VS installer, select "Desktop development with C++":

    ![VS installer showing desktop development with C++ selected](assets/vs-installer.png)
1. Confirm the installation.

This will install the C++ compiler used in later steps. VS is also the recommended IDE for Luanti.

You may notice that the C++ tools include CMake, but is should also be installed separately for compatibility with Luanti.

### CMake

Install from [cmake.org/download](https://cmake.org/download/). Once installed, you should be able to run `cmake-gui` from the start menu:

![cmake-gui in Windows start menu shows app result](./assets/cmake-gui-search.png)

<!-- 
### vcpkg

vcpkg is a package manager for C++.

```powershell
cd /c # Install vcpkg at any path without spaces in it, see below for details
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

Find `path/to/vcpkg/buildsystems/vcpkg.cmake` to confirm vcpkg is ready to use.

> For more details, [follow the vcpkg installation guide](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started#1---set-up-vcpkg)

It is highly recommended to install vcpkg at a path without spaces in it for compatibility with  Luanti.

```
"C:\Program Files\vcpkg\vcpkg.exe" # bad, may have issues
"C:\vcpkg\vcpkg.exe" # good, no spaces means fewer problems
```

If spaces are present, when trying to compile Luanti, you may see errors like:

```
libtool:   error: 'Files/vcpkg/buildtrees/libiconv/x64-windows-dbg/lib/libcharset.la' is not a directory
```
-->

<!-- todo check if this is necessary
## Compiling and installing the dependencies

After you successfully built vcpkg you can easily install the required libraries:

```powershell
vcpkg install zlib zstd curl[winssl] openal-soft libvorbis libogg libjpeg-turbo sqlite3 freetype luajit gmp jsoncpp gettext[tools] sdl2 --triplet x64-windows
```

This command takes about 10-30 minutes to complete, depending on your device.

-   `curl` is highly recommended, as it's required to read the serverlist; `curl[winssl]` is required to use the content store.
-   `openal-soft`, `libvorbis` and `libogg` are optional, but required to use sound.
-   `luajit` is optional, it replaces the integrated Lua interpreter with a faster just-in-time interpreter.
-   `gmp` and `jsoncpp` are optional, otherwise the bundled versions will be compiled
-   `gettext` is optional, but required to use translations.

There are other optional libraries, but we don't test if they can build and link correctly.

Use `--triplet` to specify the target triplet, e.g. `x64-windows` or `x86-windows`.
-->

## Install Luanti dependencies

1. Start up the CMake GUI (Win > search "cmake-gui" > open)
2. Select **Browse Source...** and select `path/to/luanti` (where you've cloned the repo)
3. Select **Browse Build...** and select `path/to/luanti/build` (a new folder that CMake will prompt to create)
4. Select **Configure**
5. Choose the right Visual Studio version and target platform. Currently, Luanti uses Visual Studio 16 2019, but newer VS versions should work as well. The VS version has to match the version of the installed dependencies.
6. Choose **Specify toolchain file for cross-compiling**
7. Click **Next**
8. Select the vcpkg toolchain file, e.g. `C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake`. Save this value for later.
9. Click Finish
10. Wait until CMake generates the cache file (this may take about 10-30 minutes, depending on your device)
11. If there are any errors, solve them and hit **Configure**
12. Click **Generate**
13. Click **Open Project**

## Compile Luanti

There are two ways to compile Luanti: via Visual Studio or via CLI.

### Compile in Visual Studio

14. Compile Luanti inside Visual Studio.
    -   If you get "Unable to start program '...\x64\Debug\ALL_BUILD'. Access is denied", try compiling via the CLI instead.

### Compile via CLI

While in the `path/to/minetest` folder, run the following script in PowerShell:

```powershell
$vs="Visual Studio 17 2022" # or "Visual Studio 16 2019", etc., whatever matches your system
$toolchain_file="path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" # todo vcpkg path from "Install Luanti dependencies" section
cmake . -G $vs -DCMAKE_TOOLCHAIN_FILE=$toolchain_file -DCMAKE_BUILD_TYPE=Release -DENABLE_CURSES=OFF
cmake --build . --config Release
```

## Windows Installer using WiX Toolset

Requirements:

-   [Visual Studio 2017](https://visualstudio.microsoft.com/)
-   [WiX Toolset](https://wixtoolset.org/)

In the Visual Studio 2017 Installer select **Optional Features -> WiX Toolset**.

Build the binaries as described above, but make sure you unselect `RUN_IN_PLACE`.

Open the generated project file with Visual Studio. Right-click **Package** and choose **Generate**.
It may take some minutes to generate the installer.
