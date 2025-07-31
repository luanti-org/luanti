// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUIButton.h"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "IGUIFont.h"
#include "os.h"

namespace gui
{

//! constructor
CGUIButton::CGUIButton(IGUIEnvironment *environment, IGUIElement *parent,
		s32 id, core::rect<s32> rectangle, bool noclip) :
		IGUIButton(environment, parent, id, rectangle),
		SpriteBank(0), OverrideFont(0),
		OverrideColorEnabled(false), OverrideColor(video::SColor(101, 255, 255, 255)),
		ClickTime(0), HoverTime(0), FocusTime(0),
		ClickShiftState(false), ClickControlState(false),
		IsPushButton(false), Pressed(false),
		UseAlphaChannel(false), DrawBorder(true), ScaleImage(false)
{
	setNotClipped(noclip);

	// This element can be tabbed.
	setTabStop(true);
	setTabOrder(-1);
}

//! destructor
CGUIButton::~CGUIButton()
{
	if (OverrideFont)
		OverrideFont->drop();

	if (SpriteBank)
		SpriteBank->drop();
}

//! Sets if the images should be scaled to fit the button
void CGUIButton::setScaleImage(bool scaleImage)
{
	ScaleImage = scaleImage;
}

//! Returns whether the button scale the used images
bool CGUIButton::isScalingImage() const
{
	return ScaleImage;
}

//! Sets if the button should use the skin to draw its border
void CGUIButton::setDrawBorder(bool border)
{
	DrawBorder = border;
}

void CGUIButton::setSpriteBank(IGUISpriteBank *sprites)
{
	if (sprites)
		sprites->grab();

	if (SpriteBank)
		SpriteBank->drop();

	SpriteBank = sprites;
}

void CGUIButton::setSprite(EGUI_BUTTON_STATE state, s32 index, video::SColor color, bool loop)
{
	ButtonSprites[(u32)state].Index = index;
	ButtonSprites[(u32)state].Color = color;
	ButtonSprites[(u32)state].Loop = loop;
}

//! Get the sprite-index for the given state or -1 when no sprite is set
s32 CGUIButton::getSpriteIndex(EGUI_BUTTON_STATE state) const
{
	return ButtonSprites[(u32)state].Index;
}

//! Get the sprite color for the given state. Color is only used when a sprite is set.
video::SColor CGUIButton::getSpriteColor(EGUI_BUTTON_STATE state) const
{
	return ButtonSprites[(u32)state].Color;
}

//! Returns if the sprite in the given state does loop
bool CGUIButton::getSpriteLoop(EGUI_BUTTON_STATE state) const
{
	return ButtonSprites[(u32)state].Loop;
}

//! called if an event happened.
bool CGUIButton::OnEvent(const SEvent &event)
{
	if (!isEnabled())
		return IGUIElement::OnEvent(event);

	switch (event.EventType) {
	case EET_KEY_INPUT_EVENT:
		if (event.KeyInput.PressedDown &&
				(event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE)) {
			if (!IsPushButton)
				setPressed(true);
			else
				setPressed(!Pressed);

			return true;
		}
		if (Pressed && !IsPushButton && event.KeyInput.PressedDown && event.KeyInput.Key == KEY_ESCAPE) {
			setPressed(false);
			return true;
		} else if (!event.KeyInput.PressedDown && Pressed &&
				   (event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE)) {

			if (!IsPushButton)
				setPressed(false);

			if (Parent) {
				ClickShiftState = event.KeyInput.Shift;
				ClickControlState = event.KeyInput.Control;

				SEvent newEvent;
				newEvent.EventType = EET_GUI_EVENT;
				newEvent.GUIEvent.Caller = this;
				newEvent.GUIEvent.Element = 0;
				newEvent.GUIEvent.EventType = EGET_BUTTON_CLICKED;
				Parent->OnEvent(newEvent);
			}
			return true;
		}
		break;
	case EET_GUI_EVENT:
		if (event.GUIEvent.Caller == this) {
			if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST) {
				if (!IsPushButton)
					setPressed(false);
				FocusTime = os::Timer::getTime();
			} else if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUSED) {
				FocusTime = os::Timer::getTime();
			} else if (event.GUIEvent.EventType == EGET_ELEMENT_HOVERED || event.GUIEvent.EventType == EGET_ELEMENT_LEFT) {
				HoverTime = os::Timer::getTime();
			}
		}
		break;
	case EET_MOUSE_INPUT_EVENT:
		if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
			if (!IsPushButton)
				setPressed(true);

