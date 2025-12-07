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

// "SDL_version.h" for SDL_VERSION_ATLEAST
#ifdef _IRR_USE_SDL3_
	#include <SDL3/SDL_clipboard.h>
	#include <SDL3/SDL_version.h>
#else
	#include <SDL_clipboard.h>
	#include <SDL_version.h>
#endif

#include "fast_atof.h"

// constructor
COSOperator::COSOperator()
{}

COSOperator::~COSOperator()
{
#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
	SDL_free(ClipboardSelectionText);
	SDL_free(PrimarySelectionText);
#endif
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
