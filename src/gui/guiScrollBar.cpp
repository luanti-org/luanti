/*
Copyright (C) 2002-2013 Nikolaus Gebhardt
This file is part of the "Irrlicht Engine".
For conditions of distribution and use, see copyright notice in irrlicht.h

Modified 2019.05.01 by stujones11, Stuart Jones <stujones111@gmail.com>

This is a heavily modified copy of the Irrlicht CGUIScrollBar class
which includes automatic scaling of the thumb slider and hiding of
the arrow buttons where there is insufficient space.
*/

#include "irrMath.h"
#include "client/guiscalingfilter.h"
#include "guiScrollBar.h"
#include "guiButton.h"
#include "guiButtonImage.h"
#include "porting.h"
#include "settings.h"
#include <IGUISkin.h>
#include "IVideoDriver.h"

GUIScrollBar::GUIScrollBar(IGUIEnvironment *environment, IGUIElement *parent, s32 id,
		core::rect<s32> rectangle, bool horizontal, bool auto_scale,
		ISimpleTextureSource *tsrc) :
		IGUIScrollBar(environment, parent, id, rectangle),
		up_button(nullptr), down_button(nullptr), is_dragging(false),
		is_horizontal(horizontal), is_auto_scaling(auto_scale),
		dragged_by_slider(false), tray_clicked(false), scroll_pos(0),
		draw_center(0), thumb_size(0), min_pos(0), max_pos(100), small_step(10),
		large_step(50), drag_offset(0), page_size(100), border_size(0),
		m_tsrc(tsrc)
{
	refreshControls();
	setNotClipped(false);
	setTabStop(true);
	setTabOrder(-1);
	setPos(0);

	auto skin = Environment->getSkin();
	TrackColor = skin->getColor(EGDC_SCROLLBAR);

	for (size_t i = 0; i < 4; i++) {
		video::SColor base =
			skin->getColor((gui::EGUI_DEFAULT_COLOR)i);
		ThumbColors[i] = base;
	}
}

bool GUIScrollBar::OnEvent(const SEvent &event)
{
	if (isEnabled()) {
		switch (event.EventType) {
		case EET_KEY_INPUT_EVENT:
			if (event.KeyInput.PressedDown) {
				const s32 old_pos = getTargetPos();
				bool absorb = true;
				switch (event.KeyInput.Key) {
				case KEY_LEFT:
				case KEY_UP:
					setPosInterpolated(old_pos - small_step);
					break;
				case KEY_RIGHT:
				case KEY_DOWN:
					setPosInterpolated(old_pos + small_step);
					break;
				case KEY_HOME:
					setPosInterpolated(min_pos);
					break;
				case KEY_PRIOR:
					setPosInterpolated(old_pos - large_step);
					break;
				case KEY_END:
					setPosInterpolated(max_pos);
					break;
				case KEY_NEXT:
					setPosInterpolated(old_pos + large_step);
					break;
				default:
					absorb = false;
				}
				if (absorb)
					return true;
			}
			break;
		case EET_MOUSE_INPUT_EVENT: {
			const core::position2di p(event.MouseInput.X, event.MouseInput.Y);
			bool is_inside = isPointInside(p);
			switch (event.MouseInput.Event) {
			case EMIE_MOUSE_WHEEL:
				if (Environment->hasFocus(this)) {
					s8 d = event.MouseInput.Wheel < 0 ? -1 : 1;
					s8 h = is_horizontal ? 1 : -1;
					setPosInterpolated(getTargetPos() + (d * small_step * h));
					return true;
				}
				break;
			case EMIE_LMOUSE_PRESSED_DOWN: {
				if (is_inside) {
					is_dragging = true;
					dragged_by_slider = slider_rect.isPointInside(p);
					core::vector2di corner = slider_rect.UpperLeftCorner;
					drag_offset = is_horizontal ? p.X - corner.X : p.Y - corner.Y;
					tray_clicked = !dragged_by_slider;
					if (tray_clicked) {
						const s32 new_pos = getPosFromMousePos(p);
						setPosAndSend(new_pos);
						// drag in the middle
						drag_offset = thumb_size / 2;
					}
					Environment->setFocus(this);
					return true;
				}
				break;
			}
			case EMIE_LMOUSE_LEFT_UP:
			case EMIE_MOUSE_MOVED: {
				if (!event.MouseInput.isLeftPressed())
					is_dragging = false;

				if (!is_dragging) {
					if (event.MouseInput.Event == EMIE_MOUSE_MOVED)
						break;
					return is_inside;
				}

				if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
					is_dragging = false;

				if (!dragged_by_slider) {
					if (is_inside) {
						dragged_by_slider = slider_rect.isPointInside(p);
						tray_clicked = !dragged_by_slider;
					}
					if (!dragged_by_slider) {
						tray_clicked = false;
						if (event.MouseInput.Event == EMIE_MOUSE_MOVED)
							return is_inside;
					}
				}

				const s32 new_pos = getPosFromMousePos(p);
				setPosAndSend(new_pos);

				return is_inside;
			}
			default:
				break;
			}
		} break;
		default:
			break;
		}
	}
	return IGUIElement::OnEvent(event);
}

