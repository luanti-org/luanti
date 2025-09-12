// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIScrollBar.h"
#include "IGUIButton.h"

namespace gui
{

class CGUIScrollBar : public IGUIScrollBar
{
public:
	//! constructor
	CGUIScrollBar(bool horizontal, IGUIEnvironment *environment,
			IGUIElement *parent, s32 id, core::rect<s32> rectangle,
			bool noclip = false);

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

	//! sets the position of the scrollbar
	void setPos(s32 pos) override;

	//! sets the content height to scroll
	void setPageSize(s32 size) override { }

	//! updates the rectangle
	void updateAbsolutePosition() override;

private:
	void refreshControls();
	s32 getPosFromMousePos(const core::position2di &p) const;

	IGUIButton *UpButton;
	IGUIButton *DownButton;

	core::rect<s32> SliderRect;

	bool Dragging;
	bool Horizontal;
	bool DraggedBySlider;
	bool TrayClick;
	s32 Pos;
	s32 DrawPos;
	s32 DrawHeight;
	s32 Min;
	s32 Max;
	s32 SmallStep;
	s32 LargeStep;
	s32 DesiredPos;
	u32 LastChange;
	video::SColor CurrentIconColor;

	f32 range() const { return (f32)(Max - Min); }
};

} // end namespace gui
