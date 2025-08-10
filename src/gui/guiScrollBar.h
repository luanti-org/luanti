/*
Copyright (C) 2002-2013 Nikolaus Gebhardt
This file is part of the "Irrlicht Engine".
For conditions of distribution and use, see copyright notice in irrlicht.h

Modified 2019.05.01 by stujones11, Stuart Jones <stujones111@gmail.com>

This is a heavily modified copy of the Irrlicht CGUIScrollBar class
which includes automatic scaling of the thumb slider and hiding of
the arrow buttons where there is insufficient space.
*/

#pragma once

#include <optional>
#include <IGUIElement.h>
#include <IGUIEnvironment.h>

class ISimpleTextureSource;


using namespace gui;

class GUIScrollBar : public IGUIElement
{
public:
	GUIScrollBar(IGUIEnvironment *environment, IGUIElement *parent, s32 id,
			core::rect<s32> rectangle, bool horizontal, bool auto_scale,
			ISimpleTextureSource *tsrc);

	enum ArrowVisibility
	{
		HIDE,
		SHOW,
		DEFAULT
	};

	virtual void draw() override;
	virtual void updateAbsolutePosition() override;
	virtual bool OnEvent(const SEvent &event) override;
	virtual void OnPostRender(u32 time_ms) override;

	s32 getMax() const { return max_pos; }
	s32 getMin() const { return min_pos; }
	s32 getLargeStep() const { return large_step; }
	s32 getSmallStep() const { return small_step; }
	s32 getPos() const;
	s32 getTargetPos() const;
	bool isHorizontal() const { return is_horizontal; }

	void setMax(const s32 &max);
	void setMin(const s32 &min);
	void setSmallStep(const s32 &step);
	void setLargeStep(const s32 &step);
	//! Sets a position immediately, aborting any ongoing interpolation.
	// setPos does not send EGET_SCROLL_BAR_CHANGED events for you.
	void setPos(const s32 pos);
	//! The same as setPos, but it takes care of sending EGET_SCROLL_BAR_CHANGED events.
	void setPosAndSend(const s32 &pos);
	//! Sets a target position for interpolation.
	// If you want to do an interpolated addition, use
	// setPosInterpolated(getTargetPos() + x).
	// setPosInterpolated takes care of sending EGET_SCROLL_BAR_CHANGED events.
	void setPosInterpolated(const s32 &pos);
	void setPageSize(const s32 &size);
	void setArrowsVisible(ArrowVisibility visible);

private:
	void refreshControls();
	s32 getPosFromMousePos(const core::position2di &p) const;
	f32 range() const { return f32(max_pos - min_pos); }

	IGUIButton *up_button;
	IGUIButton *down_button;
	ArrowVisibility arrow_visibility = DEFAULT;
	bool is_dragging;
	bool is_horizontal;
	bool is_auto_scaling;
	bool dragged_by_slider;
	bool tray_clicked;
	s32 scroll_pos;
	s32 draw_center;
	s32 thumb_size;
	s32 min_pos;
	s32 max_pos;
	s32 small_step;
	s32 large_step;
	s32 drag_offset;
	s32 page_size;
	s32 border_size;

	core::rect<s32> slider_rect;
	video::SColor current_icon_color;

	ISimpleTextureSource *m_tsrc;

	void setPosRaw(const s32 pos);
	void updatePos();
	std::optional<s32> target_pos;
	u32 last_time_ms = 0;
	u32 last_delta_ms = 17; // assume 60 FPS
	void interpolatePos();
};
