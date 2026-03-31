// Copyright (C) 2002-2012 Nikolaus Gebhardt
// Copyright (C) 2019 Stuart Jones <stujones111@gmail.com>
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUIScrollBar.h"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "CGUIButton.h"
#include "IGUIFontBitmap.h"

namespace gui
{

//! constructor
CGUIScrollBar::CGUIScrollBar(IGUIEnvironment *environment,
		IGUIElement *parent, s32 id, core::rect<s32> rectangle,
		bool horizontal, bool noclip) :
		IGUIScrollBar(environment, parent, id, rectangle),
		UpButton(0),
		DownButton(0), Dragging(false), Horizontal(horizontal),
		DraggedBySlider(false), Pos(0), DrawPos(0),
		DrawHeight(0), Min(0), Max(100), SmallStep(10), LargeStep(50), DesiredPos(0),
		LastChange(0)
{
	refreshControls();

	setNotClipped(noclip);

	// this element can be tabbed to
	setTabStop(true);
	setTabOrder(-1);

	setPos(0);
}

//! destructor
CGUIScrollBar::~CGUIScrollBar()
{
	if (UpButton)
		UpButton->drop();

	if (DownButton)
		DownButton->drop();
}

//! called if an event happened.
bool CGUIScrollBar::OnEvent(const SEvent &event)
{
	if (isEnabled()) {

		switch (event.EventType) {
		case EET_KEY_INPUT_EVENT:
			if (event.KeyInput.PressedDown) {
				const s32 oldPos = getTargetPos();
				bool absorb = true;
				switch (event.KeyInput.Key) {
				case KEY_LEFT:
				case KEY_UP:
					setPosInterpolated(oldPos - SmallStep);
					break;
				case KEY_RIGHT:
				case KEY_DOWN:
					setPosInterpolated(oldPos + SmallStep);
					break;
				case KEY_HOME:
					setPosInterpolated(Min);
					break;
				case KEY_PRIOR:
					setPosInterpolated(oldPos - LargeStep);
					break;
				case KEY_END:
					setPosInterpolated(Max);
					break;
				case KEY_NEXT:
					setPosInterpolated(oldPos + LargeStep);
					break;
				default:
					absorb = false;
				}

				if (absorb)
					return true;
			}
			break;
		case EET_GUI_EVENT:
			if (event.GUIEvent.EventType == EGET_BUTTON_CLICKED) {
				if (event.GUIEvent.Caller == UpButton)
					setPosAndSend(Pos - SmallStep);
				else if (event.GUIEvent.Caller == DownButton)
					setPosAndSend(Pos + SmallStep);

				return true;
			} else if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST) {
				if (event.GUIEvent.Caller == this)
					Dragging = false;
			}
			break;
		case EET_MOUSE_INPUT_EVENT: {
			const core::position2di p(event.MouseInput.X, event.MouseInput.Y);
			bool isInside = isPointInside(p);
			switch (event.MouseInput.Event) {
			case EMIE_MOUSE_WHEEL:
				if (Environment->hasFocus(this)) {
					s8 d = event.MouseInput.Wheel < 0 ? -1 : 1;
					s8 h = Horizontal ? 1 : -1;
					setPosInterpolated(getTargetPos() + (d * SmallStep * h));
					return true;
				}
				break;
			case EMIE_LMOUSE_PRESSED_DOWN: {
				if (isInside) {
					Dragging = true;
					DraggedBySlider = SliderRect.isPointInside(p);
					const s32 newPos = getPosFromMousePos(p);
					if (DraggedBySlider) {
						core::vector2di corner = SliderRect.UpperLeftCorner;
						DragOffset = Horizontal ? p.X - corner.X : p.Y - corner.Y;
					} else if (JumpToTrayPos) {
						setPosAndSend(newPos);
						// drag in the middle
						DragOffset = DrawHeight / 2;
						DraggedBySlider = true;
					}
					DesiredPos = newPos;
					Environment->setFocus(this);
					return true;
				}
				break;
			}
			case EMIE_LMOUSE_LEFT_UP:
			case EMIE_MOUSE_MOVED: {
				if (!event.MouseInput.isLeftPressed())
					Dragging = false;

				if (!Dragging) {
					if (event.MouseInput.Event == EMIE_MOUSE_MOVED)
						break;
					return isInside;
				}

				if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
					Dragging = false;

				const s32 newPos = getPosFromMousePos(p);
				if (DraggedBySlider) {
					setPosAndSend(newPos);
				} else {
					DesiredPos = newPos;
				}
				return isInside;
			} break;

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

void CGUIScrollBar::OnPostRender(u32 timeMs)
{
	u32 deltaMs = timeMs - LastTimeMs;
	LastTimeMs = timeMs;

	interpolatePos(deltaMs);
	handleAutoScroll(deltaMs);

	if (Dragging && !DraggedBySlider && timeMs > LastChange + 200) {
		LastChange = timeMs;

		if (DesiredPos >= Pos + LargeStep)
			setPosAndSend(Pos + LargeStep);
		else if (DesiredPos <= Pos - LargeStep)
			setPosAndSend(Pos - LargeStep);
		else if (DesiredPos >= Pos - LargeStep && DesiredPos <= Pos + LargeStep)
			setPosAndSend(DesiredPos);
	}
}

//! draws the element and its children
void CGUIScrollBar::draw()
{
	if (!IsVisible)
		return;

	IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return;

	video::SColor iconColor = skin->getColor(isEnabled() ? EGDC_WINDOW_SYMBOL : EGDC_GRAY_WINDOW_SYMBOL);
	if (iconColor != CurrentIconColor) {
		refreshControls();
	}

	SliderRect = AbsoluteRect;

	// draws the background
	skin->draw2DRectangle(this, skin->getColor(EGDC_SCROLLBAR), SliderRect, &AbsoluteClippingRect);

	if (core::isnotzero(range())) {
		// recalculate slider rectangle
		if (Horizontal) {
			SliderRect.UpperLeftCorner.X = AbsoluteRect.UpperLeftCorner.X + DrawPos - DrawHeight / 2;
			SliderRect.LowerRightCorner.X = SliderRect.UpperLeftCorner.X + DrawHeight;
		} else {
			SliderRect.UpperLeftCorner.Y = AbsoluteRect.UpperLeftCorner.Y + DrawPos - DrawHeight / 2;
			SliderRect.LowerRightCorner.Y = SliderRect.UpperLeftCorner.Y + DrawHeight;
		}

		skin->draw3DButtonPaneStandard(this, SliderRect, &AbsoluteClippingRect);
	}

	// draw buttons
	IGUIElement::draw();
}


void CGUIScrollBar::setPageSize(s32 size)
{
	PageSize = size;
	updatePos();
}

void CGUIScrollBar::updateAbsolutePosition()
{
	IGUIElement::updateAbsolutePosition();
	// todo: properly resize
	refreshControls();
	updatePos();
}

void CGUIScrollBar::setArrowsVisible(ArrowVisibility visible)
{
	UpDownVisible = visible;
	refreshControls();
}

//!
s32 CGUIScrollBar::getPosFromMousePos(const core::position2di &pos) const
{
	f32 w, // tray width (size minus all buttons)
		p; // clicked position relative to the scrollbar

	s32 occupied = 2 * BorderSize + DrawHeight;
	s32 offset = 0; // from center position
	if (DraggedBySlider)
		offset = DragOffset - DrawHeight / 2;

	if (Horizontal) {
		w = RelativeRect.getWidth() - occupied;
		p = pos.X - AbsoluteRect.UpperLeftCorner.X - occupied / 2 - offset;
	} else {
		w = RelativeRect.getHeight() - occupied;
		p = pos.Y - AbsoluteRect.UpperLeftCorner.Y - occupied / 2 - offset;
	}
	return (s32)(p / w * range()) + Min;
}

void CGUIScrollBar::updatePos()
{
	setPosRaw(Pos);
}

void CGUIScrollBar::setPosRaw(const s32 pos)
{
	s32 thumb_area = 0;
	s32 thumb_min = 0;

	if (Horizontal) {
		thumb_min = std::min(RelativeRect.getHeight(), RelativeRect.getWidth() / 2);
		thumb_area = RelativeRect.getWidth() - BorderSize * 2;
	} else {
		thumb_min = std::min(RelativeRect.getWidth(), RelativeRect.getHeight() / 2);
		thumb_area = RelativeRect.getHeight() - BorderSize * 2;
	}

	if (PageSize >= 0) {
		// Auto-scaling
		DrawHeight = std::min<s32>(INT32_MAX,
				thumb_area * (f32(thumb_area + BorderSize * 2)) / f32(PageSize));
	}

	DrawHeight = core::s32_clamp(DrawHeight, thumb_min, thumb_area);
	Pos = core::s32_clamp(pos, Min, Max);

	f32 f = core::isnotzero(range()) ? (f32(thumb_area) - f32(DrawHeight)) / range()
					 : 1.0f;
	DrawPos = s32((f32(Pos - Min) * f) + (f32(DrawHeight) * 0.5f)) +
		BorderSize;
}

//! sets the position of the scrollbar
void CGUIScrollBar::setPos(s32 pos)
{
	setPosRaw(pos);
	TargetPos = std::nullopt;
}

void CGUIScrollBar::setPosAndSend(const s32 pos)
{
	const s32 oldPos = Pos;
	setPos(pos);
	if (Pos != oldPos && Parent) {
		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = this;
		e.GUIEvent.Element = nullptr;
		e.GUIEvent.EventType = EGET_SCROLL_BAR_CHANGED;
		Parent->OnEvent(e);
	}
}

void CGUIScrollBar::setPosInterpolated(const s32 pos)
{
	if (!SmoothScroll) {
		setPosAndSend(pos);
		return;
	}

	s32 clamped = core::s32_clamp(pos, Min, Max);
	if (Pos != clamped) {
		TargetPos = clamped;
	} else {
		TargetPos = std::nullopt;
	}
}

//! gets the small step value
s32 CGUIScrollBar::getSmallStep() const
{
	return SmallStep;
}

//! sets the small step value
void CGUIScrollBar::setSmallStep(s32 step)
{
	if (step > 0)
		SmallStep = step;
	else
		SmallStep = 10;
}

//! gets the small step value
s32 CGUIScrollBar::getLargeStep() const
{
	return LargeStep;
}

//! sets the small step value
void CGUIScrollBar::setLargeStep(s32 step)
{
	if (step > 0)
		LargeStep = step;
	else
		LargeStep = 50;
}

//! gets the maximum value of the scrollbar.
s32 CGUIScrollBar::getMax() const
{
	return Max;
}

//! sets the maximum value of the scrollbar.
void CGUIScrollBar::setMax(s32 max)
{
	Max = max;
	if (Min > Max)
		Min = Max;

	bool enable = core::isnotzero(range());
	UpButton->setEnabled(enable);
	DownButton->setEnabled(enable);
	updatePos();
}

//! gets the minimum value of the scrollbar.
s32 CGUIScrollBar::getMin() const
{
	return Min;
}

//! sets the minimum value of the scrollbar.
void CGUIScrollBar::setMin(s32 min)
{
	Min = min;
	if (Max < Min)
		Max = Min;

	bool enable = core::isnotzero(range());
	UpButton->setEnabled(enable);
	DownButton->setEnabled(enable);
	updatePos();
}

//! gets the current position of the scrollbar
s32 CGUIScrollBar::getPos() const
{
	return Pos;
}

s32 CGUIScrollBar::getTargetPos() const
{
	if (TargetPos.has_value()) {
		s32 clamped = core::s32_clamp(*TargetPos, Min, Max);
		return clamped;
	}
	return Pos;
}

//! refreshes the position and text on child buttons
void CGUIScrollBar::refreshControls()
{
	CurrentIconColor = video::SColor(255, 255, 255, 255);

	IGUISkin *skin = Environment->getSkin();
	IGUISpriteBank *sprites = 0;

	if (skin) {
		sprites = skin->getSpriteBank();
		CurrentIconColor = skin->getColor(isEnabled() ? EGDC_WINDOW_SYMBOL : EGDC_GRAY_WINDOW_SYMBOL);
	}

	if (Horizontal) {
		const s32 h = RelativeRect.getHeight();
		BorderSize = RelativeRect.getWidth() < h * 4 ? 0 : h;
		if (!UpButton) {
			UpButton = new CGUIButton(Environment, this, -1, {}, NoClip);
			UpButton->setSubElement(true);
			UpButton->setTabStop(false);
		}
		if (sprites) {
			UpButton->setSpriteBank(sprites);
			UpButton->setSprite(EGBS_BUTTON_UP, skin->getIcon(EGDI_CURSOR_LEFT), CurrentIconColor);
			UpButton->setSprite(EGBS_BUTTON_DOWN, skin->getIcon(EGDI_CURSOR_LEFT), CurrentIconColor);
		}
		UpButton->setRelativePosition(core::rect<s32>(0, 0, h, h));
		UpButton->setAlignment(EGUIA_UPPERLEFT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT);
		if (!DownButton) {
			DownButton = new CGUIButton(Environment, this, -1, {}, NoClip);
			DownButton->setSubElement(true);
			DownButton->setTabStop(false);
		}
		if (sprites) {
			DownButton->setSpriteBank(sprites);
			DownButton->setSprite(EGBS_BUTTON_UP, skin->getIcon(EGDI_CURSOR_RIGHT), CurrentIconColor);
			DownButton->setSprite(EGBS_BUTTON_DOWN, skin->getIcon(EGDI_CURSOR_RIGHT), CurrentIconColor);
		}
		DownButton->setRelativePosition(core::rect<s32>(RelativeRect.getWidth() - h, 0, RelativeRect.getWidth(), h));
		DownButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT);
	} else {
		const s32 w = RelativeRect.getWidth();
		BorderSize = RelativeRect.getHeight() < w * 4 ? 0 : w;
		if (!UpButton) {
			UpButton = new CGUIButton(Environment, this, -1, {}, NoClip);
			UpButton->setSubElement(true);
			UpButton->setTabStop(false);
		}
		if (sprites) {
			UpButton->setSpriteBank(sprites);
			UpButton->setSprite(EGBS_BUTTON_UP, skin->getIcon(EGDI_CURSOR_UP), CurrentIconColor);
			UpButton->setSprite(EGBS_BUTTON_DOWN, skin->getIcon(EGDI_CURSOR_UP), CurrentIconColor);
		}
		UpButton->setRelativePosition(core::rect<s32>(0, 0, w, w));
		UpButton->setAlignment(EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
		if (!DownButton) {
			DownButton = new CGUIButton(Environment, this, -1, {}, NoClip);
			DownButton->setSubElement(true);
			DownButton->setTabStop(false);
		}
		if (sprites) {
			DownButton->setSpriteBank(sprites);
			DownButton->setSprite(EGBS_BUTTON_UP, skin->getIcon(EGDI_CURSOR_DOWN), CurrentIconColor);
			DownButton->setSprite(EGBS_BUTTON_DOWN, skin->getIcon(EGDI_CURSOR_DOWN), CurrentIconColor);
		}
		DownButton->setRelativePosition(core::rect<s32>(0, RelativeRect.getHeight() - w, w, RelativeRect.getHeight()));
		DownButton->setAlignment(EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT);
	}

	// Automatically hide Up/Down if the space is constrained too much
	bool visible;
	//BorderSize = 0; // uncomment to test
	if (UpDownVisible == DEFAULT)
		visible = (BorderSize != 0);
	else if (UpDownVisible == HIDE) {
		visible = false;
		BorderSize = 0;
	} else {
		visible = true;
		if (Horizontal)
			BorderSize = RelativeRect.getHeight();
		else
			BorderSize = RelativeRect.getWidth();
	}

	UpButton->setVisible(visible);
	DownButton->setVisible(visible);
}

static inline s32 interpolate_scroll(s32 from, s32 to, f32 amount)
{
	s32 step = core::round32((to - from) * core::clamp(amount, 0.001f, 1.0f));
	if (step == 0)
		return to;
	return from + step;
}

void CGUIScrollBar::interpolatePos(u32 deltaMs)
{
	if (TargetPos.has_value()) {
		// Adjust to match 60 FPS. This also means that interpolation is
		// effectively disabled at <= 30 FPS.
		f32 amount = 0.5f * (deltaMs / 16.667f);
		setPosRaw(interpolate_scroll(Pos, *TargetPos, amount));
		if (Pos == TargetPos)
			TargetPos = std::nullopt;

		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = this;
		e.GUIEvent.Element = nullptr;
		e.GUIEvent.EventType = EGET_SCROLL_BAR_CHANGED;
		Parent->OnEvent(e);
	}
}

void CGUIScrollBar::handleAutoScroll(u32 deltaMs)
{
	const bool up_pressed = UpButton && UpButton->isPressed();
	const bool down_pressed = DownButton && DownButton->isPressed();

	// if neither is pressed, reset counter
	if (!up_pressed && !down_pressed) {
		AutoScrollMs = 0; // reset counter when no arrow is held
		return;
	}

	const u32 initial_delay = 300; // ms before repeating starts
	const u32 repeat_delay  = 150; // ms between repeats
	assert(initial_delay > repeat_delay);

	const bool is_initial = (AutoScrollMs == 0);
	// counter is 0, so start counting
	AutoScrollMs += deltaMs;

	// wait for initial delay
	if (AutoScrollMs < initial_delay && !is_initial)
		return;

	// after initial delay, repeat every repeat_delay
	const s32 autoscroll_stepsize = SmallStep * (up_pressed ? -1 : 1);
	setPosInterpolated(getTargetPos() + autoscroll_stepsize);
	if (!is_initial)
		AutoScrollMs -= repeat_delay;
}

} // end namespace gui
