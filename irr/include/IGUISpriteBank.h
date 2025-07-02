// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "irrArray.h"
#include "SColor.h"
#include "rect.h"

namespace video
{
class ITexture;
} // end namespace video

namespace gui
{

//! A single sprite frame.
// Note for implementer: Can't fix variable names to uppercase as this is a public interface used since a while
struct SGUISpriteFrame
{
	SGUISpriteFrame() :
			textureNumber(0), rectNumber(0)
	{
	}

	SGUISpriteFrame(u32 textureIndex, u32 positionIndex) :
			textureNumber(textureIndex), rectNumber(positionIndex)
	{
	}

	//! Texture index in IGUISpriteBank
	u32 textureNumber;

	//! Index in IGUISpriteBank::getPositions()
	u32 rectNumber;
};

//! A sprite composed of several frames.
// Note for implementer: Can't fix variable names to uppercase as this is a public interface used since a while
struct SGUISprite
{
	SGUISprite() :
			frameTime(0) {}
	SGUISprite(const SGUISpriteFrame &firstFrame) :
			frameTime(0)
	{
		Frames.push_back(firstFrame);
	}

	core::array<SGUISpriteFrame> Frames;
	u32 frameTime;
};

//! Sprite bank interface.
/** See http://http://irrlicht.sourceforge.net/forum//viewtopic.php?f=9&t=25742
 * for more information how to use the spritebank.
 */
class IGUISpriteBank : public virtual IReferenceCounted
{
public:
	//! Returns the list of rectangles held by the sprite bank
	virtual core::array<core::rect<s32>> &getPositions() = 0;

	//! Returns the array of animated sprites within the sprite bank
	virtual core::array<SGUISprite> &getSprites() = 0;

	//! Returns the number of textures held by the sprite bank
	virtual u32 getTextureCount() const = 0;

	//! Gets the texture with the specified index
	virtual video::ITexture *getTexture(u32 index) const = 0;

	//! Adds a texture to the sprite bank
	virtual void addTexture(video::ITexture *texture) = 0;

	//! Changes one of the textures in the sprite bank
	virtual void setTexture(u32 index, video::ITexture *texture) = 0;

	//! Add the texture and use it for a single non-animated sprite.
	/** The texture and the corresponding rectangle and sprite will all be added to the end of each array.
	 \returns The index of the sprite or -1 on failure */
	virtual s32 addTextureAsSprite(video::ITexture *texture) = 0;

	//! Clears sprites, rectangles and textures
	virtual void clear() = 0;

	//! Draws a sprite in 2d with position and color
	/**
	\param index Index of SGUISprite to draw
	\param pos Target position - depending on center value either the left-top or the sprite center is used as pivot
	\param clip Clipping rectangle, can be 0 when clipping is not wanted.
	\param color Color with which the image is drawn.
		Note that the alpha component is used. If alpha is other than
		255, the image will be transparent.
	\param starttime Tick when the first frame was drawn (only difference currenttime-starttime matters).
	\param currenttime To calculate the frame of animated sprites
	\param loop When true animations are looped
	\param center When true pivot is set to the sprite-center. So it affects pos.
	*/
	virtual void draw2DSprite(u32 index, const core::position2di &pos,
			const core::rect<s32> *clip = 0,
			const video::SColor &color = video::SColor(255, 255, 255, 255),
			u32 starttime = 0, u32 currenttime = 0,
			bool loop = true, bool center = false) = 0;

	//! Draws a sprite in 2d with destination rectangle and colors
	/**
		\param index Index of SGUISprite to draw
		\param destRect The sprite will be scaled to fit this target rectangle
		\param clip Clipping rectangle, can be 0 when clipping is not wanted.
		\param colors Array of 4 colors denoting the color values of
		the corners of the destRect
		\param timeTicks Current frame for animated sprites
		(same as currenttime-starttime in other draw2DSprite function)
		\param loop When true animations are looped
	*/
	virtual void draw2DSprite(u32 index, const core::rect<s32> &destRect,
			const core::rect<s32> *clip = 0,
			const video::SColor *const colors = 0,
			u32 timeTicks = 0,
			bool loop = true) = 0;

	//! Draws a sprite batch in 2d using an array of positions and a color
	virtual void draw2DSpriteBatch(const core::array<u32> &indices, const core::array<core::position2di> &pos,
			const core::rect<s32> *clip = 0,
			const video::SColor &color = video::SColor(255, 255, 255, 255),
			u32 starttime = 0, u32 currenttime = 0,
			bool loop = true, bool center = false) = 0;
};

} // end namespace gui