void GUIScrollBar::draw()
{
	if (!IsVisible)
		return;

	IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return;

	video::IVideoDriver* driver = Environment->getVideoDriver();

	video::SColor icon_color = skin->getColor(
			isEnabled() ? EGDC_WINDOW_SYMBOL : EGDC_GRAY_WINDOW_SYMBOL);
	if (icon_color != current_icon_color)
		refreshControls();

	track_rect = AbsoluteRect;
	if (is_horizontal) {
		track_rect.UpperLeftCorner.X  += TrackPadding.UpperLeftCorner.X;
		track_rect.UpperLeftCorner.Y  += TrackPadding.UpperLeftCorner.Y;
		track_rect.LowerRightCorner.X += TrackPadding.LowerRightCorner.X;
		track_rect.LowerRightCorner.Y += TrackPadding.LowerRightCorner.Y;
	} else {
		track_rect.UpperLeftCorner.X  += TrackPadding.UpperLeftCorner.Y;
		track_rect.UpperLeftCorner.Y  += TrackPadding.UpperLeftCorner.X;
		track_rect.LowerRightCorner.X += TrackPadding.LowerRightCorner.Y;
		track_rect.LowerRightCorner.Y += TrackPadding.LowerRightCorner.X;
	}

	if (DrawBorder)
		skin->draw2DRectangle(this, TrackColor, track_rect,
				&AbsoluteClippingRect);

	if (TrackTexture.Texture) {
		core::rect<s32> sourceRect = core::rect<s32>(core::position2di(0,0), TrackTexture.Texture->getOriginalSize());
		video::SColor image_colors[] = { BgColor, BgColor, BgColor, BgColor };
		if (TrackTexture.MiddleRect.getArea() == 0) {
			driver->draw2DImage(TrackTexture.Texture.get(),
					track_rect, sourceRect,
					&AbsoluteClippingRect, image_colors, true);
		} else {
			draw2DImage9Slice(driver, TrackTexture.Texture.get(),
					track_rect, sourceRect,
					TrackTexture.MiddleRect, &AbsoluteClippingRect, image_colors);
		}
	}

	slider_rect = AbsoluteRect;
	if (core::isnotzero(range())) {
		if (is_horizontal) {
			slider_rect.UpperLeftCorner.X = AbsoluteRect.UpperLeftCorner.X +
							draw_center - thumb_size / 2;
			slider_rect.LowerRightCorner.X =
					slider_rect.UpperLeftCorner.X + thumb_size;
		} else {
			slider_rect.UpperLeftCorner.Y = AbsoluteRect.UpperLeftCorner.Y +
							draw_center - thumb_size / 2;
			slider_rect.LowerRightCorner.Y =
					slider_rect.UpperLeftCorner.Y + thumb_size;
		}
		if (DrawBorder)
			skin->drawColored3DButtonPaneStandard(this, slider_rect, &AbsoluteClippingRect, ThumbColors);

		if (ThumbTexture.Texture) {
			core::rect<s32> sourceRect = core::rect<s32>(core::position2di(0,0), ThumbTexture.Texture->getOriginalSize());
			video::SColor image_colors[] = { BgColor, BgColor, BgColor, BgColor };
			if (ThumbTexture.MiddleRect.getArea() == 0) {
				driver->draw2DImage(ThumbTexture.Texture.get(),
						slider_rect,
						sourceRect, &AbsoluteClippingRect,
						image_colors, true);
			} else {
				draw2DImage9Slice(driver, ThumbTexture.Texture.get(),
						slider_rect,
						sourceRect, ThumbTexture.MiddleRect, &AbsoluteClippingRect, image_colors);
			}
		}
	}
	IGUIElement::draw();
}

