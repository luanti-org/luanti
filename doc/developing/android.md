# Android tips & tricks

## Sign the Android APK from CI

The [Github Actions Workflow](https://github.com/luanti-org/luanti/actions?query=workflow%3Aandroid+event%3Apush)
automatically produces an APK for each architecture.
Before installing them onto a device they however need to be signed.

This requires an installation of the Android SDK and `adb`.
```bash
.../android-sdk/build-tools/30.0.3/apksigner sign --ks ~/.android/debug.keystore \
	app-arm64-v8a-release-unsigned.apk
# Enter 'android' (without quotes) when asked for a password
```

Note that the `debug.keystore` will not exist if you have never compiled an
Android app on your system (probably).

After that installing it will work:
```bash
adb install -r -d ./app-arm64-v8a-release-unsigned.apk
```

## How to get debug output from Luanti on Android

In case debug.txt isn't enough (e.g. when debugging a crash), you can get debug
output using logcat:

`adb logcat -s Luanti '*:F'`

Note that you can do this even *after* the app has crashed,
since Android keeps an internal buffer.

A segmentation fault for example looks like this:

```
01-10 17:20:22.215 19308 20560 F libc    : Fatal signal 6 (SIGABRT), code -1 (SI_QUEUE) in tid 20560 (MinetestNativeT), pid 19308 (netest.minetest)
01-10 17:20:22.287 20576 20576 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
01-10 17:20:22.287 20576 20576 F DEBUG   : Build fingerprint: '...'
01-10 17:20:22.287 20576 20576 F DEBUG   : Revision: '4'
01-10 17:20:22.287 20576 20576 F DEBUG   : ABI: 'arm64'
01-10 17:20:22.288 20576 20576 F DEBUG   : Timestamp: 2024-01-10 17:20:22+0100
01-10 17:20:22.288 20576 20576 F DEBUG   : pid: 19308, tid: 20560, name: MinetestNativeT  >>> net.minetest.minetest <<<
01-10 17:20:22.288 20576 20576 F DEBUG   : uid: 10385
01-10 17:20:22.288 20576 20576 F DEBUG   : signal 6 (SIGABRT), code -1 (SI_QUEUE), fault addr --------
[ ... more information follows ... ]
```

If you want get rid of previous output you can do that with `adb logcat -c`.

## I edited builtin, shaders, ... but nothing changed. Help!

You're probably hitting two problems:
* the build system only generates assets once
* the app only re-extracts assets when the version changes

Force regenerating the assets: `./gradlew app:clean`

Erase the app's memory of which version was installed: `adb shell run-as net.minetest.minetest rm shared_prefs/MinetestSettings.xml`

If this doesn't work you can also uninstall it using `adb shell pm uninstall net.minetest.minetest`. You will obviously lose your data.

Then build and install as normal and your changes should be applied.
