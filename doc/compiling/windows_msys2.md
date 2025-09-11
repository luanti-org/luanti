# Compiling on Windows using MSYS2

[Compiling with Visual Studio](windows.md) | **Compiling with MSYS2**

For a video tutorial of setting up a Luanti development environment on Windows using MSYS2 (including setting up VSCodium), you can see [this video](https://www.youtube.com/watch?v=y8eWDzLNa3E) by Exe_Virus.

## Installation

First, download and install the [latest version of MSYS2](https://www.msys2.org/).

After installation, a terminal opens. Run the following command to update the environment:

```bash
pacman -Syu
```

The terminal will then ask you to close it when done, proceed with doing so. Then open the MSYS2 CLANG64 environment whose icon has a dark orange background. (For information on what the different environments do, see [Environments](https://www.msys2.org/docs/environments/) in MSYS2's documentation)

## Compiling

Install all the necessary dependencies:

```bash
pacman -S git mingw-w64-clang-x86_64-{clang,cmake,ninja,curl-winssl,libpng,libjpeg-turbo,freetype,libogg,libvorbis,sqlite3,openal,zstd,gettext,luajit,SDL2}
```

Navigate to some folder where you want to clone the Luanti repository. To get out of MSYS2's home folder and into your regular users folder, you would want to enter something like this:

```bash
cd /c/Users/$USER/Desktop
```

Clone Luanti:

```bash
git clone --depth 1 https://github.com/luanti-org/luanti
cd luanti
```

And start the building process:

```bash
mkdir build; cd build
cmake .. -G Ninja
ninja
```

Once it's finished compiling, there should be a luanti.exe executable inside of `bin/` inside your Luanti folder. You can run it inside of the MSYS2 environment by running `../bin/luanti.exe` in the terminal, but it will not work if you try to open the executable from Windows explorer, as the necessary DLLs aren't next to the executable.

## Bundling DLLs

For bundling DLLs, you may want to use [msys2-bundledlls](https://github.com/rollerozxa/msys2-bundledlls) which is able to copy over any libraries the executable is linked against and put it next to the executable.

Run the following commands from the build directory to download the script:

```bash
curl https://raw.githubusercontent.com/rollerozxa/msys2-bundledlls/master/bundledlls > ../bundledlls
```

When you have downloaded the script, usage is as such:

```bash
../bundledlls ../bin/luanti.exe ../bin/
```

It will print out a list of libraries it has copied to the binary folder once finished. Now it should be possible to run the Luanti executable outside of the MSYS2 environment.

## Notes

### Something about packages being untrusted or corrupted?

If you have an existing MSYS2 install that has been dormant for a while without updates, you might run into issues trying to install or update packages as the keyring is outdated. See [potential issues](https://www.msys2.org/docs/updating/#potential-issues) on the MSYS2 website on how to solve this.

### Graphics is broken in a VM

If you're doing this inside of a VM and you want to test the executable but get graphics issues due to a lack of hardware acceleration, you can try downloading the Mesa software renderer as a DLL. [Download the 64-bit version of the DLL here](https://fdossena.com/?p=mesa/index.frag) and put it next to the executable in `bin/`. It will be slow but should be usable for testing.