static inline s32 interpolate_scroll(s32 from, s32 to, f32 amount)
{
	s32 step = core::round32((to - from) * core::clamp(amount, 0.001f, 1.0f));
	if (step == 0)
		return to;
	return from + step;
}

void GUIScrollBar::interpolatePos()
{
	if (target_pos.has_value()) {
		// Adjust to match 60 FPS. This also means that interpolation is
		// effectively disabled at <= 30 FPS.
		f32 amount = 0.5f * (last_delta_ms / 16.667f);
		setPosRaw(interpolate_scroll(scroll_pos, *target_pos, amount));
		if (scroll_pos == target_pos)
			target_pos = std::nullopt;

		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = this;
		e.GUIEvent.Element = nullptr;
		e.GUIEvent.EventType = EGET_SCROLL_BAR_CHANGED;
		Parent->OnEvent(e);
	}
}

void GUIScrollBar::OnPostRender(u32 time_ms)
{
	last_delta_ms = porting::getDeltaMs(last_time_ms, time_ms);
	last_time_ms = time_ms;
	interpolatePos();

	const bool up_pressed = up_button && up_button->isPressed();
	const bool down_pressed = down_button && down_button->isPressed();

	// if neither is pressed, reset counter
	if (!up_pressed && !down_pressed) {
		m_arrow_counter = 0; // reset counter when no arrow is held
		return;
	}

	const u32 initial_delay = 300; // ms before repeating starts
	const u32 repeat_delay  = 150; // ms between repeats
	assert(initial_delay > repeat_delay);

	const bool is_initial = (m_arrow_counter == 0);
	// counter is 0, so start counting
	m_arrow_counter += last_delta_ms;

	// wait for initial delay
	if (m_arrow_counter < initial_delay && !is_initial)
		return;

	// after initial delay, repeat every repeat_delay
	const s32 autoscroll_stepsize = small_step * (up_pressed ? -1 : 1);
	setPosInterpolated(getTargetPos() + autoscroll_stepsize);
	if (!is_initial)
		m_arrow_counter -= repeat_delay;
}

void GUIScrollBar::updateAbsolutePosition()
{
	IGUIElement::updateAbsolutePosition();
	refreshControls();
	updatePos();
}

s32 GUIScrollBar::getPosFromMousePos(const core::position2di &pos) const
{
	s32 w, p;
	s32 offset = dragged_by_slider ? drag_offset : thumb_size / 2;

	if (is_horizontal) {
		w = RelativeRect.getWidth() - border_size * 2 - thumb_size;
		p = pos.X - AbsoluteRect.UpperLeftCorner.X - border_size - offset;
	} else {
		w = RelativeRect.getHeight() - border_size * 2 - thumb_size;
		p = pos.Y - AbsoluteRect.UpperLeftCorner.Y - border_size - offset;
	}
	return core::isnotzero(range()) ? s32(f32(p) / f32(w) * range() + 0.5f) + min_pos : 0;
}

void GUIScrollBar::updatePos()
{
	setPosRaw(scroll_pos);
}

void GUIScrollBar::setPosRaw(const s32 pos)
{
	s32 thumb_area = 0;
	s32 thumb_min = 0;

	if (is_horizontal) {
		thumb_min = std::min(RelativeRect.getHeight(), RelativeRect.getWidth() / 2);
		thumb_area = RelativeRect.getWidth() - border_size * 2;
	} else {
		thumb_min = std::min(RelativeRect.getWidth(), RelativeRect.getHeight() / 2);
		thumb_area = RelativeRect.getHeight() - border_size * 2;
	}

	if (StaticThumb)
		thumb_size = 0;
	else if (is_auto_scaling)
		thumb_size = (s32)std::fmin(S32_MAX,
				thumb_area / (f32(page_size) / f32(thumb_area + border_size * 2)));

	thumb_size = core::s32_clamp(thumb_size, thumb_min, thumb_area);
	scroll_pos = core::s32_clamp(pos, min_pos, max_pos);

	f32 f = core::isnotzero(range()) ? (f32(thumb_area) - f32(thumb_size)) / range()
					 : 1.0f;
	draw_center = s32((f32(scroll_pos - min_pos) * f) + (f32(thumb_size) * 0.5f)) +
		border_size;
}

