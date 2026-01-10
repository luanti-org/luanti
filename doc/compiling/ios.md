# Compiling for iOS

THIS DOCUMENT IS NOT FINISHED!!!

## Requirements

- macOS
- [Homebrew](https://brew.sh/)
- XCode
- iOS/IPhoneSimulator SDK
- iOS Simulator for newest iOS is supported only on Apple Silicon deviecs.

Install dependencies with homebrew:

```
brew install cmake git
```

## Generate Xcode project

This script will download and build all requested dependencies for iOS or IPhoneSimulator and generate Xcode project for Luanti.

```bash
/path/to/ios_build_with_deps.sh https://github.com/luanti-org/luanti.git master ../luanti_ios ../luanti_ios_deps Debug "iPhoneSimulator" "18.2" "all"
```

### Build and Run

* Open generated Xcode project
* Select scheme `luanti`
* Configure signing Team etc.
* Run Build command
* Open application from `build/build/Debug/` directory or run it from Xcode

