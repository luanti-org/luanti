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
#include <IGUIEnvironment.h>
#include <IGUIScrollBar.h>
#include "StyleSpec.h"
#include "guiButton.h"
#include "irr_ptr.h"

class ISimpleTextureSource;


using namespace gui;

class GUIScrollBar final : public IGUIScrollBar
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

	struct ScrollBarImage {
		irr_ptr<video::ITexture> Texture;
		core::rect<s32> SourceRect;
		core::rect<s32> MiddleRect;
	};

	virtual void draw() override;
	virtual void updateAbsolutePosition() override;
	virtual bool OnEvent(const SEvent &event) override;
	virtual void OnPostRender(u32 time_ms) override;

	s32 getMax() const override { return max_pos; }
	s32 getMin() const override { return min_pos; }
	s32 getLargeStep() const override { return large_step; }
	s32 getSmallStep() const override { return small_step; }
	s32 getPos() const override;
	s32 getTargetPos() const override;
	bool isHorizontal() const { return is_horizontal; }

	void setMax(s32 max) override;
	void setMin(s32 min) override;
	void setSmallStep(s32 step) override;
	void setLargeStep(s32 step) override;
	//! Sets a position immediately, aborting any ongoing interpolation.
	// setPos does not send EGET_SCROLL_BAR_CHANGED events for you.
	void setPos(const s32 pos) override;
	//! The same as setPos, but it takes care of sending EGET_SCROLL_BAR_CHANGED events.
	void setPosAndSend(s32 pos);
	//! Sets a target position for interpolation.
	// If you want to do an interpolated addition, use
	// setPosInterpolated(getTargetPos() + x).
	// setPosInterpolated takes care of sending EGET_SCROLL_BAR_CHANGED events.
	void setPosInterpolated(s32 pos) override;
	void setPageSize(s32 size) override;
	void setArrowsVisible(ArrowVisibility visible);

	void setColor(video::SColor color);
	void setTrackImage(video::ITexture* image);
	void setThumbImage(video::ITexture* image);
	void setFromStyle(const StyleSpec& style);
	void setStyles(const std::array<StyleSpec, StyleSpec::NUM_STATES>& styles,
			const std::array<StyleSpec, StyleSpec::NUM_STATES>& up_arrow_styles,
			const std::array<StyleSpec, StyleSpec::NUM_STATES>& down_arrow_styles);

private:
	void refreshControls();
	s32 getPosFromMousePos(const core::position2di &p) const;
	f32 range() const { return f32(max_pos - min_pos); }

	GUIButton *up_button;
	GUIButton *down_button;
	ArrowVisibility arrow_visibility = DEFAULT;
	bool is_dragging;
	bool is_horizontal;
	bool is_auto_scaling;
	bool dragged_by_slider;
	bool tray_clicked;

	u32 m_arrow_counter = 0; // reset counter when no arrow is held

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

	std::array<StyleSpec, StyleSpec::NUM_STATES> Styles;
	std::array<StyleSpec, StyleSpec::NUM_STATES> UpArrowStyles;
	std::array<StyleSpec, StyleSpec::NUM_STATES> DownArrowStyles;

	core::rect<s32> track_rect;
	core::rect<s32> slider_rect;
	video::SColor current_icon_color;

	core::rect<s32> TrackPadding;

	ScrollBarImage TrackTexture;
	ScrollBarImage ThumbTexture;

	video::SColor BgColor = video::SColor(0xFF,0xFF,0xFF,0xFF);

	video::SColor TrackColor;
	video::SColor ThumbColors[4];

	bool DrawBorder = true;
	bool StaticThumb = false;

	ISimpleTextureSource *m_tsrc;

	void setPosRaw(const s32 pos);
	void updatePos();
	std::optional<s32> target_pos;
	u32 last_time_ms = 0;
	u32 last_delta_ms = 17; // assume 60 FPS
	void interpolatePos();
};
