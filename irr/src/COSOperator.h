// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IOSOperator.h"

class CIrrDeviceLinux;

//! The OSOperator provides OS-specific methods and information.
class COSOperator : public IOSOperator
{
public:
	// constructor
#if defined(_IRR_COMPILE_WITH_X11_DEVICE_)
	COSOperator(const core::stringc &osversion, CIrrDeviceLinux *device);
#endif
	COSOperator(const core::stringc &osversion);

	~COSOperator();

	COSOperator(const COSOperator &) = delete;
	COSOperator &operator=(const COSOperator &) = delete;

	//! Get the current OS version as string.
	const core::stringc &getOperatingSystemVersion() const override;

	//! Copies text to the clipboard
	//! \param text: text in utf-8
	void copyToClipboard(const c8 *text) const override;

	//! Copies text to the primary selection
	//! This is a no-op on some platforms.
	//! \param text: text in utf-8
	void copyToPrimarySelection(const c8 *text) const override;

	//! Get text from the clipboard
	//! \return Returns 0 if no string is in there, otherwise an utf-8 string.
	const c8 *getTextFromClipboard() const override;

	//! Get text from the primary selection
	//! This is a no-op on some platforms.
	//! \return Returns 0 if no string is in there, otherwise an utf-8 string.
	const c8 *getTextFromPrimarySelection() const override;

	//! Get the total and available system RAM
	/** \param totalBytes: will contain the total system memory in Kilobytes (1024 B)
	\param availableBytes: will contain the available memory in Kilobytes (1024 B)
	\return True if successful, false if not */
	bool getSystemMemory(u32 *Total, u32 *Avail) const override;

private:
	core::stringc OperatingSystem;

#if defined(_IRR_COMPILE_WITH_X11_DEVICE_)
	CIrrDeviceLinux *IrrDeviceLinux;
#endif

#ifdef _IRR_WINDOWS_API_
	mutable core::stringc ClipboardBuf;
#endif

#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
	// These need to be freed with SDL_free
	mutable char *ClipboardSelectionText = nullptr;
	mutable char *PrimarySelectionText = nullptr;
#endif
};
