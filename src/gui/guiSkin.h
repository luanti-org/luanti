// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __GUI_SKIN_H_INCLUDED__
#define __GUI_SKIN_H_INCLUDED__

#include "IrrCompileConfig.h"
#ifdef _IRR_COMPILE_WITH_GUI_

#include "IGUISkin.h"
#include "irrString.h"
#include <string>
#include "ITexture.h"

namespace irr
{
namespace video
{
	class IVideoDriver;
}
namespace gui
{
	class GUISkin : public IGUISkin
	{
	public:

		GUISkin(EGUI_SKIN_TYPE type, video::IVideoDriver* driver);

		//! destructor
		virtual ~GUISkin();

		//! returns default color
		virtual video::SColor getColor(EGUI_DEFAULT_COLOR color) const;

		//! sets a default color
		virtual void setColor(EGUI_DEFAULT_COLOR which, video::SColor newColor);

		//! returns size for the given size type
		virtual s32 getSize(EGUI_DEFAULT_SIZE size) const;

		//! sets a default size
		virtual void setSize(EGUI_DEFAULT_SIZE which, s32 size);

		//! returns the default font
		virtual IGUIFont* getFont(EGUI_DEFAULT_FONT which=EGDF_DEFAULT) const;

		//! sets a default font
		virtual void setFont(IGUIFont* font, EGUI_DEFAULT_FONT which=EGDF_DEFAULT);

		//! sets the sprite bank used for drawing icons
		virtual void setSpriteBank(IGUISpriteBank* bank);

		//! gets the sprite bank used for drawing icons
		virtual IGUISpriteBank* getSpriteBank() const;

		//! Returns a default icon
		/** Returns the sprite index within the sprite bank */
		virtual u32 getIcon(EGUI_DEFAULT_ICON icon) const;

		//! Sets a default icon
		/** Sets the sprite index used for drawing icons like arrows,
		close buttons and ticks in checkboxes
		\param icon: Enum specifying which icon to change
		\param index: The sprite index used to draw this icon */
		virtual void setIcon(EGUI_DEFAULT_ICON icon, u32 index);

		//! Returns a default text.
		/** For example for Message box button captions:
		"OK", "Cancel", "Yes", "No" and so on. */
		virtual const wchar_t* getDefaultText(EGUI_DEFAULT_TEXT text) const;

		//! Sets a default text.
		/** For example for Message box button captions:
		"OK", "Cancel", "Yes", "No" and so on. */
		virtual void setDefaultText(EGUI_DEFAULT_TEXT which, const wchar_t* newText);

		//! draws a standard 3d button pane
		/** Used for drawing for example buttons in normal state.
		It uses the colors EGDC_3D_DARK_SHADOW, EGDC_3D_HIGH_LIGHT, EGDC_3D_SHADOW and
		EGDC_3D_FACE for this. See EGUI_DEFAULT_COLOR for details.
		\param rect: Defining area where to draw.
		\param clip: Clip area.
		\param element: Pointer to the element which wishes to draw this. This parameter
		is usually not used by ISkin, but can be used for example by more complex
		implementations to find out how to draw the part exactly. */
		virtual void draw3DButtonPaneStandard(IGUIElement* element,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0)
		{
			drawColored3DButtonPaneStandard(element, rect,clip);
		}

		virtual void drawColored3DButtonPaneStandard(IGUIElement* element,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0,
				const video::SColor* colors=0);

		//! draws a pressed 3d button pane
		/** Used for drawing for example buttons in pressed state.
		It uses the colors EGDC_3D_DARK_SHADOW, EGDC_3D_HIGH_LIGHT, EGDC_3D_SHADOW and
		EGDC_3D_FACE for this. See EGUI_DEFAULT_COLOR for details.
		\param rect: Defining area where to draw.
		\param clip: Clip area.
		\param element: Pointer to the element which wishes to draw this. This parameter
		is usually not used by ISkin, but can be used for example by more complex
		implementations to find out how to draw the part exactly. */
		virtual void draw3DButtonPanePressed(IGUIElement* element,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0)
		{
			drawColored3DButtonPanePressed(element, rect, clip);
		}

		virtual void drawColored3DButtonPanePressed(IGUIElement* element,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0,
				const video::SColor* colors=0);

		//! draws a sunken 3d pane
		/** Used for drawing the background of edit, combo or check boxes.
		\param element: Pointer to the element which wishes to draw this. This parameter
		is usually not used by ISkin, but can be used for example by more complex
		implementations to find out how to draw the part exactly.
		\param bgcolor: Background color.
		\param flat: Specifies if the sunken pane should be flat or displayed as sunken
		deep into the ground.
		\param rect: Defining area where to draw.
		\param clip: Clip area.	*/
		virtual void draw3DSunkenPane(IGUIElement* element,
				video::SColor bgcolor, bool flat,
				bool fillBackGround,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0)
		{
			drawColored3DSunkenPane(element, bgcolor, flat, fillBackGround, rect, clip);
		}

