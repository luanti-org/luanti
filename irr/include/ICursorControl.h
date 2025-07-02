// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "position2d.h"
#include "rect.h"

namespace gui
{

class IGUISpriteBank;

//! Default icons for cursors
enum ECURSOR_ICON
{
	// Following cursors might be system specific, or might use an Irrlicht icon-set. No guarantees so far.
	ECI_NORMAL,   // arrow
	ECI_CROSS,    // Crosshair
	ECI_HAND,     // Hand
	ECI_HELP,     // Arrow and question mark
	ECI_IBEAM,    // typical text-selection cursor
	ECI_NO,       // should not click icon
	ECI_WAIT,     // hourglass
	ECI_SIZEALL,  // arrow in all directions
	ECI_SIZENESW, // resizes in direction north-east or south-west
	ECI_SIZENWSE, // resizes in direction north-west or south-east
	ECI_SIZENS,   // resizes in direction north or south
	ECI_SIZEWE,   // resizes in direction west or east
	ECI_UP,       // up-arrow

	// Implementer note: Should we add system specific cursors, which use guaranteed the system icons,
	// then I would recommend using a naming scheme like ECI_W32_CROSS, ECI_X11_CROSSHAIR and adding those
	// additionally.

	ECI_COUNT // maximal of defined cursors. Note that higher values can be created at runtime
};

//! Names for ECURSOR_ICON
const c8 *const GUICursorIconNames[ECI_COUNT + 1] = {
		"normal",
		"cross",
		"hand",
		"help",
		"ibeam",
		"no",
		"wait",
		"sizeall",
		"sizenesw",
		"sizenwse",
		"sizens",
		"sizewe",
		"sizeup",
		0,
	};

//! structure used to set sprites as cursors.
struct SCursorSprite
{
	SCursorSprite() :
			SpriteBank(0), SpriteId(-1)
	{
	}

	SCursorSprite(gui::IGUISpriteBank *spriteBank, s32 spriteId, const core::position2d<s32> &hotspot = (core::position2d<s32>(0, 0))) :
			SpriteBank(spriteBank), SpriteId(spriteId), HotSpot(hotspot)
	{
	}

	IGUISpriteBank *SpriteBank;
	s32 SpriteId;
	core::position2d<s32> HotSpot;
};

//! platform specific behavior flags for the cursor
enum ECURSOR_PLATFORM_BEHAVIOR
{
	//! default - no platform specific behavior
	ECPB_NONE = 0,

	//! On X11 try caching cursor updates as XQueryPointer calls can be expensive.
	/** Update cursor positions only when the irrlicht timer has been updated or the timer is stopped.
		This means you usually get one cursor update per device->run() which will be fine in most cases.
		See this forum-thread for a more detailed explanation:
		http://irrlicht.sourceforge.net/forum/viewtopic.php?f=7&t=45525
	*/
	ECPB_X11_CACHE_UPDATES = 1
};

//! Interface to manipulate the mouse cursor.
class ICursorControl : public virtual IReferenceCounted
{
public:
	//! Changes the visible state of the mouse cursor.
	/** \param visible: The new visible state. If true, the cursor will be visible,
	if false, it will be invisible. */
	virtual void setVisible(bool visible) = 0;

	//! Returns if the cursor is currently visible.
	/** \return True if the cursor flag is set to visible, false if not. */
	virtual bool isVisible() const = 0;

	//! Sets the new position of the cursor.
	/** The position must be
	between (0.0f, 0.0f) and (1.0f, 1.0f), where (0.0f, 0.0f) is
	the top left corner and (1.0f, 1.0f) is the bottom right corner of the
	render window.
	\param pos New position of the cursor. */
	virtual void setPosition(const core::position2d<f32> &pos) = 0;

	//! Sets the new position of the cursor.
	/** The position must be
	between (0.0f, 0.0f) and (1.0f, 1.0f), where (0.0f, 0.0f) is
	the top left corner and (1.0f, 1.0f) is the bottom right corner of the
	render window.
	\param x New x-coord of the cursor.
	\param y New x-coord of the cursor. */
	virtual void setPosition(f32 x, f32 y) = 0;

	//! Sets the new position of the cursor.
	/** \param pos: New position of the cursor. The coordinates are pixel units. */
	virtual void setPosition(const core::position2d<s32> &pos) = 0;

	//! Sets the new position of the cursor.
	/** \param x New x-coord of the cursor. The coordinates are pixel units.
	\param y New y-coord of the cursor. The coordinates are pixel units. */
	virtual void setPosition(s32 x, s32 y) = 0;

	//! Returns the current position of the mouse cursor.
	/** \param updateCursor When true ask system/OS for current cursor position.
		When false return the last known (buffered) position ( this is useful to
		check what has become of a setPosition call with float numbers).
	\return Returns the current position of the cursor. The returned position
	is the position of the mouse cursor in pixel units. */
	virtual const core::position2d<s32> &getPosition(bool updateCursor = true) = 0;

	//! Returns the current position of the mouse cursor.
	/** \param updateCursor When true ask system/OS for current cursor position.
		When false return the last known (buffered) position (this is
		useful to check what has become of a setPosition call with float numbers
		and is often different from the values you passed in setPosition).
	\return Returns the current position of the cursor. The returned position
	is a value between (0.0f, 0.0f) and (1.0f, 1.0f), where (0.0f, 0.0f) is
	the top left corner and (1.0f, 1.0f) is the bottom right corner of the
	render window. */
	virtual core::position2d<f32> getRelativePosition(bool updateCursor = true) = 0;

	//! Sets an absolute reference rect for setting and retrieving the cursor position.
	/** If this rect is set, the cursor position is not being calculated relative to
	the rendering window but to this rect. You can set the rect pointer to 0 to disable
	this feature again. This feature is useful when rendering into parts of foreign windows
	for example in an editor.
	\param rect: A pointer to an reference rectangle or 0 to disable the reference rectangle.*/
	virtual void setReferenceRect(core::rect<s32> *rect = 0) = 0;

	//! Internally fixes the mouse position, and reports relative mouse movement compared to the old position
	/** Specific to SDL */
	virtual void setRelativeMode(bool relative){};

	//! Sets the active cursor icon
	/** Setting cursor icons is so far only supported on Win32 and Linux */
	virtual void setActiveIcon(ECURSOR_ICON iconId) {}

	//! Gets the currently active icon
	virtual ECURSOR_ICON getActiveIcon() const { return gui::ECI_NORMAL; }

	//! Add a custom sprite as cursor icon.
	/** \return Identification for the icon */
	virtual ECURSOR_ICON addIcon(const gui::SCursorSprite &icon) { return gui::ECI_NORMAL; }

	//! replace a cursor icon.
	/** Changing cursor icons is so far only supported on Win32 and Linux
		Note that this only changes the icons within your application, system cursors outside your
		application will not be affected.
	*/
	virtual void changeIcon(ECURSOR_ICON iconId, const gui::SCursorSprite &sprite) {}

	//! Return a system-specific size which is supported for cursors. Larger icons will fail, smaller icons might work.
	virtual core::dimension2di getSupportedIconSize() const { return core::dimension2di(0, 0); }

	//! Set platform specific behavior flags.
	virtual void setPlatformBehavior(ECURSOR_PLATFORM_BEHAVIOR behavior) {}

	//! Return platform specific behavior.
	/** \return Behavior set by setPlatformBehavior or ECPB_NONE for platforms not implementing specific behaviors.
	 */
	virtual ECURSOR_PLATFORM_BEHAVIOR getPlatformBehavior() const { return ECPB_NONE; }
};

} // end namespace gui
