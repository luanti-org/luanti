// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIButton.h"
#include "IGUISpriteBank.h"
#include "ITexture.h"
#include "SColor.h"

namespace gui
{

class CGUIButton : public IGUIButton
{
public:
	//! constructor
	CGUIButton(IGUIEnvironment *environment, IGUIElement *parent,
			s32 id, core::rect<s32> rectangle, bool noclip = false);

	//! destructor
	virtual ~CGUIButton();

	//! called if an event happened.
	bool OnEvent(const SEvent &event) override;

	//! draws the element and its children
	void draw() override;

	//! sets another skin independent font. if this is set to zero, the button uses the font of the skin.
	void setOverrideFont(IGUIFont *font = 0) override;

	//! Gets the override font (if any)
	IGUIFont *getOverrideFont() const override;

	//! Get the font which is used right now for drawing
	IGUIFont *getActiveFont() const override;

	//! Sets another color for the button text.
	void setOverrideColor(video::SColor color) override;

	//! Gets the override color
	video::SColor getOverrideColor(void) const override;

	//! Gets the currently used text color
	video::SColor getActiveColor() const override;

	//! Sets if the button text should use the override color or the color in the gui skin.
	void enableOverrideColor(bool enable) override;

	//! Checks if an override color is enabled
	bool isOverrideColorEnabled(void) const override;

	//! Sets an image which should be displayed on the button when it is in the given state.
	void setImage(EGUI_BUTTON_IMAGE_STATE state, video::ITexture *image = 0, const core::rect<s32> &sourceRect = core::rect<s32>(0, 0, 0, 0)) override;

	//! Sets an image which should be displayed on the button when it is in normal state.
	void setImage(video::ITexture *image = 0) override
	{
		setImage(EGBIS_IMAGE_UP, image);
	}

	//! Sets an image which should be displayed on the button when it is in normal state.
	void setImage(video::ITexture *image, const core::rect<s32> &pos) override
	{
		setImage(EGBIS_IMAGE_UP, image, pos);
	}

	//! Sets an image which should be displayed on the button when it is in pressed state.
	void setPressedImage(video::ITexture *image = 0) override
	{
		setImage(EGBIS_IMAGE_DOWN, image);
	}

	//! Sets an image which should be displayed on the button when it is in pressed state.
	void setPressedImage(video::ITexture *image, const core::rect<s32> &pos) override
	{
		setImage(EGBIS_IMAGE_DOWN, image, pos);
	}

	//! Sets the sprite bank used by the button
	void setSpriteBank(IGUISpriteBank *bank = 0) override;

	//! Sets the animated sprite for a specific button state
	/** \param index: Number of the sprite within the sprite bank, use -1 for no sprite
	\param state: State of the button to set the sprite for
	\param index: The sprite number from the current sprite bank
	\param color: The color of the sprite
	*/
	virtual void setSprite(EGUI_BUTTON_STATE state, s32 index,
			video::SColor color = video::SColor(255, 255, 255, 255),
			bool loop = false) override;

	//! Get the sprite-index for the given state or -1 when no sprite is set
	s32 getSpriteIndex(EGUI_BUTTON_STATE state) const override;

	//! Get the sprite color for the given state. Color is only used when a sprite is set.
	video::SColor getSpriteColor(EGUI_BUTTON_STATE state) const override;

	//! Returns if the sprite in the given state does loop
	bool getSpriteLoop(EGUI_BUTTON_STATE state) const override;

	//! Sets if the button should behave like a push button. Which means it
	//! can be in two states: Normal or Pressed. With a click on the button,
	//! the user can change the state of the button.
	void setIsPushButton(bool isPushButton = true) override;

	//! Checks whether the button is a push button
	bool isPushButton() const override;

	//! Sets the pressed state of the button if this is a pushbutton
	void setPressed(bool pressed = true) override;

	//! Returns if the button is currently pressed
	bool isPressed() const override;

	//! Sets if the button should use the skin to draw its border
	void setDrawBorder(bool border = true) override;

	//! Checks if the button face and border are being drawn
	bool isDrawingBorder() const override;

	//! Sets if the alpha channel should be used for drawing images on the button (default is false)
	void setUseAlphaChannel(bool useAlphaChannel = true) override;

	//! Checks if the alpha channel should be used for drawing images on the button
	bool isAlphaChannelUsed() const override;

	//! Sets if the button should scale the button images to fit
	void setScaleImage(bool scaleImage = true) override;

	//! Checks whether the button scales the used images
	bool isScalingImage() const override;

	//! Get if the shift key was pressed in last EGET_BUTTON_CLICKED event
	bool getClickShiftState() const override
	{
		return ClickShiftState;
	}

	//! Get if the control key was pressed in last EGET_BUTTON_CLICKED event
	bool getClickControlState() const override
	{
		return ClickControlState;
	}

protected:
	void drawSprite(EGUI_BUTTON_STATE state, u32 startTime, const core::position2di &center);
	EGUI_BUTTON_IMAGE_STATE getImageState(bool pressed) const;

private:
	struct ButtonSprite
	{
		ButtonSprite() :
				Index(-1), Loop(false)
		{
		}

		bool operator==(const ButtonSprite &other) const
		{
			return Index == other.Index && Color == other.Color && Loop == other.Loop;
		}

		s32 Index;
		video::SColor Color;
		bool Loop;
	};

	ButtonSprite ButtonSprites[EGBS_COUNT];
	IGUISpriteBank *SpriteBank;

	struct ButtonImage
	{
		ButtonImage() :
				Texture(0), SourceRect(core::rect<s32>(0, 0, 0, 0))
		{
		}

		ButtonImage(const ButtonImage &other) :
				Texture(0), SourceRect(core::rect<s32>(0, 0, 0, 0))
		{
			*this = other;
		}

		~ButtonImage()
		{
			if (Texture)
				Texture->drop();
		}

		ButtonImage &operator=(const ButtonImage &other)
		{
			if (this == &other)
				return *this;

			if (other.Texture)
				other.Texture->grab();
			if (Texture)
				Texture->drop();
			Texture = other.Texture;
			SourceRect = other.SourceRect;
			return *this;
		}

		bool operator==(const ButtonImage &other) const
		{
			return Texture == other.Texture && SourceRect == other.SourceRect;
		}

		video::ITexture *Texture;
		core::rect<s32> SourceRect;
	};

	ButtonImage ButtonImages[EGBIS_COUNT];

	IGUIFont *OverrideFont;

	bool OverrideColorEnabled;
	video::SColor OverrideColor;

	u32 ClickTime, HoverTime, FocusTime;

	bool ClickShiftState;
	bool ClickControlState;

	bool IsPushButton;
	bool Pressed;
	bool UseAlphaChannel;
	bool DrawBorder;
	bool ScaleImage;
};

} // end namespace gui
