// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "COSOperator.h"

#ifdef _IRR_WINDOWS_API_
#include <windows.h>
#else
#include <cstring>
#include <unistd.h>
#ifndef _IRR_ANDROID_PLATFORM_
#include <sys/types.h>
#ifdef _IRR_OSX_PLATFORM_
#include <sys/sysctl.h>
#endif
#endif
#endif

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
#include <SDL_clipboard.h>
#include <SDL_version.h>
#endif

#include "fast_atof.h"

// constructor
COSOperator::COSOperator(const core::stringc &osVersion) :
		OperatingSystem(osVersion)
{}

COSOperator::~COSOperator()
{
#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
	SDL_free(ClipboardSelectionText);
	SDL_free(PrimarySelectionText);
#endif
}

//! returns the current operating system version as string.
const core::stringc &COSOperator::getOperatingSystemVersion() const
{
	return OperatingSystem;
}

//! copies text to the clipboard
void COSOperator::copyToClipboard(const c8 *text) const
{
	if (strlen(text) == 0)
		return;

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	SDL_SetClipboardText(text);
#endif
}

//! copies text to the primary selection
void COSOperator::copyToPrimarySelection(const c8 *text) const
{
	if (strlen(text) == 0)
		return;

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
#if SDL_VERSION_ATLEAST(2, 25, 0)
	SDL_SetPrimarySelectionText(text);
#endif
#endif
}

//! gets text from the clipboard
const c8 *COSOperator::getTextFromClipboard() const
{
#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	SDL_free(ClipboardSelectionText);
	ClipboardSelectionText = SDL_GetClipboardText();
	return ClipboardSelectionText;
#else

	return 0;
#endif
}

//! gets text from the primary selection
const c8 *COSOperator::getTextFromPrimarySelection() const
{
#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
#if SDL_VERSION_ATLEAST(2, 25, 0)
	SDL_free(PrimarySelectionText);
	PrimarySelectionText = SDL_GetPrimarySelectionText();
	return PrimarySelectionText;
#endif
	return 0;

#else

	return 0;
#endif
}

bool COSOperator::getSystemMemory(u32 *Total, u32 *Avail) const
{
#if defined(_IRR_WINDOWS_API_)

	MEMORYSTATUSEX MemoryStatusEx;
	MemoryStatusEx.dwLength = sizeof(MEMORYSTATUSEX);

	// cannot fail
	GlobalMemoryStatusEx(&MemoryStatusEx);

	if (Total)
		*Total = (u32)(MemoryStatusEx.ullTotalPhys >> 10);
	if (Avail)
		*Avail = (u32)(MemoryStatusEx.ullAvailPhys >> 10);
	return true;

#elif defined(_IRR_POSIX_API_) && defined(_SC_PHYS_PAGES) && defined(_SC_AVPHYS_PAGES)
	long ps = sysconf(_SC_PAGESIZE);
	long pp = sysconf(_SC_PHYS_PAGES);
	long ap = sysconf(_SC_AVPHYS_PAGES);

	if (ps == -1 || (Total && pp == -1) || (Avail && ap == -1))
		return false;

	if (Total)
		*Total = (u32)((pp >> 10) * ps);
	if (Avail)
		*Avail = (u32)((ap >> 10) * ps);
	return true;
#elif defined(_IRR_OSX_PLATFORM_)
	int mib[2];
	int64_t physical_memory;
	size_t length;

	// Get the Physical memory size
	mib[0] = CTL_HW;
	mib[1] = HW_MEMSIZE;
	length = sizeof(int64_t);
	sysctl(mib, 2, &physical_memory, &length, NULL, 0);

	if (Total)
		*Total = (u32)(physical_memory >> 10);
	if (Avail)
		*Avail = (u32)(physical_memory >> 10); // we don't know better
	return true;
#else
	return false;
#endif
}
