// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
#include <cassert>

//! Base class of most objects of the Irrlicht Engine.
/** This class provides reference counting through the methods grab() and drop().
It also is able to store a debug string for every instance of an object.
Most objects of the Irrlicht
Engine are derived from IReferenceCounted, and so they are reference counted.

When you create an object in the Irrlicht engine, calling a method
which starts with 'create', an object is created, and you get a pointer
to the new object. If you no longer need the object, you have
to call drop(). This will destroy the object, if grab() was not called
in another part of you program, because this part still needs the object.
Note, that you only need to call drop() to the object, if you created it,
and the method had a 'create' in it.

A simple example:

If you want to create a texture, you may want to call an imaginable method
IDriver::createTexture. You call
ITexture* texture = driver->createTexture(dimension2d<u32>(128, 128));
If you no longer need the texture, call texture->drop().

If you want to load a texture, you may want to call imaginable method
IDriver::loadTexture. You do this like
ITexture* texture = driver->loadTexture("example.jpg");
You will not have to drop the pointer to the loaded texture, because
the name of the method does not start with 'create'. The texture
is stored somewhere by the driver.
*/
class IReferenceCounted
{
public:
	//! Constructor.
	IReferenceCounted() :
			ReferenceCounter(1)
	{
	}

	//! Destructor.
	virtual ~IReferenceCounted()
	{
	}

	// Reference counted objects can be neither copied nor moved.
	IReferenceCounted(const IReferenceCounted &) = delete;
	IReferenceCounted &operator=(const IReferenceCounted &) = delete;

	//! Grabs the object. Increments the reference counter by one.
	/** Someone who calls grab() to an object, should later also
	call drop() to it. If an object never gets as much drop() as
	grab() calls, it will never be destroyed. The
	IReferenceCounted class provides a basic reference counting
	mechanism with its methods grab() and drop(). Most objects of
	the Irrlicht Engine are derived from IReferenceCounted, and so
	they are reference counted.

	When you create an object in the Irrlicht engine, calling a
	method which starts with 'create', an object is created, and
	you get a pointer to the new object. If you no longer need the
	object, you have to call drop(). This will destroy the object,
	if grab() was not called in another part of you program,
	because this part still needs the object. Note, that you only
	need to call drop() to the object, if you created it, and the
	method had a 'create' in it.

	A simple example:

	If you want to create a texture, you may want to call an
	imaginable method IDriver::createTexture. You call
	ITexture* texture = driver->createTexture(dimension2d<u32>(128, 128));
	If you no longer need the texture, call texture->drop().
	If you want to load a texture, you may want to call imaginable
	method IDriver::loadTexture. You do this like
	ITexture* texture = driver->loadTexture("example.jpg");
	You will not have to drop the pointer to the loaded texture,
	because the name of the method does not start with 'create'.
	The texture is stored somewhere by the driver. */
	void grab() const { ++ReferenceCounter; }

	//! Drops the object. Decrements the reference counter by one.
	/** The IReferenceCounted class provides a basic reference
	counting mechanism with its methods grab() and drop(). Most
	objects of the Irrlicht Engine are derived from
	IReferenceCounted, and so they are reference counted.

	When you create an object in the Irrlicht engine, calling a
	method which starts with 'create', an object is created, and
	you get a pointer to the new object. If you no longer need the
	object, you have to call drop(). This will destroy the object,
	if grab() was not called in another part of you program,
	because this part still needs the object. Note, that you only
	need to call drop() to the object, if you created it, and the
	method had a 'create' in it.

	A simple example:

	If you want to create a texture, you may want to call an
	imaginable method IDriver::createTexture. You call
	ITexture* texture = driver->createTexture(dimension2d<u32>(128, 128));
	If you no longer need the texture, call texture->drop().
	If you want to load a texture, you may want to call imaginable
	method IDriver::loadTexture. You do this like
	ITexture* texture = driver->loadTexture("example.jpg");
	You will not have to drop the pointer to the loaded texture,
	because the name of the method does not start with 'create'.
	The texture is stored somewhere by the driver.
	\return True, if the object was deleted. */
	bool drop() const
	{
		// someone is doing bad reference counting.
		assert(ReferenceCounter > 0);

		--ReferenceCounter;
		if (!ReferenceCounter) {
			delete this;
			return true;
		}

		return false;
	}

	//! Get the reference count.
	/** \return Current value of the reference counter. */
	s32 getReferenceCount() const
	{
		return ReferenceCounter;
	}

private:

	//! The reference counter. Mutable to do reference counting on const objects.
	mutable s32 ReferenceCounter;
};
