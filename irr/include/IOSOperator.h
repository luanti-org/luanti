// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "irrString.h"

//! The OSOperator provides OS-specific methods and information.
class IOSOperator : public virtual IReferenceCounted
{
public:
	//! Get the current OS version as string.
	virtual const core::stringc &getOperatingSystemVersion() const = 0;

	//! Copies text to the clipboard
	//! \param text: text in utf-8
	virtual void copyToClipboard(const c8 *text) const = 0;

	//! Copies text to the primary selection
	//! This is a no-op on some platforms.
	//! \param text: text in utf-8
	virtual void copyToPrimarySelection(const c8 *text) const = 0;

	//! Get text from the clipboard
	//! \return Returns 0 if no string is in there, otherwise an utf-8 string.
	virtual const c8 *getTextFromClipboard() const = 0;

	//! Get text from the primary selection
	//! This is a no-op on some platforms.
	//! \return Returns 0 if no string is in there, otherwise an utf-8 string.
	virtual const c8 *getTextFromPrimarySelection() const = 0;

	//! Get the total and available system RAM
	/** \param totalBytes: will contain the total system memory in Kilobytes (1024 B)
	\param availableBytes: will contain the available memory in Kilobytes (1024 B)
	\return True if successful, false if not */
	virtual bool getSystemMemory(u32 *totalBytes, u32 *availableBytes) const = 0;
};