void GUIScrollBar::setPos(const s32 pos)
{
	setPosRaw(pos);
	target_pos = std::nullopt;
}

void GUIScrollBar::setPosAndSend(const s32 pos)
{
	const s32 old_pos = scroll_pos;
	setPos(pos);
	if (scroll_pos != old_pos && Parent) {
		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = this;
		e.GUIEvent.Element = nullptr;
		e.GUIEvent.EventType = EGET_SCROLL_BAR_CHANGED;
		Parent->OnEvent(e);
	}
}

void GUIScrollBar::setPosInterpolated(const s32 pos)
{
	if (!g_settings->getBool("smooth_scrolling")) {
		setPosAndSend(pos);
		return;
	}

	s32 clamped = core::s32_clamp(pos, min_pos, max_pos);
	if (scroll_pos != clamped) {
		target_pos = clamped;
		interpolatePos();
	} else {
		target_pos = std::nullopt;
	}
}

void GUIScrollBar::setSmallStep(const s32 step)
{
	small_step = step > 0 ? step : 10;
}

void GUIScrollBar::setLargeStep(const s32 step)
{
	large_step = step > 0 ? step : 50;
}

void GUIScrollBar::setMax(const s32 max)
{
	max_pos = max;
	if (min_pos > max_pos)
		min_pos = max_pos;

	bool enable = core::isnotzero(range());
	up_button->setEnabled(enable);
	down_button->setEnabled(enable);
	updatePos();
}

void GUIScrollBar::setMin(const s32 min)
{
	min_pos = min;
	if (max_pos < min_pos)
		max_pos = min_pos;

	bool enable = core::isnotzero(range());
	up_button->setEnabled(enable);
	down_button->setEnabled(enable);
	updatePos();
}

void GUIScrollBar::setPageSize(const s32 size)
{
	page_size = size;
	updatePos();
}

void GUIScrollBar::setArrowsVisible(ArrowVisibility visible)
{
	arrow_visibility = visible;
	refreshControls();
}

s32 GUIScrollBar::getPos() const
{
	return scroll_pos;
}

s32 GUIScrollBar::getTargetPos() const
{
	if (target_pos.has_value()) {
		s32 clamped = core::s32_clamp(*target_pos, min_pos, max_pos);
		return clamped;
	}
	return scroll_pos;
}