			return true;
		} else if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP) {
			bool wasPressed = Pressed;

			if (!AbsoluteClippingRect.isPointInside(core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
				if (!IsPushButton)
					setPressed(false);
				return true;
			}

			if (!IsPushButton)
				setPressed(false);
			else {
				setPressed(!Pressed);
			}

			if ((!IsPushButton && wasPressed && Parent) ||
					(IsPushButton && wasPressed != Pressed)) {
				ClickShiftState = event.MouseInput.Shift;
				ClickControlState = event.MouseInput.Control;

				SEvent newEvent;
				newEvent.EventType = EET_GUI_EVENT;
				newEvent.GUIEvent.Caller = this;
				newEvent.GUIEvent.Element = 0;
				newEvent.GUIEvent.EventType = EGET_BUTTON_CLICKED;
				Parent->OnEvent(newEvent);
			}

			return true;
		}
		break;
	default:
		break;
	}

	return Parent ? Parent->OnEvent(event) : false;
}

//! draws the element and its children
void CGUIButton::draw()
{
	if (!IsVisible)
		return;

	IGUISkin *skin = Environment->getSkin();
	video::IVideoDriver *driver = Environment->getVideoDriver();

	if (DrawBorder) {
		if (!Pressed) {
			skin->draw3DButtonPaneStandard(this, AbsoluteRect, &AbsoluteClippingRect);
		} else {
			skin->draw3DButtonPanePressed(this, AbsoluteRect, &AbsoluteClippingRect);
		}
	}

	const core::position2di buttonCenter(AbsoluteRect.getCenter());

	EGUI_BUTTON_IMAGE_STATE imageState = getImageState(Pressed);
	if (ButtonImages[(u32)imageState].Texture) {
		core::position2d<s32> pos(buttonCenter);
		core::rect<s32> sourceRect(ButtonImages[(u32)imageState].SourceRect);
		if (sourceRect.getWidth() == 0 && sourceRect.getHeight() == 0)
			sourceRect = core::rect<s32>(core::position2di(0, 0), ButtonImages[(u32)imageState].Texture->getOriginalSize());

		pos.X -= sourceRect.getWidth() / 2;
		pos.Y -= sourceRect.getHeight() / 2;

		if (Pressed) {
			// Create a pressed-down effect by moving the image when it looks identical to the unpressed state image
			EGUI_BUTTON_IMAGE_STATE unpressedState = getImageState(false);
			if (unpressedState == imageState || ButtonImages[(u32)imageState] == ButtonImages[(u32)unpressedState]) {
				pos.X += skin->getSize(EGDS_BUTTON_PRESSED_IMAGE_OFFSET_X);
				pos.Y += skin->getSize(EGDS_BUTTON_PRESSED_IMAGE_OFFSET_Y);
			}
		}

		driver->draw2DImage(ButtonImages[(u32)imageState].Texture,
				ScaleImage ? AbsoluteRect : core::rect<s32>(pos, sourceRect.getSize()),
				sourceRect, &AbsoluteClippingRect,
				0, UseAlphaChannel);
	}

	if (SpriteBank) {
		core::position2di pos(buttonCenter);
		if (Pressed) {
			pos.X += skin->getSize(EGDS_BUTTON_PRESSED_SPRITE_OFFSET_X);
			pos.Y += skin->getSize(EGDS_BUTTON_PRESSED_SPRITE_OFFSET_Y);
		}

		if (isEnabled()) {
			// pressed / unpressed animation
			EGUI_BUTTON_STATE state = Pressed ? EGBS_BUTTON_DOWN : EGBS_BUTTON_UP;
			drawSprite(state, ClickTime, pos);

			// focused / unfocused animation
			state = Environment->hasFocus(this) ? EGBS_BUTTON_FOCUSED : EGBS_BUTTON_NOT_FOCUSED;
			drawSprite(state, FocusTime, pos);

			// mouse over / off animation
			state = Environment->getHovered() == this ? EGBS_BUTTON_MOUSE_OVER : EGBS_BUTTON_MOUSE_OFF;
			drawSprite(state, HoverTime, pos);
		} else {
			// draw disabled
			drawSprite(EGBS_BUTTON_DISABLED, 0, pos);
		}
	}

	if (Text.size()) {
		IGUIFont *font = getActiveFont();

		core::rect<s32> rect = AbsoluteRect;
		if (Pressed) {
			rect.UpperLeftCorner.X += skin->getSize(EGDS_BUTTON_PRESSED_TEXT_OFFSET_X);
			rect.UpperLeftCorner.Y += skin->getSize(EGDS_BUTTON_PRESSED_TEXT_OFFSET_Y);
		}

		if (font)
			font->draw(Text.c_str(), rect,
					getActiveColor(),
					true, true, &AbsoluteClippingRect);
	}

	IGUIElement::draw();
}

