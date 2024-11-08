IrrlichtMt version 1.9
======================

IrrlichtMt is the 3D engine of [Minetest](https://github.com/minetest).
It is based on the [Irrlicht Engine](https://irrlicht.sourceforge.io/) but is now developed independently.
It is intentionally not compatible to upstream and is planned to be eventually absorbed into Minetest.

Build
-----

The build system is CMake.

The following libraries are required to be installed:
* zlib, libPNG, libJPEG
* OpenGL
  * or on mobile: OpenGL ES (can be optionally enabled on desktop too)
* on Unix: X11
* SDL2 (see below)

Aside from standard search options (`ZLIB_INCLUDE_DIR`, `ZLIB_LIBRARY`, ...) the following options are available:
* `ENABLE_OPENGL` - Enable OpenGL driver
* `ENABLE_OPENGL3` (default: `OFF`) - Enable OpenGL 3+ driver
* `ENABLE_GLES2` - Enable OpenGL ES 2+ driver
* `USE_SDL2` (default: ON for Android, OFF for other platforms) - Use SDL2 instead of older native device code

However, IrrlichtMt cannot be built or installed separately.

Platforms
---------

We aim to support these platforms:
* Windows via MinGW
* Linux (GL or GLES)
* macOS
* Android

This doesn't mean other platforms don't work or won't be supported, if you find something that doesn't work contributions are welcome.

License
-------

The license of the Irrlicht Engine is based on the zlib/libpng license and applies to this fork, too.

	The Irrlicht Engine License
	===========================

	Copyright (C) 2002-2012 Nikolaus Gebhardt

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgement in the product documentation would be
	 appreciated but is not required.
	2. Altered source versions must be clearly marked as such, and must not be
	 misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