void GUIScrollBar::refreshControls()
{
	IGUISkin *skin = Environment->getSkin();
	IGUISpriteBank *sprites = nullptr;
	current_icon_color = video::SColor(255, 255, 255, 255);

	if (skin) {
		sprites = skin->getSpriteBank();
		current_icon_color =
				skin->getColor(isEnabled() ? EGDC_WINDOW_SYMBOL
							   : EGDC_GRAY_WINDOW_SYMBOL);
	}
	if (is_horizontal) {
		s32 h = RelativeRect.getHeight();
		border_size = RelativeRect.getWidth() < h * 4 ? 0 : h;
		if (!up_button) {
			core::rect<s32> up_button_rect(0, 0, h, h);
			up_button = GUIButtonImage::addButton(Environment, up_button_rect, m_tsrc,
					this, -1, L"");
			up_button->setSubElement(true);
			up_button->setTabStop(false);
		}
		// Logical up corresponds to visual down in horizontal orientation.
		if (DownArrowStyles[StyleSpec::STATE_DEFAULT].isNotDefault(StyleSpec::FGIMG)) {
			up_button->setSpriteBank(nullptr);
		} else if (sprites) {
			up_button->setSpriteBank(sprites);
			up_button->setSprite(EGBS_BUTTON_UP,
					s32(skin->getIcon(EGDI_CURSOR_LEFT)),
					current_icon_color);
			up_button->setSprite(EGBS_BUTTON_DOWN,
					s32(skin->getIcon(EGDI_CURSOR_LEFT)),
					current_icon_color);
		}
		up_button->setRelativePosition(core::rect<s32>(0, 0, h, h));
		up_button->setAlignment(EGUIA_UPPERLEFT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT,
				EGUIA_LOWERRIGHT);
		if (!down_button) {
			core::rect<s32> down_button_rect(
					RelativeRect.getWidth() - h, 0,
					RelativeRect.getWidth(), h
				);
			down_button = GUIButtonImage::addButton(Environment, down_button_rect, m_tsrc,
					this, -1, L"");
			down_button->setSubElement(true);
			down_button->setTabStop(false);
		}
		// Logical down corresponds to visual up in horizontal orientation.
		if (UpArrowStyles[StyleSpec::STATE_DEFAULT].isNotDefault(StyleSpec::FGIMG)) {
			down_button->setSpriteBank(nullptr);
		} else if (sprites) {
			down_button->setSpriteBank(sprites);
			down_button->setSprite(EGBS_BUTTON_UP,
					s32(skin->getIcon(EGDI_CURSOR_RIGHT)),
					current_icon_color);
			down_button->setSprite(EGBS_BUTTON_DOWN,
					s32(skin->getIcon(EGDI_CURSOR_RIGHT)),
					current_icon_color);
		}
		down_button->setRelativePosition(
				core::rect<s32>(RelativeRect.getWidth() - h, 0,
						RelativeRect.getWidth(), h));
		down_button->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT,
				EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT);
	} else {
		s32 w = RelativeRect.getWidth();
		border_size = RelativeRect.getHeight() < w * 4 ? 0 : w;
		if (!up_button) {
			core::rect<s32> up_button_rect(0, 0, w, w);
			up_button = GUIButtonImage::addButton(Environment, up_button_rect, m_tsrc,
					this, -1, L"");
			up_button->setSubElement(true);
			up_button->setTabStop(false);
		}
		if (UpArrowStyles[StyleSpec::STATE_DEFAULT].isNotDefault(StyleSpec::FGIMG)) {
			up_button->setSpriteBank(nullptr);
		} else if (sprites) {
			up_button->setSpriteBank(sprites);
			up_button->setSprite(EGBS_BUTTON_UP,
					s32(skin->getIcon(EGDI_CURSOR_UP)),
					current_icon_color);
			up_button->setSprite(EGBS_BUTTON_DOWN,
					s32(skin->getIcon(EGDI_CURSOR_UP)),
					current_icon_color);
		}
		up_button->setRelativePosition(core::rect<s32>(0, 0, w, w));
		up_button->setAlignment(EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT,
				EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
		if (!down_button) {
			core::rect<s32> down_button_rect(
					0, RelativeRect.getHeight() - w,
					w, RelativeRect.getHeight()
				);
			down_button = GUIButtonImage::addButton(Environment, down_button_rect, m_tsrc,
					this, -1, L"");
			down_button->setSubElement(true);
			down_button->setTabStop(false);
		}
		if (DownArrowStyles[StyleSpec::STATE_DEFAULT].isNotDefault(StyleSpec::FGIMG)) {
			down_button->setSpriteBank(nullptr);
		} else if (sprites) {
			down_button->setSpriteBank(sprites);
			down_button->setSprite(EGBS_BUTTON_UP,
					s32(skin->getIcon(EGDI_CURSOR_DOWN)),
					current_icon_color);
			down_button->setSprite(EGBS_BUTTON_DOWN,
					s32(skin->getIcon(EGDI_CURSOR_DOWN)),
					current_icon_color);
		}
		down_button->setRelativePosition(
				core::rect<s32>(0, RelativeRect.getHeight() - w, w,
						RelativeRect.getHeight()));
		down_button->setAlignment(EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT,
				EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT);
	}

	bool visible;
	if (arrow_visibility == DEFAULT)
		visible = (border_size != 0);
	else if (arrow_visibility == HIDE) {
		visible = false;
		border_size = 0;
	} else {
		visible = true;
		if (is_horizontal)
			border_size = RelativeRect.getHeight();
		else
			border_size = RelativeRect.getWidth();
	}

	// Note: The styling targets are named in accordance with the buttons' visual layout
	// ('up' is on the top or the right). However, the variables here are named based
	// on the buttons' logical function ('up' decrements, 'down' increments).
	// Since the value of a horizontal scrollbar increases to the right, the 'up'
	// visual position corresponds to the 'down' logical function, and vice versa.
	up_button->setStyles(is_horizontal ? DownArrowStyles : UpArrowStyles);
	down_button->setStyles(is_horizontal ? UpArrowStyles : DownArrowStyles);

	up_button->setVisible(visible);
	down_button->setVisible(visible);
}

