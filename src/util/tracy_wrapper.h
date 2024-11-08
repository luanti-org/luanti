// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 DS

/*
 * Wrapper for <tracy/Tracy.hpp>, so that we can use Tracy's macros without
 * having it as mandatory dependency.
 *
 * For annotations that you don't intend to upstream, you can also include
 * <tracy/Tracy.hpp> directly (which also works in irr/).
 */

#pragma once

#include "config.h"
#include "util/basic_macros.h"

#if BUILD_WITH_TRACY

#include <tracy/Tracy.hpp> // IWYU pragma: export

#else

// Copied from Tracy.hpp

#define TracyNoop

#define ZoneNamed(x,y)
#define ZoneNamedN(x,y,z)
#define ZoneNamedC(x,y,z)
#define ZoneNamedNC(x,y,z,w)

#define ZoneTransient(x,y)
#define ZoneTransientN(x,y,z)

#define ZoneScoped
#define ZoneScopedN(x)
#define ZoneScopedC(x)
#define ZoneScopedNC(x,y)

#define ZoneText(x,y)
#define ZoneTextV(x,y,z)
#define ZoneTextF(x,...)
#define ZoneTextVF(x,y,...)
#define ZoneName(x,y)
#define ZoneNameV(x,y,z)
#define ZoneNameF(x,...)
#define ZoneNameVF(x,y,...)
#define ZoneColor(x)
#define ZoneColorV(x,y)
#define ZoneValue(x)
#define ZoneValueV(x,y)
#define ZoneIsActive false
#define ZoneIsActiveV(x) false

#define FrameMark
#define FrameMarkNamed(x)
#define FrameMarkStart(x)
#define FrameMarkEnd(x)

#define FrameImage(x,y,z,w,a)

#define TracyLockable( type, varname ) type varname
#define TracyLockableN( type, varname, desc ) type varname
#define TracySharedLockable( type, varname ) type varname
#define TracySharedLockableN( type, varname, desc ) type varname
#define LockableBase( type ) type
#define SharedLockableBase( type ) type
#define LockMark(x) (void)x
#define LockableName(x,y,z)

#define TracyPlot(x,y)
#define TracyPlotConfig(x,y,z,w,a)

#define TracyMessage(x,y)
#define TracyMessageL(x)
#define TracyMessageC(x,y,z)
#define TracyMessageLC(x,y)
#define TracyAppInfo(x,y)

#define TracyAlloc(x,y)
#define TracyFree(x)
#define TracySecureAlloc(x,y)
#define TracySecureFree(x)

#define TracyAllocN(x,y,z)
#define TracyFreeN(x,y)
#define TracySecureAllocN(x,y,z)
#define TracySecureFreeN(x,y)

#define ZoneNamedS(x,y,z)
#define ZoneNamedNS(x,y,z,w)
#define ZoneNamedCS(x,y,z,w)
#define ZoneNamedNCS(x,y,z,w,a)

#define ZoneTransientS(x,y,z)
#define ZoneTransientNS(x,y,z,w)

#define ZoneScopedS(x)
#define ZoneScopedNS(x,y)
#define ZoneScopedCS(x,y)
#define ZoneScopedNCS(x,y,z)

#define TracyAllocS(x,y,z)
#define TracyFreeS(x,y)
#define TracySecureAllocS(x,y,z)
#define TracySecureFreeS(x,y)

#define TracyAllocNS(x,y,z,w)
#define TracyFreeNS(x,y,z)
#define TracySecureAllocNS(x,y,z,w)
#define TracySecureFreeNS(x,y,z)

#define TracyMessageS(x,y,z)
#define TracyMessageLS(x,y)
#define TracyMessageCS(x,y,z,w)
#define TracyMessageLCS(x,y,z)

#define TracySourceCallbackRegister(x,y)
#define TracyParameterRegister(x,y)
#define TracyParameterSetup(x,y,z,w)
#define TracyIsConnected false
#define TracyIsStarted false
#define TracySetProgramName(x)

#define TracyFiberEnter(x)
#define TracyFiberEnterHint(x,y)
#define TracyFiberLeave

#endif


// Helper for making sure frames end in all possible control flow path
class FrameMarker
{
	const char *m_name;
	bool m_started = false;

public:
	FrameMarker(const char *name) : m_name(name) {}

	~FrameMarker() { end(); }

	DISABLE_CLASS_COPY(FrameMarker)

	FrameMarker(FrameMarker &&other) noexcept :
		m_name(other.m_name), m_started(other.m_started)
	{
		other.m_started = false;
	}

	FrameMarker &operator=(FrameMarker &&other) noexcept
	{
		if (&other != this) {
			end();
			m_name = other.m_name;
			m_started = other.m_started;
			other.m_started = false;
		}
		return *this;
	}

	FrameMarker &&started() &&
	{
		if (!m_started) {
			FrameMarkStart(m_name);
			m_started = true;
		}
		return std::move(*this);
	}

	void start()
	{
		// no move happens, because we drop the reference
		(void)std::move(*this).started();
	}

	void end()
	{
		if (m_started) {
			m_started = false;
			FrameMarkEnd(m_name);
		}
	}
};