		virtual void drawColored3DSunkenPane(IGUIElement* element,
				video::SColor bgcolor, bool flat,
				bool fillBackGround,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0,
				const video::SColor* colors=0);

		//! draws a window background
		/** Used for drawing the background of dialogs and windows.
		\param element: Pointer to the element which wishes to draw this. This parameter
		is usually not used by ISkin, but can be used for example by more complex
		implementations to find out how to draw the part exactly.
		\param titleBarColor: Title color.
		\param drawTitleBar: True to enable title drawing.
		\param rect: Defining area where to draw.
		\param clip: Clip area.
		\param checkClientArea: When set to non-null the function will not draw anything,
		but will instead return the clientArea which can be used for drawing by the calling window.
		That is the area without borders and without titlebar.
		\return Returns rect where it would be good to draw title bar text. This will
		work even when checkClientArea is set to a non-null value.*/
		virtual core::rect<s32> draw3DWindowBackground(IGUIElement* element,
				bool drawTitleBar, video::SColor titleBarColor,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip,
				core::rect<s32>* checkClientArea)
		{
			return drawColored3DWindowBackground(element, drawTitleBar, titleBarColor,
				rect, clip, checkClientArea);
		}

		virtual core::rect<s32> drawColored3DWindowBackground(IGUIElement* element,
				bool drawTitleBar, video::SColor titleBarColor,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip,
				core::rect<s32>* checkClientArea,
				const video::SColor* colors=0);

		//! draws a standard 3d menu pane
		/** Used for drawing for menus and context menus.
		It uses the colors EGDC_3D_DARK_SHADOW, EGDC_3D_HIGH_LIGHT, EGDC_3D_SHADOW and
		EGDC_3D_FACE for this. See EGUI_DEFAULT_COLOR for details.
		\param element: Pointer to the element which wishes to draw this. This parameter
		is usually not used by ISkin, but can be used for example by more complex
		implementations to find out how to draw the part exactly.
		\param rect: Defining area where to draw.
		\param clip: Clip area.	*/
		virtual void draw3DMenuPane(IGUIElement* element,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0)
		{
			drawColored3DMenuPane(element, rect, clip);
		}

		virtual void drawColored3DMenuPane(IGUIElement* element,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0,
				const video::SColor* colors=0);

		//! draws a standard 3d tool bar
		/** Used for drawing for toolbars and menus.
		\param element: Pointer to the element which wishes to draw this. This parameter
		is usually not used by ISkin, but can be used for example by more complex
		implementations to find out how to draw the part exactly.
		\param rect: Defining area where to draw.
		\param clip: Clip area.	*/
		virtual void draw3DToolBar(IGUIElement* element,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0)
		{
			drawColored3DToolBar(element, rect, clip);
		}

		virtual void drawColored3DToolBar(IGUIElement* element,
				const core::rect<s32>& rect,
				const core::rect<s32>* clip=0,
				const video::SColor* colors=0);

		//! draws a tab button
		/** Used for drawing for tab buttons on top of tabs.
		\param element: Pointer to the element which wishes to draw this. This parameter
		is usually not used by ISkin, but can be used for example by more complex
		implementations to find out how to draw the part exactly.
		\param active: Specifies if the tab is currently active.
		\param rect: Defining area where to draw.
		\param clip: Clip area.	*/
		virtual void draw3DTabButton(IGUIElement* element, bool active,
			const core::rect<s32>& rect, const core::rect<s32>* clip=0, EGUI_ALIGNMENT alignment=EGUIA_UPPERLEFT)
		{
			drawColored3DTabButton(element, active, rect, clip, alignment);
		}

		virtual void drawColored3DTabButton(IGUIElement* element, bool active,
			const core::rect<s32>& rect, const core::rect<s32>* clip=0, EGUI_ALIGNMENT alignment=EGUIA_UPPERLEFT,
			const video::SColor* colors=0);

		//! draws a tab control body
		/** \param element: Pointer to the element which wishes to draw this. This parameter
		is usually not used by ISkin, but can be used for example by more complex
		implementations to find out how to draw the part exactly.
		\param border: Specifies if the border should be drawn.
		\param background: Specifies if the background should be drawn.
		\param rect: Defining area where to draw.
		\param clip: Clip area.	*/
		virtual void draw3DTabBody(IGUIElement* element, bool border, bool background,
			const core::rect<s32>& rect, const core::rect<s32>* clip=0, s32 tabHeight=-1, EGUI_ALIGNMENT alignment=EGUIA_UPPERLEFT)
		{
			drawColored3DTabBody(element, border, background, rect, clip, tabHeight, alignment);
		}

