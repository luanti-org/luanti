// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "guiButton.h"


#include "client/guiscalingfilter.h"
#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "IGUIFont.h"
#include "irrlicht_changes/static_text.h"
#include "porting.h"
#include "StyleSpec.h"
#include "util/numeric.h"


using namespace gui;

// Multiply with a color to get the default corresponding hovered color
#define COLOR_HOVERED_MOD 1.25f

// Multiply with a color to get the default corresponding pressed color
#define COLOR_PRESSED_MOD 0.85f

//! constructor
GUIButton::GUIButton(IGUIEnvironment* environment, IGUIElement* parent,
		s32 id, core::rect<s32> rectangle, ISimpleTextureSource *tsrc,
		bool noclip) :
	IGUIButton(environment, parent, id, rectangle),
	TSrc(tsrc)
{
	setNotClipped(noclip);

	// This element can be tabbed.
	setTabStop(true);
	setTabOrder(-1);

	// PATCH
	for (size_t i = 0; i < 4; i++) {
		Colors[i] = Environment->getSkin()->getColor((EGUI_DEFAULT_COLOR)i);
	}
	StaticText = gui::StaticText::add(Environment, Text.c_str(), core::rect<s32>(0,0,rectangle.getWidth(),rectangle.getHeight()), false, false, this, id);
	StaticText->setTextAlignment(EGUIA_CENTER, EGUIA_CENTER);
	// END PATCH
}

//! destructor
GUIButton::~GUIButton()
{
	if (OverrideFont)
		OverrideFont->drop();

	if (SpriteBank)
		SpriteBank->drop();
}


//! Sets if the images should be scaled to fit the button
void GUIButton::setScaleImage(bool scaleImage)
{
	ScaleImage = scaleImage;
}


//! Returns whether the button scale the used images
bool GUIButton::isScalingImage() const
{
	return ScaleImage;
}


//! Sets if the button should use the skin to draw its border
void GUIButton::setDrawBorder(bool border)
{
	DrawBorder = border;
}


void GUIButton::setSpriteBank(IGUISpriteBank* sprites)
{
	if (sprites)
		sprites->grab();

	if (SpriteBank)
		SpriteBank->drop();

	SpriteBank = sprites;
}

void GUIButton::setSprite(EGUI_BUTTON_STATE state, s32 index, video::SColor color, bool loop)
{
	ButtonSprites[(u32)state].Index	= index;
	ButtonSprites[(u32)state].Color	= color;
	ButtonSprites[(u32)state].Loop	= loop;
}

//! Get the sprite-index for the given state or -1 when no sprite is set
s32 GUIButton::getSpriteIndex(EGUI_BUTTON_STATE state) const
{
	return ButtonSprites[(u32)state].Index;
}

//! Get the sprite color for the given state. Color is only used when a sprite is set.
video::SColor GUIButton::getSpriteColor(EGUI_BUTTON_STATE state) const
{
	return ButtonSprites[(u32)state].Color;
}

//! Returns if the sprite in the given state does loop
bool GUIButton::getSpriteLoop(EGUI_BUTTON_STATE state) const
{
	return ButtonSprites[(u32)state].Loop;
}

