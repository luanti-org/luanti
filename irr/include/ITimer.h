// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"

//! Interface for getting and manipulating the virtual time
class ITimer : public virtual IReferenceCounted
{
public:
	//! Returns current real time in milliseconds of the system.
	/** This value does not start with 0 when the application starts.
	For example in one implementation the value returned could be the
	amount of milliseconds which have elapsed since the system was started.
	*/
	virtual u32 getRealTime() const = 0;

	//! Returns current virtual time in milliseconds.
	/** This value starts with 0 and can be manipulated using setTime(),
	stopTimer(), startTimer(), etc. This value depends on the set speed of
	the timer if the timer is stopped, etc. If you need the system time,
	use getRealTime() */
	virtual u32 getTime() const = 0;

	//! sets current virtual time
	virtual void setTime(u32 time) = 0;

	//! Stops the virtual timer.
	/** The timer is reference counted, which means everything which calls
	stop() will also have to call start(), otherwise the timer may not
	start/stop correctly again. */
	virtual void stop() = 0;

	//! Starts the virtual timer.
	/** The timer is reference counted, which means everything which calls
	stop() will also have to call start(), otherwise the timer may not
	start/stop correctly again. */
	virtual void start() = 0;

	//! Sets the speed of the timer
	/** The speed is the factor with which the time is running faster or
	slower then the real system time. */
	virtual void setSpeed(f32 speed = 1.0f) = 0;

	//! Returns current speed of the timer
	/** The speed is the factor with which the time is running faster or
	slower then the real system time. */
	virtual f32 getSpeed() const = 0;

	//! Returns if the virtual timer is currently stopped
	virtual bool isStopped() const = 0;

	//! Advances the virtual time
	/** Makes the virtual timer update the time value based on the real
	time. This is called automatically when calling IrrlichtDevice::run(),
	but you can call it manually if you don't use this method. */
	virtual void tick() = 0;
};