void CGUIButton::drawSprite(EGUI_BUTTON_STATE state, u32 startTime, const core::position2di &center)
{
	u32 stateIdx = (u32)state;
	s32 spriteIdx = ButtonSprites[stateIdx].Index;
	if (spriteIdx == -1)
		return;

	u32 rectIdx = SpriteBank->getSprites()[spriteIdx].Frames[0].rectNumber;
	core::rect<s32> srcRect = SpriteBank->getPositions()[rectIdx];

	IGUISkin *skin = Environment->getSkin();
	s32 scale = std::max(std::floor(skin->getScale()), 1.0f);
	core::rect<s32> rect(center, srcRect.getSize() * scale);
	rect -= rect.getSize() / 2;

	const video::SColor colors[] = {
		ButtonSprites[stateIdx].Color,
		ButtonSprites[stateIdx].Color,
		ButtonSprites[stateIdx].Color,
		ButtonSprites[stateIdx].Color,
	};
	SpriteBank->draw2DSprite(spriteIdx, rect, &AbsoluteClippingRect, colors,
			os::Timer::getTime() - startTime, ButtonSprites[stateIdx].Loop);
}

EGUI_BUTTON_IMAGE_STATE CGUIButton::getImageState(bool pressed) const
{
	// figure state we should have
	EGUI_BUTTON_IMAGE_STATE state = EGBIS_IMAGE_DISABLED;
	bool focused = Environment->hasFocus(this);
	bool mouseOver = static_cast<const IGUIElement *>(Environment->getHovered()) == this; // (static cast for Borland)
	if (isEnabled()) {
		if (pressed) {
			if (focused && mouseOver)
				state = EGBIS_IMAGE_DOWN_FOCUSED_MOUSEOVER;
			else if (focused)
				state = EGBIS_IMAGE_DOWN_FOCUSED;
			else if (mouseOver)
				state = EGBIS_IMAGE_DOWN_MOUSEOVER;
			else
				state = EGBIS_IMAGE_DOWN;
		} else // !pressed
		{
			if (focused && mouseOver)
				state = EGBIS_IMAGE_UP_FOCUSED_MOUSEOVER;
			else if (focused)
				state = EGBIS_IMAGE_UP_FOCUSED;
			else if (mouseOver)
				state = EGBIS_IMAGE_UP_MOUSEOVER;
			else
				state = EGBIS_IMAGE_UP;
		}
	}

	// find a compatible state that has images
	while (state != EGBIS_IMAGE_UP && !ButtonImages[(u32)state].Texture) {
		switch (state) {
		case EGBIS_IMAGE_UP_FOCUSED:
			state = EGBIS_IMAGE_UP_MOUSEOVER;
			break;
		case EGBIS_IMAGE_UP_FOCUSED_MOUSEOVER:
			state = EGBIS_IMAGE_UP_FOCUSED;
			break;
		case EGBIS_IMAGE_DOWN_MOUSEOVER:
			state = EGBIS_IMAGE_DOWN;
			break;
		case EGBIS_IMAGE_DOWN_FOCUSED:
			state = EGBIS_IMAGE_DOWN_MOUSEOVER;
			break;
		case EGBIS_IMAGE_DOWN_FOCUSED_MOUSEOVER:
			state = EGBIS_IMAGE_DOWN_FOCUSED;
			break;
		case EGBIS_IMAGE_DISABLED:
			if (pressed)
				state = EGBIS_IMAGE_DOWN;
			else
				state = EGBIS_IMAGE_UP;
			break;
		default:
			state = EGBIS_IMAGE_UP;
		}
	}

	return state;
}