//! called if an event happened.
bool GUIButton::OnEvent(const SEvent& event)
{
	if (!isEnabled())
		return IGUIElement::OnEvent(event);

	switch(event.EventType)
	{
	case EET_KEY_INPUT_EVENT:
		if (event.KeyInput.PressedDown &&
			(event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE))
		{
			if (!IsPushButton)
				setPressed(true);
			else
				setPressed(!Pressed);

			return true;
		}
		if (Pressed && !IsPushButton && event.KeyInput.PressedDown && event.KeyInput.Key == KEY_ESCAPE)
		{
			setPressed(false);
			return true;
		}
		else
		if (!event.KeyInput.PressedDown && Pressed &&
			(event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE))
		{

			if (!IsPushButton)
				setPressed(false);

			if (Parent)
			{
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
		if (event.GUIEvent.Caller == this)
		{
			if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST)
			{
				if (!IsPushButton)
					setPressed(false);
				FocusTime = (u32)porting::getTimeMs();
			}
			else if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUSED)
			{
				FocusTime = (u32)porting::getTimeMs();
			}
			else if (event.GUIEvent.EventType == EGET_ELEMENT_HOVERED || event.GUIEvent.EventType == EGET_ELEMENT_LEFT)
			{
				HoverTime = (u32)porting::getTimeMs();
			}
		}
		break;
	case EET_MOUSE_INPUT_EVENT:
		if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN)
		{
			// Sometimes formspec elements can receive mouse events when the
			// mouse is outside of the formspec. Thus, we test the position here.
			if ( !IsPushButton && AbsoluteClippingRect.isPointInside(
						core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y ))) {
				Environment->setFocus(this);
				setPressed(true);
			}

			return true;
		}
		else
		if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
		{
			bool wasPressed = Pressed;

			if ( !AbsoluteClippingRect.isPointInside( core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y ) ) )
			{
				if (!IsPushButton)
					setPressed(false);
				return true;
			}

			if (!IsPushButton)
				setPressed(false);
			else
			{
				setPressed(!Pressed);
			}

			if ((!IsPushButton && wasPressed && Parent) ||
				(IsPushButton && wasPressed != Pressed))
			{
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
void GUIButton::draw()
{
	if (!IsVisible)
		return;

	// PATCH
	// Track hovered state, if it has changed then we need to update the style.
	bool hovered = isHovered();
	bool focused = isFocused();
	if (hovered != WasHovered || focused != WasFocused) {
		WasHovered = hovered;
		WasFocused = focused;
		setFromState();
	}

	video::IVideoDriver* driver = Environment->getVideoDriver();
	IGUISkin *skin = Environment->getSkin();
	// END PATCH

	if (DrawBorder)
	{
		if (!Pressed)
		{
			// PATCH
			skin->drawColored3DButtonPaneStandard(this, AbsoluteRect,
					&AbsoluteClippingRect, Colors);
			// END PATCH
		}
		else
		{
			// PATCH
			skin->drawColored3DButtonPanePressed(this, AbsoluteRect,
					&AbsoluteClippingRect, Colors);
			// END PATCH
		}
	}

	const core::position2di buttonCenter(AbsoluteRect.getCenter());
	// PATCH
	// The image changes based on the state, so we use the default every time.
	EGUI_BUTTON_IMAGE_STATE imageState = EGBIS_IMAGE_UP;
	// END PATCH
	if ( ButtonImages[(u32)imageState].Texture )
	{
		core::position2d<s32> pos(buttonCenter);
		core::rect<s32> sourceRect(ButtonImages[(u32)imageState].SourceRect);
		if ( sourceRect.getWidth() == 0 && sourceRect.getHeight() == 0 )
			sourceRect = core::rect<s32>(core::position2di(0,0), ButtonImages[(u32)imageState].Texture->getOriginalSize());

		pos.X -= sourceRect.getWidth() / 2;
		pos.Y -= sourceRect.getHeight() / 2;

		if ( Pressed )
		{
			// Create a pressed-down effect by moving the image when it looks identical to the unpressed state image
			EGUI_BUTTON_IMAGE_STATE unpressedState = getImageState(false);
			if ( unpressedState == imageState || ButtonImages[(u32)imageState] == ButtonImages[(u32)unpressedState] )
			{
				pos.X += skin->getSize(EGDS_BUTTON_PRESSED_IMAGE_OFFSET_X);
				pos.Y += skin->getSize(EGDS_BUTTON_PRESSED_IMAGE_OFFSET_Y);
			}
		}

		// PATCH
		video::ITexture* texture = ButtonImages[(u32)imageState].Texture;
		video::SColor image_colors[] = { BgColor, BgColor, BgColor, BgColor };
		if (BgMiddle.getArea() == 0) {
			driver->draw2DImage(texture,
					ScaleImage? AbsoluteRect : core::rect<s32>(pos, sourceRect.getSize()),
					sourceRect, &AbsoluteClippingRect,
					image_colors, UseAlphaChannel);
		} else {
			draw2DImage9Slice(driver, texture,
					ScaleImage ? AbsoluteRect : core::rect<s32>(pos, sourceRect.getSize()),
					sourceRect, BgMiddle, &AbsoluteClippingRect, image_colors);
		}
		// END PATCH
	}

	if (SpriteBank)
	{
		if (isEnabled())
		{
			core::position2di pos(buttonCenter);
			// pressed / unpressed animation
			EGUI_BUTTON_STATE state = Pressed ? EGBS_BUTTON_DOWN : EGBS_BUTTON_UP;
			drawSprite(state, ClickTime, pos);

			// focused / unfocused animation
			state = Environment->hasFocus(this) ? EGBS_BUTTON_FOCUSED : EGBS_BUTTON_NOT_FOCUSED;
			drawSprite(state, FocusTime, pos);

			// mouse over / off animation
			state = isHovered() ? EGBS_BUTTON_MOUSE_OVER : EGBS_BUTTON_MOUSE_OFF;
			drawSprite(state, HoverTime, pos);
		}
		else
		{
			// draw disabled
//			drawSprite(EGBS_BUTTON_DISABLED, 0, pos);
		}
	}

	IGUIElement::draw();
}

void GUIButton::drawSprite(EGUI_BUTTON_STATE state, u32 startTime, const core::position2di& center)
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
			porting::getTimeMs() - startTime, ButtonSprites[stateIdx].Loop);
}

