/* irrlicht.h -- interface of the 'Irrlicht Engine'

  Copyright (C) 2002-2012 Nikolaus Gebhardt

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgment in the product documentation would be
	 appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Please note that the Irrlicht Engine is based in part on the work of the
  Independent JPEG Group, the zlib and the libPng. This means that if you use
  the Irrlicht Engine in your product, you must acknowledge somewhere in your
  documentation that you've used the IJG code. It would also be nice to mention
  that you use the Irrlicht Engine, the zlib and libPng. See the README files
  in the jpeglib, the zlib and libPng for further information.
*/

#pragma once

#include "IrrlichtDevice.h"
#include "dimension2d.h"
#include "EDriverTypes.h"
#include "IEventReceiver.h"
#include "irrTypes.h"
#include "SIrrCreationParameters.h"

//! Creates an Irrlicht device with the option to specify advanced parameters.
/** Usually you should used createDevice() for creating an Irrlicht Engine device.
Use this function only if you wish to specify advanced parameters like a window
handle in which the device should be created.
\param parameters: Structure containing advanced parameters for the creation of the device.
See SIrrlichtCreationParameters for details.
\return Returns pointer to the created IrrlichtDevice or null if the
device could not be created. */
extern "C" IrrlichtDevice *createDeviceEx(
		const SIrrlichtCreationParameters &parameters);

// THE FOLLOWING IS AN EMPTY LIST OF ALL SUB NAMESPACES
// EXISTING ONLY FOR THE DOCUMENTATION SOFTWARE DOXYGEN.

//! Basic classes such as vectors, planes, arrays, lists, and so on can be found in this namespace.
namespace core
{
}

//! The gui namespace contains useful classes for easy creation of a graphical user interface.
namespace gui
{
}

//! This namespace provides interfaces for input/output: Reading and writing files, accessing zip archives, ...
namespace io
{
}

//! All scene management can be found in this namespace: Mesh loading, special scene nodes like octrees and billboards, ...
namespace scene
{
}

//! The video namespace contains classes for accessing the video driver. All 2d and 3d rendering is done here.
namespace video
{
}

/*! \file irrlicht.h
	\brief Main header file of the irrlicht, needed to create a device.
*/
