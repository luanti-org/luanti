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
#include "IrrCompileConfig.h" // for IRRLICHT_API and IRRCALLCONV

/*! \mainpage Irrlicht Engine 1.9 API documentation
 *
 * <div align="center"><img src="logobig.png" ></div>
 *
 * \section intro Introduction
 *
 * Welcome to the Irrlicht Engine API documentation.
 * Here you'll find any information you'll need to develop applications with
 * the Irrlicht Engine. If you are looking for a tutorial on how to start, you'll
 * find some on the homepage of the Irrlicht Engine at
 * <A HREF="http://irrlicht.sourceforge.net" >irrlicht.sourceforge.net</A>
 * or inside the SDK in the examples directory.
 *
 * The Irrlicht Engine is intended to be an easy-to-use 3d engine, so
 * this documentation is an important part of it. If you have any questions or
 * suggestions, just send a email to the author of the engine, Nikolaus Gebhardt
 * (niko (at) irrlicht3d.org).
 *
 *
 * \section links Links
 *
 * <A HREF="namespaces.html">Namespaces</A>: A very good place to start reading
 * the documentation.<BR>
 * <A HREF="annotated.html">Class list</A>: List of all classes with descriptions.<BR>
 * <A HREF="functions.html">Class members</A>: Good place to find forgotten features.<BR>
 *
 * \section irrexample Short example
 *
 * A simple application, starting up the engine, loading a Quake 2 animated
 * model file and the corresponding texture, animating and displaying it
 * in front of a blue background and placing a user controlable 3d camera
 * would look like the following code. I think this example shows the usage
 * of the engine quite well:
 *
 * \code
 * #include <irrlicht.h>
 * // include a bunch of other stuff...
 *
 * using namespace irr;
 *
 * int main()
 * {
 *	// start up the engine
 *	IrrlichtDevice *device = createDevice(video::EDT_OPENGL,
 *		core::dimension2d<u32>(640,480));
 *
 *	video::IVideoDriver* driver = device->getVideoDriver();
 *	scene::ISceneManager* scenemgr = device->getSceneManager();
 *
 *	device->setWindowCaption(L"Hello World!");
 *
 *	// load and show quake2 .md2 model
 *	scene::ISceneNode* node = scenemgr->addAnimatedMeshSceneNode(
 *		scenemgr->getMesh("quake2model.md2"));
 *
 *	// if everything worked, add a texture and disable lighting
 *	if (node)
 *	{
 *		node->setMaterialTexture(0, driver->getTexture("texture.bmp"));
 *		node->setMaterialFlag(video::EMF_LIGHTING, false);
 *	}
 *
 *	// add a first person shooter style user controlled camera
 *	scenemgr->addCameraSceneNodeFPS();
 *
 *	// draw everything
 *	while(device->run() && driver)
 *	{
 *		driver->beginScene(video::ECBF_COLOR | video::ECBF_DEPTH, video::SColor(255,0,0,255));
 *		scenemgr->drawAll();
 *		driver->endScene();
 *	}
 *
 *	// delete device
 *	device->drop();
 *	return 0;
 * }
 * \endcode
 *
 * Irrlicht can load a lot of file formats automatically, see irr::scene::ISceneManager::getMesh()
 * for a detailed list. So if you would like to replace the simple blue screen background by
 * a cool Quake 3 Map, optimized by an octree, just insert this code
 * somewhere before the while loop:
 *
 * \code
 *	// add .pk3 archive to the file system
 *	device->getFileSystem()->addZipFileArchive("quake3map.pk3");
 *
 *	// load .bsp file and show it using an octree
 *	scenemgr->addOctreeSceneNode(
 *		scenemgr->getMesh("quake3map.bsp"));
 * \endcode
 *
 * As you can see, the engine uses namespaces. Everything in the engine is
 * placed into the namespace 'irr', but there are also 5 sub namespaces.
 * You can find a list of all namespaces with descriptions at the
 * <A HREF="namespaces.html"> namespaces page</A>.
 * This is also a good place to start reading the documentation. If you
 * don't want to write the namespace names all the time, just use all namespaces like
 * this:
 * \code
 * using namespace core;
 * using namespace scene;
 * using namespace video;
 * using namespace io;
 * using namespace gui;
 * \endcode
 *
 * There is a lot more the engine can do, but I hope this gave a short
 * overview over the basic features of the engine. For more examples, please take
 * a look into the examples directory of the SDK.
 */

//! Everything in the Irrlicht Engine can be found in this namespace.
namespace irr
{
//! Creates an Irrlicht device. The Irrlicht device is the root object for using the engine.
/** If you need more parameters to be passed to the creation of the Irrlicht Engine device,
use the createDeviceEx() function.
\param driverType: Type of the video driver to use.
\param windowSize: Size of the window or the video mode in fullscreen mode.
\param bits: Bits per pixel in fullscreen mode. Ignored if windowed mode.
\param fullscreen: Should be set to true if the device should run in fullscreen. Otherwise
	the device runs in windowed mode.
\param stencilbuffer: Specifies if the stencil buffer should be enabled. Set this to true,
if you want the engine be able to draw stencil buffer shadows. Note that not all
devices are able to use the stencil buffer. If they don't no shadows will be drawn.
\param vsync: Specifies vertical synchronization: If set to true, the driver will wait
for the vertical retrace period, otherwise not.
\param receiver: A user created event receiver.
\return Returns pointer to the created IrrlichtDevice or null if the
device could not be created.
*/
extern "C" IRRLICHT_API IrrlichtDevice *IRRCALLCONV createDevice(
		video::E_DRIVER_TYPE driverType = video::EDT_OPENGL,
		// parentheses are necessary for some compilers
		const core::dimension2d<u32> &windowSize = (core::dimension2d<u32>(640, 480)),
		u32 bits = 32,
		bool fullscreen = false,
		bool stencilbuffer = true,
		bool vsync = false,
		IEventReceiver *receiver = 0);

//! Creates an Irrlicht device with the option to specify advanced parameters.
/** Usually you should used createDevice() for creating an Irrlicht Engine device.
Use this function only if you wish to specify advanced parameters like a window
handle in which the device should be created.
\param parameters: Structure containing advanced parameters for the creation of the device.
See irr::SIrrlichtCreationParameters for details.
\return Returns pointer to the created IrrlichtDevice or null if the
device could not be created. */
extern "C" IRRLICHT_API IrrlichtDevice *IRRCALLCONV createDeviceEx(
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
}

/*! \file irrlicht.h
	\brief Main header file of the irrlicht, needed to create a device.
*/
