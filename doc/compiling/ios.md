# Compiling for iOS

## Requirements

- macOS
- [Homebrew](https://brew.sh/)
- Xcode
- iOS/iPhoneSimulator SDK
- iOS Simulator for newest iOS is supported only on Apple Silicon devices.

Install dependencies with homebrew:

```
brew install cmake git
```

## Prebuild Luanti dependencies

For manual build, use `build.sh` script from cloned repository https://github.com/luanti-org/luanti_ios_deps

- iPhoneSimulator
	`./build.sh /dir/to/download/sources /dir/to/instal/built/deps iPhoneSimulator iPhoneSimulatorSDKVersion all`
	(Replace iPhoneSimulatorSDKVersion with your installed SDK version, e.g. 26.2)

- iPhoneOS
	`./build.sh /dir/to/download/sources /dir/to/instal/built/deps iPhoneOS iPhoneOSSDKVersion all`
	(Replace iPhoneOSSDKVersion with your installed SDK version, e.g. 26.2)

## Generate Xcode project

Clone Luanti git repository.
And do something like:

```bash
mkdir build
cd build

DEPS_DIR=/absolute/path/to/ios/precompiled/deps
REPDIR=/absolute/path/to/luanti/repository

export CMAKE_PREFIX_PATH=${DEPS_DIR}
export SDKROOT="/path/to/sdk/for/iPhoneOS/or/iPhoneSimulator"

cmake .. \
	-DCMAKE_SYSTEM_NAME=iOS \
	# see util/ci/build_ios.sh for full parameter list
```

### Build and Run for developing purposes

* Open generated Xcode project
* Select scheme `luanti`
* Configure signing Team etc.
* If building for simulator, select simulator target device
* Run Build command
* Run app from Xcode

### Example of automated build from CI

Script [build_ios.sh](util/ci/build_ios.sh) called from [ios.yml](.github/workflows/ios.yml).
