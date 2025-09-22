# Improving build times

This page contains useful tips for improving build times when compiling Luanti. This is useful when you are repeatedly building Luanti for testing or if you want to make incremental builds as quick as possible.

The instructions assume you are on Linux, as that's where you can usually expect the best compile times in general.

## Use Clang

Luanti supports building with the Clang compiler, which generally compiles somewhat faster compared to GCC. You should be able to install Clang from your Linux distribution's package manager and then make CMake build with Clang instead of GCC with the following build options:

```bash
-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
```

## Use mold

`mold` is a linker that runs much faster than the standard `ld` or even the `lld` linker.

```bash
-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold" -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=mold"
```

## Use ninja

Ninja is a build system that aims to have less overhead than regular makefiles, which should make it slightly faster. CMake supports generating Ninja build files by using `-G Ninja` when first generating a build directory. You cannot change the generator in an already created build folder.

Do note that Ninja will automatically detect your processor count and set the amount of jobs if you don't manually set anything. This might be an issue if you want to do something while compiling in the background. You can check what is the default with `ninja --help` and manually pass the job count like one would do with make: `ninja -j4`.

## Android: Disabling unused ABIs

When building Luanti for Android by default it will build for all four supported ABIs, meaning it will build Luanti four times over. If you are doing testing and know the device you are using you can save build times by limiting it to only building a single ABI that works on the device.

You can change the ABI filter `abiFilters` in `android/native/build.gradle`. Below is an explanation of each ABI listed:

| ABI ID        | ABI name   | Where you may need it                   |
| ------------- | ---------- | --------------------------------------- |
| `armeabi-v7a` | 32-bit ARM | Very old phones                         |
| `arm64-v8a`   | 64-bit ARM | Modern phones (likely the one you have) |
| `x86`         | 32-bit x86 | Android SDK emulator (x86 images)       |
| `x86_64`      | 64-bit x86 | Android SDK emulator (x86_64 images)    |

## Luanti compile option tweaks

These may reduce build times further, but at cost of some functionality being missing.

- `-DENABLE_LTO=OFF` to disable LTO which may make linking take longer than expected. This is disabled by default in Debug builds.
- `-DBUILD_UNITTESTS=OFF` which may save some time when doing full rebuilds. May not be useful if you want to actually run the unit tests.
- `-ENABLE_GETTEXT=OFF` disables PO->MO compilation, if this takes a while to finish. Translations will be disabled in the resulting build.

## When in a hurry

Once you have cloned the Luanti repository:

```bash
mkdir build; cd build
cmake .. -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold" -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=mold" -G Ninja
ninja
```