//! sets another skin independent font. if this is set to zero, the button uses the font of the skin.
void CGUIButton::setOverrideFont(IGUIFont *font)
{
	if (OverrideFont == font)
		return;

	if (OverrideFont)
		OverrideFont->drop();

	OverrideFont = font;

	if (OverrideFont)
		OverrideFont->grab();
}

//! Gets the override font (if any)
IGUIFont *CGUIButton::getOverrideFont() const
{
	return OverrideFont;
}

//! Get the font which is used right now for drawing
IGUIFont *CGUIButton::getActiveFont() const
{
	if (OverrideFont)
		return OverrideFont;
	IGUISkin *skin = Environment->getSkin();
	if (skin)
		return skin->getFont(EGDF_BUTTON);
	return 0;
}

//! Sets another color for the text.
void CGUIButton::setOverrideColor(video::SColor color)
{
	OverrideColor = color;
	OverrideColorEnabled = true;
}

video::SColor CGUIButton::getOverrideColor() const
{
	return OverrideColor;
}

video::SColor CGUIButton::getActiveColor() const
{
	if (OverrideColorEnabled)
		return OverrideColor;
	IGUISkin *skin = Environment->getSkin();
	if (skin)
		return OverrideColorEnabled ? OverrideColor : skin->getColor(isEnabled() ? EGDC_BUTTON_TEXT : EGDC_GRAY_TEXT);
	return OverrideColor;
}

void CGUIButton::enableOverrideColor(bool enable)
{
	OverrideColorEnabled = enable;
}

bool CGUIButton::isOverrideColorEnabled() const
{
	return OverrideColorEnabled;
}

void CGUIButton::setImage(EGUI_BUTTON_IMAGE_STATE state, video::ITexture *image, const core::rect<s32> &sourceRect)
{
	if (state >= EGBIS_COUNT)
		return;

	if (image)
		image->grab();

	u32 stateIdx = (u32)state;
	if (ButtonImages[stateIdx].Texture)
		ButtonImages[stateIdx].Texture->drop();

	ButtonImages[stateIdx].Texture = image;
	ButtonImages[stateIdx].SourceRect = sourceRect;
}

//! Sets if the button should behave like a push button. Which means it
//! can be in two states: Normal or Pressed. With a click on the button,
//! the user can change the state of the button.
void CGUIButton::setIsPushButton(bool isPushButton)
{
	IsPushButton = isPushButton;
}

//! Returns if the button is currently pressed
bool CGUIButton::isPressed() const
{
	return Pressed;
}

//! Sets the pressed state of the button if this is a pushbutton
void CGUIButton::setPressed(bool pressed)
{
	if (Pressed != pressed) {
		ClickTime = os::Timer::getTime();
		Pressed = pressed;
	}
}

//! Returns whether the button is a push button
bool CGUIButton::isPushButton() const
{
	return IsPushButton;
}

//! Sets if the alpha channel should be used for drawing images on the button (default is false)
void CGUIButton::setUseAlphaChannel(bool useAlphaChannel)
{
	UseAlphaChannel = useAlphaChannel;
}

//! Returns if the alpha channel should be used for drawing images on the button
bool CGUIButton::isAlphaChannelUsed() const
{
	return UseAlphaChannel;
}

bool CGUIButton::isDrawingBorder() const
{
	return DrawBorder;
}

} // end namespace gui