		virtual void drawColored3DTabBody(IGUIElement* element, bool border, bool background,
			const core::rect<s32>& rect, const core::rect<s32>* clip=0, s32 tabHeight=-1, EGUI_ALIGNMENT alignment=EGUIA_UPPERLEFT,
			const video::SColor* colors=0);

		//! draws an icon, usually from the skin's sprite bank
		/** \param element: Pointer to the element which wishes to draw this icon.
		This parameter is usually not used by IGUISkin, but can be used for example
		by more complex implementations to find out how to draw the part exactly.
		\param icon: Specifies the icon to be drawn.
		\param position: The position to draw the icon
		\param starttime: The time at the start of the animation
		\param currenttime: The present time, used to calculate the frame number
		\param loop: Whether the animation should loop or not
		\param clip: Clip area.	*/
		virtual void drawIcon(IGUIElement* element, EGUI_DEFAULT_ICON icon,
				const core::position2di position,
				u32 starttime=0, u32 currenttime=0,
				bool loop=false, const core::rect<s32>* clip=0)
		{
			drawColoredIcon(element, icon, position, starttime, currenttime, loop, clip);
		}

		virtual void drawColoredIcon(IGUIElement* element, EGUI_DEFAULT_ICON icon,
				const core::position2di position,
				u32 starttime=0, u32 currenttime=0,
				bool loop=false, const core::rect<s32>* clip=0,
				const video::SColor* colors=0);

		//! draws a 2d rectangle.
		/** \param element: Pointer to the element which wishes to draw this icon.
		This parameter is usually not used by IGUISkin, but can be used for example
		by more complex implementations to find out how to draw the part exactly.
		\param color: Color of the rectangle to draw. The alpha component specifies how
		transparent the rectangle will be.
		\param pos: Position of the rectangle.
		\param clip: Pointer to rectangle against which the rectangle will be clipped.
		If the pointer is null, no clipping will be performed. */
		virtual void draw2DRectangle(IGUIElement* element, const video::SColor &color,
				const core::rect<s32>& pos, const core::rect<s32>* clip = 0);


		//! get the type of this skin
		virtual EGUI_SKIN_TYPE getType() const;

		//! gets the colors
		virtual void getColors(video::SColor* colors); // ::PATCH:

	private:

		video::SColor Colors[EGDC_COUNT];
		s32 Sizes[EGDS_COUNT];
		u32 Icons[EGDI_COUNT];
		IGUIFont* Fonts[EGDF_COUNT];
		IGUISpriteBank* SpriteBank;
		core::stringw Texts[EGDT_COUNT];
		video::IVideoDriver* Driver;
		bool UseGradient;

		EGUI_SKIN_TYPE Type;
	};

	#define set3DSkinColors(skin, button_color) \
		{ \
			skin->setColor(EGDC_3D_FACE, button_color); \
			skin->setColor(EGDC_3D_DARK_SHADOW, button_color, 0.25f); \
			skin->setColor(EGDC_3D_SHADOW, button_color, 0.5f); \
			skin->setColor(EGDC_3D_LIGHT, button_color); \
			skin->setColor(EGDC_3D_HIGH_LIGHT, button_color, 1.5f); \
		}

	#define getElementSkinColor(color) \
		{ \
			if (!Colors) \
			{ \
				IGUISkin* skin = Environment->getSkin(); \
				if (skin) \
					return skin->getColor(color); \
			} \
			return Colors[color]; \
		}

	#define setElementSkinColor(which, newColor, shading) \
		{ \
			if (!Colors) \
			{ \
				Colors = new video::SColor[EGDC_COUNT]; \
				GUISkin* skin = (GUISkin *)Environment->getSkin(); \
				if (skin) \
					skin->getColors(Colors); \
			} \
			Colors[which] = newColor; \
			setShading(Colors[which],shading); \
		}
} // end namespace gui
//! Sets the shading
inline void setShading(video::SColor &color,f32 s) // :PATCH:
{
	if (s < 1.0f)
	{
		color.setRed(color.getRed() * s);
		color.setGreen(color.getGreen() * s);
		color.setBlue(color.getBlue() * s);
	}
	else if (s > 1.0f)
	{
		s -= 1.0f;

		color.setRed(color.getRed() + (255 - color.getRed()) * s);
		color.setGreen(color.getGreen() + (255 - color.getGreen()) * s);
		color.setBlue(color.getBlue() + (255 - color.getBlue()) * s);
	}
}
} // end namespace irr


#endif // _IRR_COMPILE_WITH_GUI_

#endif