EGUI_BUTTON_IMAGE_STATE GUIButton::getImageState(bool pressed) const
{
	// PATCH
	return getImageState(pressed, ButtonImages);
	// END PATCH
}

EGUI_BUTTON_IMAGE_STATE GUIButton::getImageState(bool pressed, const ButtonImage* images) const
{
	// figure state we should have
	EGUI_BUTTON_IMAGE_STATE state = EGBIS_IMAGE_DISABLED;
	bool focused = isFocused();
	bool mouseOver = isHovered();
	if (isEnabled())
	{
		if ( pressed )
		{
			if ( focused && mouseOver )
				state = EGBIS_IMAGE_DOWN_FOCUSED_MOUSEOVER;
			else if ( focused )
				state = EGBIS_IMAGE_DOWN_FOCUSED;
			else if ( mouseOver )
				state = EGBIS_IMAGE_DOWN_MOUSEOVER;
			else
				state = EGBIS_IMAGE_DOWN;
		}
		else // !pressed
		{
			if ( focused && mouseOver )
				state = EGBIS_IMAGE_UP_FOCUSED_MOUSEOVER;
			else if ( focused )
				state = EGBIS_IMAGE_UP_FOCUSED;
			else if ( mouseOver )
				state = EGBIS_IMAGE_UP_MOUSEOVER;
			else
				state = EGBIS_IMAGE_UP;
		}
	}

	// find a compatible state that has images
	while ( state != EGBIS_IMAGE_UP && !images[(u32)state].Texture )
	{
		// PATCH
		switch ( state )
		{
			case EGBIS_IMAGE_UP_FOCUSED:
				state = EGBIS_IMAGE_UP;
				break;
			case EGBIS_IMAGE_UP_FOCUSED_MOUSEOVER:
				state = EGBIS_IMAGE_UP_FOCUSED;
				break;
			case EGBIS_IMAGE_DOWN_MOUSEOVER:
				state = EGBIS_IMAGE_DOWN;
				break;
			case EGBIS_IMAGE_DOWN_FOCUSED:
				state = EGBIS_IMAGE_DOWN;
				break;
			case EGBIS_IMAGE_DOWN_FOCUSED_MOUSEOVER:
				state = EGBIS_IMAGE_DOWN_FOCUSED;
				break;
			case EGBIS_IMAGE_DISABLED:
				if ( pressed )
					state = EGBIS_IMAGE_DOWN;
				else
					state = EGBIS_IMAGE_UP;
				break;
			default:
				state = EGBIS_IMAGE_UP;
		}
		// END PATCH
	}

	return state;
}

//! sets another skin independent font. if this is set to zero, the button uses the font of the skin.
void GUIButton::setOverrideFont(IGUIFont* font)
{
	if (OverrideFont == font)
		return;

	if (OverrideFont)
		OverrideFont->drop();

	OverrideFont = font;

	if (OverrideFont)
		OverrideFont->grab();

	StaticText->setOverrideFont(font);
}

//! Gets the override font (if any)
IGUIFont * GUIButton::getOverrideFont() const
{
	return OverrideFont;
}

//! Get the font which is used right now for drawing
IGUIFont* GUIButton::getActiveFont() const
{
	if ( OverrideFont )
		return OverrideFont;
	IGUISkin* skin = Environment->getSkin();
	if (skin)
		return skin->getFont(EGDF_BUTTON);
	return 0;
}

//! Sets another color for the text.
void GUIButton::setOverrideColor(video::SColor color)
{
	OverrideColor = color;
	OverrideColorEnabled = true;

	StaticText->setOverrideColor(color);
}

video::SColor GUIButton::getOverrideColor() const
{
	return OverrideColor;
}

video::SColor GUIButton::getActiveColor() const
{
	return video::SColor(0,0,0,0); // unused?
}

void GUIButton::enableOverrideColor(bool enable)
{
	OverrideColorEnabled = enable;
}

