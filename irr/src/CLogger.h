// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ILogger.h"
#include "os.h"
#include "irrString.h"
#include "IEventReceiver.h"

//! Class for logging messages, warnings and errors to stdout
class CLogger : public ILogger
{
public:
	CLogger(IEventReceiver *r);

	//! Returns the current set log level.
	ELOG_LEVEL getLogLevel() const override;

	//! Sets a new log level.	void setLogLevel(ELOG_LEVEL ll) override;
	void setLogLevel(ELOG_LEVEL ll) override;

	//! Prints out a text into the log
	void log(const c8 *text, ELOG_LEVEL ll = ELL_INFORMATION) override;

	//! Prints out a text into the log
	void log(const c8 *text, const c8 *hint, ELOG_LEVEL ll = ELL_INFORMATION) override;

	//! Sets a new event receiver
	void setReceiver(IEventReceiver *r);

private:
	ELOG_LEVEL LogLevel;
	IEventReceiver *Receiver;
};
