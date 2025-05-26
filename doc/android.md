# Luanti Android build
All Luanti builds, including the Android variant, are based on the same code.
However, additional Java code is used for proper Android integration.

## Controls

While you're playing the game normally (that is, no menu or inventory is
shown), the following controls are available:
* Look around: touch screen and slide finger
* Tap: Place a node, punch an object or use the selected item (default)
* Long tap: Dig a node or use the selected item (default)
* Press back: Pause menu
* Touch buttons: Press button
* Buttons:

1. Left upper corner: Chat
2. Right lower corner: Jump
3. Right lower corner: Crouch
4. Left lower corner (Joystick): Walk/step...
5. Left lower corner: Display inventory

When a menu or inventory is displayed:
* Double tap outside menu area: Close menu
* Press back: Close menu
* Tap on an item stack: Select that stack
* Tap on an empty slot: If a stack are selected already, that stack is placed here
* Drag and drop: Touch stack and hold finger down, move the stack to another
  slot, tap another finger while keeping first finger on screen
  --> places a single item from dragged stack into current (first touched) slot. If a stack is selected, the stack will be split as half and one of the splitted stack will be selected

### Limitations
* Some old Android device only support 2 touch at a time, some game/mod contain button combination that need 3 touch (example: jump + Aux1 + hold)
* Complicated control can be difficult or impossible on Android device

## File Path
There are some settings especially useful for Android users. The Luanti-wide
configuration file can usually be found at:

* Before 5.4.2:
    * `/sdcard/Minetest/` or `/storage/emulated/0/` if stored on the device
    * `/storage/emulated/(varying folder name)/` if stored on an SD card
* After 5.4.2:
    * `/sdcard/Android/data/net.minetest.minetest/` or `/storage/emulated/0/Android/data/net.minetest.minetest/` if stored on the device
    * `/storage/emulated/(varying folder name)/Android/data/net.minetest.minetest/` if stored on the SD card
* [Learn more about Android directory](https://docs.luanti.org/for-players/mobile/)

## Useful settings

### gui_scaling
this is a user-specified scaling factor for the GUI in case main menu is too big or small on your device, try changing this value.

### mapblock_limit
Mobile device generally have less RAM than PC, this setting limit how many mapblock can keep in RAM

### fps_limit
this setting limit max FPS (Frame per second). Default value is 60, which lowest Android device screen refresh rate commonly found, but if you're using an device have lower refresh rate, change this

## Requirements
The recommended system requirements for Luanti are listed below.

### CPU
Supported architectures:
1. ARMv7
2. AArch64
3. x86
4. x86_64

### Recommended
1. Graphics API: OpenGL ES 2.0
2. Android version: Android 5 (API Level 21) or newer
3. Free RAM: 1 GB
4. Free storage: 500 MB

## Rendering
Unlike on PC, Android devices use OpenGL ES which less powerful than OpenGL, thus
some shader settings cannot be used on OpenGL ES.
Changing the graphic driver setting to OpenGL will not work.

## Building Requirements
In order to build, your PC has to be set up to build Luanti in the usual
manner (see the regular Luanti documentation for how to get this done).
In addition to what is required for Luanti in general, you will need the
following software packages. The version number in parenthesis denotes the
version that was tested at the time this README was drafted; newer/older
versions may or may not work.

* Android SDK 29
* Android NDK r21
* Android Studio 3 [optional]

Additionally, you'll need to have an Internet connection available on the
build system, as the Android build will download some source packages.

## Build
The new build system Luanti Android is fully functional and is designed to
speed up and simplify the work, as well as adding the possibility of
cross-platform build.
You can use `./gradlew assemblerelease` or `./gradlew assembledebug` from the
command line or use Android Studio and click the build button.

When using gradlew, the newest NDK will be downloaded and installed
automatically. Or you can create a `local.properties` file and specify
`sdk.dir` and `ndk.dir` yourself.

* In order to make a release build you'll have to have a keystore setup to sign
  the resulting apk package. How this is done is not part of this README. There
  are different tutorials on the web explaining how to do it
  - choose one yourself.

* Once your keystore is setup, enter the android subdirectory and create a new
  file "ant.properties" there. Add the following lines to that file:

  > key.store=<path to your keystore>
  > key.alias=Minetest