void GUIScrollBar::setColor(video::SColor color)
{
	BgColor = color;

	auto skin = Environment->getSkin();
	TrackColor = skin->getColor(EGDC_SCROLLBAR).getInterpolated(BgColor, 0.65f);

	float d = 0.65f;
	for (size_t i = 0; i < 4; i++) {
		video::SColor base = Environment->getSkin()->getColor((gui::EGUI_DEFAULT_COLOR)i);
		ThumbColors[i] = base.getInterpolated(color, d);
	}
}

void GUIScrollBar::setTrackImage(video::ITexture* image)
{
	image->grab();

	if (TrackTexture.Texture)
		TrackTexture.Texture->drop();

	TrackTexture.Texture.reset(image);
}

void GUIScrollBar::setThumbImage(video::ITexture* image)
{
	image->grab();

	if (ThumbTexture.Texture)
		ThumbTexture.Texture->drop();

	ThumbTexture.Texture.reset(image);
}

void GUIScrollBar::setFromStyle(const StyleSpec& style)
{
	DrawBorder = style.getBool(StyleSpec::BORDER, true);

	if (style.isNotDefault(StyleSpec::BGCOLOR)) {
		setColor(style.getColor(StyleSpec::BGCOLOR, video::SColor(255,255,255,255)));
	}

	TrackTexture.MiddleRect = style.getRect(StyleSpec::BGIMG_MIDDLE, core::rect<s32>());

	if (style.isNotDefault(StyleSpec::BGIMG)) {
		video::ITexture *texture = style.getTexture(StyleSpec::BGIMG,
				m_tsrc);
		if (TrackTexture.MiddleRect.getArea() == 0) {
			setTrackImage(guiScalingImageButton(
					Environment->getVideoDriver(), texture,
							AbsoluteRect.getWidth(), AbsoluteRect.getHeight()));
		} else {
			setTrackImage(texture);
		}
	}

	if (style.isNotDefault(StyleSpec::PADDING)) {
		TrackPadding = style.getRect(StyleSpec::PADDING, core::rect<s32>());
		TrackPadding.UpperLeftCorner.X =
				core::s32_clamp(TrackPadding.UpperLeftCorner.X, 0, AbsoluteRect.getWidth() / 2);
		TrackPadding.UpperLeftCorner.Y =
				core::s32_clamp(TrackPadding.UpperLeftCorner.Y, 0, AbsoluteRect.getHeight() / 2);
		TrackPadding.LowerRightCorner.X =
				core::s32_clamp(TrackPadding.LowerRightCorner.X, -AbsoluteRect.getWidth() / 2, 0);
		TrackPadding.LowerRightCorner.Y =
				core::s32_clamp(TrackPadding.LowerRightCorner.Y, -AbsoluteRect.getHeight() / 2, 0);
	}

	ThumbTexture.MiddleRect = style.getRect(StyleSpec::FGIMG_MIDDLE, core::rect<s32>());

	if (style.isNotDefault(StyleSpec::FGIMG)) {
		video::ITexture *texture = style.getTexture(StyleSpec::FGIMG,
				m_tsrc);
		if (ThumbTexture.MiddleRect.getArea() == 0) {
			setThumbImage(guiScalingImageButton(
					Environment->getVideoDriver(), texture,
							AbsoluteRect.getWidth(), AbsoluteRect.getHeight()));
			// If a thumb image is given and the thumb middle is unspecified, we assume
			// that the user wishes to disable stretching of the thumb so as not to
			// break the image.
			StaticThumb = true;
		} else {
			setThumbImage(texture);
		}
	}
}

void GUIScrollBar::setStyles(const std::array<StyleSpec, StyleSpec::NUM_STATES>& styles,
		const std::array<StyleSpec, StyleSpec::NUM_STATES>& up_arrow_styles,
		const std::array<StyleSpec, StyleSpec::NUM_STATES>& down_arrow_styles)
{
	Styles = styles;
	UpArrowStyles = up_arrow_styles;
	DownArrowStyles = down_arrow_styles;
	setFromStyle(StyleSpec::getStyleFromStatePropagation(Styles, StyleSpec::STATE_DEFAULT));
	refreshControls();
}
