set(DEFAULT_SDL2 ON)

option(USE_SDL2 "Use the SDL2 backend" ${DEFAULT_SDL2})

option(USE_SDL2_STATIC "Link with SDL2 static libraries" FALSE)

# Compiler flags

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_definitions(_DEBUG)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
	set(CMAKE_CXX_FLAGS_RELEASE "-O3")
	set(CMAKE_CXX_FLAGS_DEBUG "-g")

	add_compile_options(-Wall -pipe)

	# Enable SSE for floating point math on 32-bit x86 by default
	# reasoning see minetest issue #11810 and https://gcc.gnu.org/wiki/FloatingPointMath
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		include(CheckCXXSourceCompiles)
		check_cxx_source_compiles("#ifndef __i686__\n#error\n#endif\nint main(){}" IS_I686)
		if(IS_I686)
			message(STATUS "Detected Intel x86: using SSE instead of x87 FPU")
			add_compile_options(-mfpmath=sse -msse)
		endif()
	endif()

elseif(MSVC)
	string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " msvcrt.lib") # ???? fuck off

	add_compile_definitions(
		# Suppress some useless warnings
		_CRT_SECURE_NO_DEPRECATE
		# Get M_PI to work
		_USE_MATH_DEFINES
		# Don't define min/max macros in minwindef.h
		NOMINMAX
	)

	add_compile_options(/Zl)

	# Enable SSE for floating point math on 32-bit x86 by default
	# reasoning see minetest issue #11810 and https://gcc.gnu.org/wiki/FloatingPointMath
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		add_compile_options(/arch:SSE)
	endif()
endif()

# Platform-independent configuration (hard-coded currently)
add_compile_definitions(IRR_ENABLE_BUILTIN_FONT)

# Platform-specific configuration

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
	set(SOLARIS TRUE)
endif()

# Device

if(WIN32)
	add_compile_definitions(_IRR_WINDOWS_ _IRR_WINDOWS_API_)
	set(DEVICE "WINDOWS")
elseif(APPLE)
	add_compile_definitions(_IRR_OSX_PLATFORM_)
	set(DEVICE "OSX")
elseif(ANDROID)
	add_compile_definitions(_IRR_ANDROID_PLATFORM_)
	if(NOT USE_SDL2)
		message(FATAL_ERROR "The Android build requires SDL2")
	endif()
elseif(EMSCRIPTEN)
	add_compile_definitions(_IRR_EMSCRIPTEN_PLATFORM_ _IRR_COMPILE_WITH_EGL_MANAGER_)
	set(LINUX_PLATFORM TRUE)
	set(DEVICE "SDL")
elseif(SOLARIS)
	add_compile_definitions(_IRR_SOLARIS_PLATFORM_ _IRR_POSIX_API_)
	set(DEVICE "X11")
else()
	add_compile_definitions(_IRR_POSIX_API_)
	set(LINUX_PLATFORM TRUE)
	set(DEVICE "X11")
endif()

if(LINUX_PLATFORM)
	add_compile_definitions(_IRR_LINUX_PLATFORM_)
endif()