bool GUIButton::isOverrideColorEnabled() const
{
	return OverrideColorEnabled;
}

void GUIButton::setImage(EGUI_BUTTON_IMAGE_STATE state, video::ITexture* image, const core::rect<s32>& sourceRect)
{
	if ( state >= EGBIS_COUNT )
		return;

	if ( image )
		image->grab();

	u32 stateIdx = (u32)state;
	if ( ButtonImages[stateIdx].Texture )
		ButtonImages[stateIdx].Texture->drop();

	ButtonImages[stateIdx].Texture = image;
	ButtonImages[stateIdx].SourceRect = sourceRect;
}

// PATCH
void GUIButton::setImage(video::ITexture* image)
{
	setImage(gui::EGBIS_IMAGE_UP, image);
}

void GUIButton::setImage(video::ITexture* image, const core::rect<s32>& pos)
{
	setImage(gui::EGBIS_IMAGE_UP, image, pos);
}

void GUIButton::setPressedImage(video::ITexture* image)
{
	setImage(gui::EGBIS_IMAGE_DOWN, image);
}

void GUIButton::setPressedImage(video::ITexture* image, const core::rect<s32>& pos)
{
	setImage(gui::EGBIS_IMAGE_DOWN, image, pos);
}

//! Sets the text displayed by the button
void GUIButton::setText(const wchar_t* text)
{
	StaticText->setText(text);

	IGUIButton::setText(text);
}
// END PATCH

//! Sets if the button should behave like a push button. Which means it
//! can be in two states: Normal or Pressed. With a click on the button,
//! the user can change the state of the button.
void GUIButton::setIsPushButton(bool isPushButton)
{
	IsPushButton = isPushButton;
}


//! Returns if the button is currently pressed
bool GUIButton::isPressed() const
{
	return Pressed;
}

// PATCH
//! Returns if this element (or one of its direct children) is hovered
bool GUIButton::isHovered() const
{
	IGUIElement *hovered = Environment->getHovered();
	return  hovered == this || (hovered != nullptr && hovered->getParent() == this);
}

//! Returns if this element (or one of its direct children) is focused
bool GUIButton::isFocused() const
{
	return Environment->hasFocus((IGUIElement*)this, true);
}
// END PATCH

//! Sets the pressed state of the button if this is a pushbutton
void GUIButton::setPressed(bool pressed)
{
	if (Pressed != pressed)
	{
		ClickTime = porting::getTimeMs();
		Pressed = pressed;
		setFromState();
	}
}


//! Returns whether the button is a push button
bool GUIButton::isPushButton() const
{
	return IsPushButton;
}


//! Sets if the alpha channel should be used for drawing images on the button (default is false)
void GUIButton::setUseAlphaChannel(bool useAlphaChannel)
{
	UseAlphaChannel = useAlphaChannel;
}


//! Returns if the alpha channel should be used for drawing images on the button
bool GUIButton::isAlphaChannelUsed() const
{
	return UseAlphaChannel;
}


bool GUIButton::isDrawingBorder() const
{
	return DrawBorder;
}


// PATCH
GUIButton* GUIButton::addButton(IGUIEnvironment *environment,
		const core::rect<s32>& rectangle, ISimpleTextureSource *tsrc,
		IGUIElement* parent, s32 id, const wchar_t* text,
		const wchar_t *tooltiptext)
{
	GUIButton* button = new GUIButton(environment, parent ? parent : environment->getRootGUIElement(), id, rectangle, tsrc);
	if (text)
		button->setText(text);

	if ( tooltiptext )
		button->setToolTipText ( tooltiptext );

	button->drop();
	return button;
}

void GUIButton::setColor(video::SColor color)
{
	BgColor = color;

	float d = 0.65f;
	for (size_t i = 0; i < 4; i++) {
		video::SColor base = Environment->getSkin()->getColor((gui::EGUI_DEFAULT_COLOR)i);
		Colors[i] = base.getInterpolated(color, d);
	}
}

//! Set element properties from a StyleSpec corresponding to the button state
void GUIButton::setFromState()
{
	StyleSpec::State state = StyleSpec::STATE_DEFAULT;

	if (isPressed())
		state = static_cast<StyleSpec::State>(state | StyleSpec::STATE_PRESSED);

	if (isHovered())
		state = static_cast<StyleSpec::State>(state | StyleSpec::STATE_HOVERED);

	if (isFocused())
		state = static_cast<StyleSpec::State>(state | StyleSpec::STATE_FOCUSED);

	setFromStyle(StyleSpec::getStyleFromStatePropagation(Styles, state));
}

