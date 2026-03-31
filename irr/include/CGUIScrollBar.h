// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIScrollBar.h"
#include "SColor.h"
#include <optional>

namespace gui
{

class IGUIButton;

class CGUIScrollBar : public IGUIScrollBar
{
public:
	//! constructor
	CGUIScrollBar(IGUIEnvironment *environment,
			IGUIElement *parent, s32 id, core::rect<s32> rectangle,
			bool horizontal, bool noclip = false);

	//! destructor
	virtual ~CGUIScrollBar();

	//! called if an event happened.
	bool OnEvent(const SEvent &event) override;

	//! draws the element and its children
	void draw() override;

	void OnPostRender(u32 timeMs) override;

	//! gets the maximum value of the scrollbar.
	s32 getMax() const override;

	//! sets the maximum value of the scrollbar.
	void setMax(s32 max) override;

	//! gets the minimum value of the scrollbar.
	s32 getMin() const override;

	//! sets the minimum value of the scrollbar.
	void setMin(s32 min) override;

	//! gets the small step value
	s32 getSmallStep() const override;

	//! sets the small step value
	void setSmallStep(s32 step) override;

	//! gets the large step value
	s32 getLargeStep() const override;

	//! sets the large step value
	void setLargeStep(s32 step) override;

	//! gets the current position of the scrollbar
	s32 getPos() const override;

	//! Smooth scroll: target position
	s32 getTargetPos() const override;

	void setPosInterpolated(s32 pos) override;

	//! sets the position of the scrollbar
	void setPos(s32 pos) override;

	//! sets the content height to scroll
	void setPageSize(s32 size) override;

	//! updates the rectangle
	void updateAbsolutePosition() override;

	bool isHorizontal() const { return Horizontal; }

	enum ArrowVisibility
	{
		HIDE,
		SHOW,
		DEFAULT
	};
	void setArrowsVisible(ArrowVisibility visible);

protected:
	void refreshControls();
	s32 getPosFromMousePos(const core::position2di &p) const;

	//! The same as setPos, but it takes care of sending EGET_SCROLL_BAR_CHANGED events.
	void setPosAndSend(s32 pos);

	IGUIButton *UpButton;
	IGUIButton *DownButton;
	ArrowVisibility UpDownVisible = DEFAULT;

	core::rect<s32> SliderRect;

	bool Dragging; //< mouse down in tray area (anywhere)
	bool Horizontal;
	bool DraggedBySlider;
	bool SmoothScroll = true; // TODO: move to IGUISkin
	bool JumpToTrayPos = true; // TODO: move to IGUISkin
	s32 Pos;
	s32 DrawPos; //< center position of the thumb
	s32 DrawHeight; //< slider size (V: height, H: width)
	s32 BorderSize = 0; //< Up/Down size (V: height, H: width)
	s32 DragOffset = 0; //< where the slider is grabbed
	s32 Min;
	s32 Max;
	s32 SmallStep;
	s32 LargeStep;
	/// Content size, for automatic slider scaling
	/// -1 disables this feature
	s32 PageSize = -1;
	s32 DesiredPos; //< destination pos for tray clicks
	u32 LastChange; //< since last click outside of slider area
	video::SColor CurrentIconColor;

	f32 range() const { return (f32)(Max - Min); }

	void setPosRaw(const s32 pos);
	void updatePos();

	// Smooth scroll
	std::optional<s32> TargetPos;
	u32 LastTimeMs = 0;
	void interpolatePos(u32 deltaMs);

	// Autoscroll
	u32 AutoScrollMs = 0;
	void handleAutoScroll(u32 deltaMs);
};

} // end namespace gui