if(MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

if(USE_SDL2)
	set(DEVICE "SDL")
elseif(DEVICE STREQUAL "SDL")
	message(FATAL_ERROR "SDL was used but not enabled?!")
endif()

add_compile_definitions("_IRR_COMPILE_WITH_${DEVICE}_DEVICE_")

# X11

if(DEVICE STREQUAL "X11")
	option(USE_X11 "Use X11" TRUE)
else()
	set(USE_X11 FALSE)
endif()

if(LINUX_PLATFORM AND USE_X11)
	option(USE_XINPUT2 "Use XInput2" TRUE)
	option(USE_XCURSOR "Use XCursor" FALSE)
else()
	set(USE_XINPUT2 FALSE)
	set(USE_XCURSOR FALSE)
endif()

# Joystick

if(NOT (BSD OR SOLARIS OR EMSCRIPTEN))
	add_compile_definitions(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
endif()

# OpenGL

if(USE_SDL2)
	if(NOT ANDROID)
		set(DEFAULT_OPENGL3 TRUE)
	endif()
else()
	set(DEFAULT_OPENGL3 FALSE)
endif()
option(ENABLE_OPENGL3 "Enable OpenGL 3+" ${DEFAULT_OPENGL3})

if(ANDROID OR EMSCRIPTEN)
	set(ENABLE_OPENGL FALSE)
else()
	option(ENABLE_OPENGL "Enable OpenGL" TRUE)
endif()

if(APPLE)
	set(ENABLE_GLES2 FALSE)
	set(ENABLE_WEBGL1 FALSE)
else()
	if(ANDROID OR EMSCRIPTEN)
		set(DEFAULT_GLES2 TRUE)
	endif()
	if(EMSCRIPTEN)
		set(DEFAULT_WEBGL1 TRUE)
	endif()
	option(ENABLE_GLES2 "Enable OpenGL ES 2+" ${DEFAULT_GLES2})
	#option(ENABLE_WEBGL1 "Enable WebGL (requires GLES2)" ${DEFAULT_WEBGL1})
	set(ENABLE_WEBGL1 FALSE) # not working currently
	if(ENABLE_WEBGL1)
		set(ENABLE_GLES2 TRUE)
	endif()
endif()

if(ENABLE_OPENGL OR (ENABLE_OPENGL3 AND NOT USE_SDL2))
	if(ENABLE_OPENGL)
		add_compile_definitions(_IRR_COMPILE_WITH_OPENGL_)
		set(OPENGL_DIRECT_LINK TRUE) # driver relies on this
	endif()
	if(DEVICE STREQUAL "WINDOWS")
		add_compile_definitions(_IRR_COMPILE_WITH_WGL_MANAGER_)
	elseif(DEVICE STREQUAL "X11")
		add_compile_definitions(_IRR_COMPILE_WITH_GLX_MANAGER_)
	elseif(DEVICE STREQUAL "OSX")
		add_compile_definitions(_IRR_COMPILE_WITH_NSOGL_MANAGER_)
	endif()
endif()

if(ENABLE_OPENGL3)
	if(DEVICE STREQUAL "WINDOWS")
		# supported
	elseif(DEVICE STREQUAL "X11")
		# supported
	elseif (NOT USE_SDL2)
		message(FATAL_ERROR "OpenGL 3 driver requires SDL2")
	endif()
endif()

if(ENABLE_GLES2)
	add_compile_definitions(_IRR_COMPILE_WITH_OGLES2_)
	if(DEVICE MATCHES "^(WINDOWS|X11)$" OR EMSCRIPTEN)
		add_compile_definitions(_IRR_COMPILE_WITH_EGL_MANAGER_)
	endif()
endif()

if(ENABLE_WEBGL1)
	add_compile_definitions(_IRR_COMPILE_WITH_WEBGL1_)
endif()

# Misc

include(TestBigEndian)
TEST_BIG_ENDIAN(BIG_ENDIAN)
if(BIG_ENDIAN)
	add_compile_definitions(__BIG_ENDIAN__)
endif()

# Configuration report

message(STATUS "Device: ${DEVICE}")
message(STATUS "OpenGL: ${ENABLE_OPENGL}")
message(STATUS "OpenGL 3: ${ENABLE_OPENGL3}")
if (ENABLE_GLES2)
	message(STATUS "OpenGL ES 2: ON (unified)")
else()
	message(STATUS "OpenGL ES 2: OFF")
endif()
#message(STATUS "WebGL: ${ENABLE_WEBGL1}")

# Required libs

include(CheckCXXSourceCompiles)

find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)

if(ENABLE_GLES2)
	find_package(OpenGLES2 REQUIRED)
endif()
if(ENABLE_OPENGL)
	find_package(OpenGL REQUIRED)
endif()
set(USE_SDL2_SHARED FALSE)
if(USE_SDL2)
	if(NOT USE_SDL2_STATIC)
		set(USE_SDL2_SHARED TRUE)
	endif()
	if(NOT ANDROID)
		find_package(SDL2 REQUIRED)
	else()
		# provided by AndroidLibs.cmake
	endif()
	message(STATUS "Found SDL2: ${SDL2_LIBRARIES}")

	# unfortunately older SDL does not provide its version to cmake, so check header.
	set(CMAKE_REQUIRED_INCLUDES ${SDL2_INCLUDE_DIRS})
	unset(CHECK_SDL_VERSION CACHE)
	check_cxx_source_compiles("#include <SDL_version.h>\n\
#if !SDL_VERSION_ATLEAST(2, 0, 10)\n\
#error\n\
#endif\n\
int main() {}" CHECK_SDL_VERSION)
	if(NOT CHECK_SDL_VERSION)
		message(FATAL_ERROR "SDL2 is too old, required is at least 2.0.10!")
	endif()

	# ...no target either.
	if(NOT TARGET SDL2::SDL2)
		add_library(SDL2::SDL2 SHARED IMPORTED)
		set_target_properties(SDL2::SDL2 PROPERTIES
			IMPORTED_LOCATION "${SDL2_LIBRARIES}")
	endif()
endif()

# More special config

if(ENABLE_OPENGL AND DEVICE STREQUAL "SDL")
	# The legacy GL driver requires some symbols from GL 4.5 to compile,
	# which SDL only provides since 2.26.0 (Nov 2022).
	# We have a fallback in case this isn't satisfied so test for it.
	set(CMAKE_REQUIRED_INCLUDES ${SDL2_INCLUDE_DIRS})
	unset(CHECK_GL_VERSION_4_5 CACHE)
	check_cxx_source_compiles("#include <SDL_opengl.h>\n\
#ifndef GL_VERSION_4_5\n\
#error\n\
#endif\n\
int main() {}" CHECK_GL_VERSION_4_5)
	if(CHECK_GL_VERSION_4_5)
		add_compile_definitions(IRR_PREFER_SDL_GL_HEADER)
	endif()
endif()

# Platform-specific libs

if(ANDROID)
	enable_language(C)
elseif(APPLE)
	find_library(COCOA_LIB Cocoa REQUIRED)
	find_library(IOKIT_LIB IOKit REQUIRED)

	add_compile_definitions(GL_SILENCE_DEPRECATION)
elseif(NOT USE_SDL2)
	# Unix probably
	find_package(X11 REQUIRED)
	if(USE_XINPUT2 AND NOT X11_Xi_FOUND)
		message(FATAL_ERROR "XInput not found")
	endif()
endif()

set(link_includes
	"${PROJECT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}"

	"${ZLIB_INCLUDE_DIR}"
	"${JPEG_INCLUDE_DIR}"
	"${PNG_INCLUDE_DIR}"
	"$<$<BOOL:${USE_SDL2}>:${SDL2_INCLUDE_DIRS}>"

	${OPENGL_INCLUDE_DIR}
	${OPENGLES2_INCLUDE_DIR}
	${EGL_INCLUDE_DIR}

	"$<$<BOOL:${USE_X11}>:${X11_INCLUDE_DIR}>"
)

# Source files

set(IRRMESHLOADER
	CB3DMeshFileLoader.h
	CGLTFMeshFileLoader.h
	COBJMeshFileLoader.h
	CXMeshFileLoader.h

	CB3DMeshFileLoader.cpp
	CGLTFMeshFileLoader.cpp
	COBJMeshFileLoader.cpp
	CXMeshFileLoader.cpp
)

add_library(IRRMESHOBJ OBJECT
	CMeshSceneNode.h
	CAnimatedMeshSceneNode.h

	SkinnedMesh.cpp
	CMeshSceneNode.cpp
	CAnimatedMeshSceneNode.cpp

	${IRRMESHLOADER}
)

target_link_libraries(IRRMESHOBJ PUBLIC tiniergltf::tiniergltf)

set(IRRDRVROBJ
	CNullDriver.h
	CGLXManager.h
	CWGLManager.h
	CEGLManager.h
	CSDLManager.h

	CNullDriver.cpp
	CGLXManager.cpp
	CWGLManager.cpp
	CEGLManager.cpp
	CSDLManager.cpp
	mt_opengl_loader.cpp
)

# the two legacy drivers

if(ENABLE_OPENGL)
	set(IRRDRVROBJ
		${IRRDRVROBJ}
		COpenGLCacheHandler.h
		COpenGLDriver.h
		COpenGLSLMaterialRenderer.h
		COpenGLExtensionHandler.h

		COpenGLCacheHandler.cpp
		COpenGLDriver.cpp
		COpenGLSLMaterialRenderer.cpp
		COpenGLExtensionHandler.cpp
	)
endif()

# the unified drivers

if(ENABLE_OPENGL3 OR ENABLE_GLES2)
	file(GLOB IRRDRVR_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/OpenGL/*.h")

	set(IRRDRVROBJ
		${IRRDRVROBJ}
		${IRRDRVR_HDRS}
		OpenGL/Driver.cpp
		OpenGL/ExtensionHandler.cpp
		OpenGL/FixedPipelineRenderer.cpp
		OpenGL/MaterialRenderer.cpp
		OpenGL/Renderer2D.cpp
		OpenGL/VBO.cpp
	)
endif()

if(ENABLE_OPENGL3)
	set(IRRDRVROBJ
		${IRRDRVROBJ}
		OpenGL3/DriverGL3.h
		OpenGL3/DriverGL3.cpp
	)
endif()

if(ENABLE_GLES2)
	set(IRRDRVROBJ
		${IRRDRVROBJ}
		OpenGLES2/DriverGLES2.h
		OpenGLES2/DriverGLES2.cpp
	)
endif()

set(IRRIMAGEOBJ
	CColorConverter.h
	CImage.h
	CImageLoaderJPG.h
	CImageLoaderPNG.h
	CImageLoaderTGA.h
	CImageWriterJPG.h
	CImageWriterPNG.h

	CColorConverter.cpp
	CImage.cpp
	CImageLoaderJPG.cpp
	CImageLoaderPNG.cpp
	CImageLoaderTGA.cpp
	CImageWriterJPG.cpp
	CImageWriterPNG.cpp
)

add_library(IRRVIDEOOBJ OBJECT
	CFPSCounter.h
	CFPSCounter.cpp
	${IRRDRVROBJ}
	${IRRIMAGEOBJ}
)

add_library(IRRIOOBJ OBJECT
	CFileList.h
	CFileSystem.h
	CLimitReadFile.h
	CMemoryFile.h
	CReadFile.h
	CWriteFile.h
	CZipReader.h

	CFileList.cpp
	CFileSystem.cpp
	CLimitReadFile.cpp
	CMemoryFile.cpp
	CReadFile.cpp
	CWriteFile.cpp
	CZipReader.cpp
)

add_library(IRROTHEROBJ OBJECT
	CIrrDeviceSDL.h
	CIrrDeviceLinux.h
	CIrrDeviceStub.h
	CIrrDeviceWin32.h
	CLogger.h
	COSOperator.h
	os.h

	CIrrDeviceSDL.cpp
	CIrrDeviceLinux.cpp
	CIrrDeviceStub.cpp
	CIrrDeviceWin32.cpp
	CLogger.cpp
	COSOperator.cpp
	Irrlicht.cpp
	os.cpp
)

if(ENABLE_OPENGL3)
	target_compile_definitions(IRROTHEROBJ PRIVATE ENABLE_OPENGL3)
endif()

if(APPLE)
	# Build all IRROTHEROBJ sources as objc++, including the .cpp's
	set_target_properties(IRROTHEROBJ PROPERTIES COMPILE_OPTIONS "-xobjective-c++")
	target_sources(IRROTHEROBJ PRIVATE
		CIrrDeviceOSX.mm
		CNSOGLManager.mm
	)
endif()

if(USE_X11)
	target_compile_definitions(IRROTHEROBJ PRIVATE _IRR_COMPILE_WITH_X11_)
endif()

if(USE_XINPUT2)
	target_compile_definitions(IRROTHEROBJ PRIVATE _IRR_LINUX_X11_XINPUT2_)
endif()

if(USE_XCURSOR)
	target_compile_definitions(IRROTHEROBJ PRIVATE _IRR_LINUX_XCURSOR_)
endif()

add_library(IRRGUIOBJ OBJECT
	CGUIButton.h
	CGUICheckBox.h
	CGUIComboBox.h
	CGUIEnvironment.h
	CGUIFileOpenDialog.h
	CGUIFont.h
	CGUIImage.h
	CGUIListBox.h
	CGUIScrollBar.h
	CGUISkin.h
	CGUIStaticText.h
	CGUITabControl.h
	CGUISpriteBank.h
	CGUIImageList.h

	CGUIButton.cpp
	CGUICheckBox.cpp
	CGUIComboBox.cpp
	CGUIEditBox.cpp
	CGUIEnvironment.cpp
	CGUIFileOpenDialog.cpp
	CGUIFont.cpp
	CGUIImage.cpp
	CGUIListBox.cpp
	CGUIScrollBar.cpp
	CGUISkin.cpp
	CGUIStaticText.cpp
	CGUITabControl.cpp
	CGUISpriteBank.cpp
	CGUIImageList.cpp
)

# Library

file(GLOB_RECURSE IrrlichtMt_PUBLIC_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/../include/*.h")

# There have to be some sources in IrrlichtMt to workaround Cmake Xcode generator bug
add_library(IrrlichtMt STATIC
	${IrrlichtMt_PUBLIC_HDRS}

	CBillboardSceneNode.h
	CCameraSceneNode.h
	CDummyTransformationSceneNode.h
	CEmptySceneNode.h
	CMeshManipulator.h
	CSceneCollisionManager.h
	CSceneManager.h
	CMeshCache.h

	CBillboardSceneNode.cpp
	CCameraSceneNode.cpp
	CDummyTransformationSceneNode.cpp
	CEmptySceneNode.cpp
	CMeshManipulator.cpp
	CSceneCollisionManager.cpp
	CSceneManager.cpp
	CMeshCache.cpp
)
foreach(object_lib
	IRRMESHOBJ IRRVIDEOOBJ
	IRRIOOBJ IRROTHEROBJ IRRGUIOBJ)
	# Set include directories for object library compilation
	target_include_directories(${object_lib} PRIVATE ${link_includes})
	if(CMAKE_GENERATOR STREQUAL "Xcode")
		# Workaround for Cmake Xcode project generator
		target_link_libraries(IrrlichtMt PRIVATE ${object_lib})
	else()
		# Add objects from object library to main library
		target_sources(IrrlichtMt PRIVATE $<TARGET_OBJECTS:${object_lib}>)
	endif()

	if(BUILD_WITH_TRACY)
		target_link_libraries(${object_lib} PRIVATE Tracy::TracyClient)
	endif()
endforeach()

# Alias target provides add_submodule compatibility
add_library(IrrlichtMt::IrrlichtMt ALIAS IrrlichtMt)

target_include_directories(IrrlichtMt
	PUBLIC
		"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
	PRIVATE
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
		${link_includes}
)

# this needs to be here and not in a variable (like link_includes) due to issues
# with the generator expressions on at least CMake 3.22, but not 3.28 or later
target_link_libraries(IrrlichtMt PRIVATE
	tiniergltf::tiniergltf
	${ZLIB_LIBRARY}
	${JPEG_LIBRARY}
	${PNG_LIBRARY}
	"$<$<BOOL:${USE_SDL2_SHARED}>:SDL2::SDL2>"
	"$<$<BOOL:${USE_SDL2_STATIC}>:SDL2::SDL2-static>"

	"$<$<BOOL:${OPENGL_DIRECT_LINK}>:${OPENGL_LIBRARIES}>"
	${EGL_LIBRARY}

	# incl. transitive SDL2 dependencies for static linking
	"$<$<PLATFORM_ID:Android>:-landroid -llog -lGLESv2 -lGLESv1_CM -lOpenSLES>"
	${COCOA_LIB}
	${IOKIT_LIB}
	"$<$<PLATFORM_ID:Windows>:gdi32>"
	"$<$<PLATFORM_ID:Windows>:winmm>"
	"$<$<BOOL:${USE_X11}>:${X11_X11_LIB}>"
	"$<$<BOOL:${USE_X11}>:${X11_Xi_LIB}>"
)

if(WIN32)
	target_compile_definitions(IrrlichtMt INTERFACE _IRR_WINDOWS_API_)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(IrrlichtMt INTERFACE _DEBUG)
endif()
if(APPLE OR ANDROID OR EMSCRIPTEN)
	target_compile_definitions(IrrlichtMt PUBLIC IRR_MOBILE_PATHS)
endif()