//! Set element properties from a StyleSpec
void GUIButton::setFromStyle(const StyleSpec& style)
{
	bool hovered = (style.getState() & StyleSpec::STATE_HOVERED) != 0;
	bool pressed = (style.getState() & StyleSpec::STATE_PRESSED) != 0;

	if (style.isNotDefault(StyleSpec::BGCOLOR)) {
		setColor(style.getColor(StyleSpec::BGCOLOR));

		// If we have a propagated hover/press color, we need to automatically
		// lighten/darken it
		if (!Styles[style.getState()].isNotDefault(StyleSpec::BGCOLOR)) {
				if (pressed) {
					BgColor = multiplyColorValue(BgColor, COLOR_PRESSED_MOD);

					for (size_t i = 0; i < 4; i++)
						Colors[i] = multiplyColorValue(Colors[i], COLOR_PRESSED_MOD);
				} else if (hovered) {
					BgColor = multiplyColorValue(BgColor, COLOR_HOVERED_MOD);

					for (size_t i = 0; i < 4; i++)
						Colors[i] = multiplyColorValue(Colors[i], COLOR_HOVERED_MOD);
				}
		}

	} else {
		BgColor = video::SColor(255, 255, 255, 255);
		for (size_t i = 0; i < 4; i++) {
			video::SColor base =
					Environment->getSkin()->getColor((gui::EGUI_DEFAULT_COLOR)i);
			if (pressed) {
				Colors[i] = multiplyColorValue(base, COLOR_PRESSED_MOD);
			} else if (hovered) {
				Colors[i] = multiplyColorValue(base, COLOR_HOVERED_MOD);
			} else {
				Colors[i] = base;
			}
		}
	}

	if (style.isNotDefault(StyleSpec::TEXTCOLOR)) {
		setOverrideColor(style.getColor(StyleSpec::TEXTCOLOR));
	} else {
		setOverrideColor(video::SColor(255,255,255,255));
		OverrideColorEnabled = false;
	}
	setNotClipped(style.getBool(StyleSpec::NOCLIP, false));
	setDrawBorder(style.getBool(StyleSpec::BORDER, true));
	setUseAlphaChannel(style.getBool(StyleSpec::ALPHA, true));
	setOverrideFont(style.getFont());

	BgMiddle = style.getRect(StyleSpec::BGIMG_MIDDLE, BgMiddle);

	if (style.isNotDefault(StyleSpec::BGIMG)) {
		video::ITexture *texture = style.getTexture(StyleSpec::BGIMG,
				getTextureSource());
		if (BgMiddle.getArea() == 0) {
			setImage(guiScalingImageButton(
					Environment->getVideoDriver(), texture,
							AbsoluteRect.getWidth(), AbsoluteRect.getHeight()));
		} else {
			// Scaling happens in `draw2DImage9Slice`
			setImage(texture);
		}
		setScaleImage(true);
	} else {
		setImage(nullptr);
	}

	// Child padding and offset
	Padding = style.getRect(StyleSpec::PADDING, core::rect<s32>());
	Padding = core::rect<s32>(
			Padding.UpperLeftCorner + BgMiddle.UpperLeftCorner,
			Padding.LowerRightCorner + BgMiddle.LowerRightCorner);

	IGUISkin *skin = Environment->getSkin();
	core::vector2d<s32> defaultPressOffset(
			skin->getSize(gui::EGDS_BUTTON_PRESSED_IMAGE_OFFSET_X),
			skin->getSize(gui::EGDS_BUTTON_PRESSED_IMAGE_OFFSET_Y));
	ContentOffset = style.getVector2i(StyleSpec::CONTENT_OFFSET, isPressed()
			? defaultPressOffset
			: core::vector2d<s32>(0));

	core::rect<s32> childBounds(
				Padding.UpperLeftCorner.X + ContentOffset.X,
				Padding.UpperLeftCorner.Y + ContentOffset.Y,
				AbsoluteRect.getWidth() + Padding.LowerRightCorner.X + ContentOffset.X,
				AbsoluteRect.getHeight() + Padding.LowerRightCorner.Y + ContentOffset.Y);

	for (IGUIElement *child : getChildren()) {
		child->setRelativePosition(childBounds);
	}
}

//! Set the styles used for each state
void GUIButton::setStyles(const std::array<StyleSpec, StyleSpec::NUM_STATES>& styles)
{
	Styles = styles;
	setFromState();
}
// END PATCH
