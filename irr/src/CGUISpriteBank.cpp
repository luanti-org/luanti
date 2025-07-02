// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUISpriteBank.h"

#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "ITexture.h"

namespace gui
{

CGUISpriteBank::CGUISpriteBank(IGUIEnvironment *env) :
		Environment(env), Driver(0)
{
	if (Environment) {
		Driver = Environment->getVideoDriver();
		if (Driver)
			Driver->grab();
	}
}

CGUISpriteBank::~CGUISpriteBank()
{
	clear();

	// drop video driver
	if (Driver)
		Driver->drop();
}

core::array<core::rect<s32>> &CGUISpriteBank::getPositions()
{
	return Rectangles;
}

core::array<SGUISprite> &CGUISpriteBank::getSprites()
{
	return Sprites;
}

u32 CGUISpriteBank::getTextureCount() const
{
	return Textures.size();
}

video::ITexture *CGUISpriteBank::getTexture(u32 index) const
{
	if (index < Textures.size())
		return Textures[index];
	else
		return 0;
}

void CGUISpriteBank::addTexture(video::ITexture *texture)
{
	if (texture)
		texture->grab();

	Textures.push_back(texture);
}

void CGUISpriteBank::setTexture(u32 index, video::ITexture *texture)
{
	while (index >= Textures.size())
		Textures.push_back(0);

	if (texture)
		texture->grab();

	if (Textures[index])
		Textures[index]->drop();

	Textures[index] = texture;
}

//! clear everything
void CGUISpriteBank::clear()
{
	// drop textures
	for (u32 i = 0; i < Textures.size(); ++i) {
		if (Textures[i])
			Textures[i]->drop();
	}
	Textures.clear();
	Sprites.clear();
	Rectangles.clear();
}

//! Add the texture and use it for a single non-animated sprite.
s32 CGUISpriteBank::addTextureAsSprite(video::ITexture *texture)
{
	if (!texture)
		return -1;

	addTexture(texture);
	u32 textureIndex = getTextureCount() - 1;

	u32 rectangleIndex = Rectangles.size();
	Rectangles.push_back(core::rect<s32>(0, 0, texture->getOriginalSize().Width, texture->getOriginalSize().Height));

	SGUISprite sprite;
	sprite.frameTime = 0;

	SGUISpriteFrame frame;
	frame.textureNumber = textureIndex;
	frame.rectNumber = rectangleIndex;
	sprite.Frames.push_back(frame);

	Sprites.push_back(sprite);

	return Sprites.size() - 1;
}

// get FrameNr for time. return true on exisiting frame
inline bool CGUISpriteBank::getFrameNr(u32 &frame, u32 index, u32 time, bool loop) const
{
	frame = 0;
	if (index >= Sprites.size())
		return false;

	const SGUISprite &sprite = Sprites[index];
	const u32 frameSize = sprite.Frames.size();
	if (frameSize < 1)
		return false;

	if (sprite.frameTime) {
		u32 f = (time / sprite.frameTime);
		if (loop)
			frame = f % frameSize;
		else
			frame = (f >= frameSize) ? frameSize - 1 : f;
	}
	return true;
}

//! draws a sprite in 2d with scale and color
void CGUISpriteBank::draw2DSprite(u32 index, const core::position2di &pos,
		const core::rect<s32> *clip, const video::SColor &color,
		u32 starttime, u32 currenttime, bool loop, bool center)
{
	u32 frame = 0;
	if (!getFrameNr(frame, index, currenttime - starttime, loop))
		return;

	const video::ITexture *tex = getTexture(Sprites[index].Frames[frame].textureNumber);
	if (!tex)
		return;

	const u32 rn = Sprites[index].Frames[frame].rectNumber;
	if (rn >= Rectangles.size())
		return;

	const core::rect<s32> &r = Rectangles[rn];
	core::position2di p(pos);
	if (center) {
		p -= r.getSize() / 2;
	}
	Driver->draw2DImage(tex, p, r, clip, color, true);
}

void CGUISpriteBank::draw2DSprite(u32 index, const core::rect<s32> &destRect,
		const core::rect<s32> *clip, const video::SColor *const colors,
		u32 timeTicks, bool loop)
{
	u32 frame = 0;
	if (!getFrameNr(frame, index, timeTicks, loop))
		return;

	const video::ITexture *tex = getTexture(Sprites[index].Frames[frame].textureNumber);
	if (!tex)
		return;

	const u32 rn = Sprites[index].Frames[frame].rectNumber;
	if (rn >= Rectangles.size())
		return;

	Driver->draw2DImage(tex, destRect, Rectangles[rn], clip, colors, true);
}

void CGUISpriteBank::draw2DSpriteBatch(const core::array<u32> &indices,
		const core::array<core::position2di> &pos,
		const core::rect<s32> *clip,
		const video::SColor &color,
		u32 starttime, u32 currenttime,
		bool loop, bool center)
{
	const u32 drawCount = core::min_<u32>(indices.size(), pos.size());

	if (!getTextureCount())
		return;
	core::array<SDrawBatch> drawBatches(getTextureCount());
	for (u32 i = 0; i < Textures.size(); ++i) {
		drawBatches.push_back(SDrawBatch());
		drawBatches[i].positions.reallocate(drawCount);
		drawBatches[i].sourceRects.reallocate(drawCount);
	}

	for (u32 i = 0; i < drawCount; ++i) {
		const u32 index = indices[i];

		// work out frame number
		u32 frame = 0;
		if (!getFrameNr(frame, index, currenttime - starttime, loop))
			return;

		const u32 texNum = Sprites[index].Frames[frame].textureNumber;
		if (texNum >= drawBatches.size()) {
			continue;
		}
		SDrawBatch &currentBatch = drawBatches[texNum];

		const u32 rn = Sprites[index].Frames[frame].rectNumber;
		if (rn >= Rectangles.size())
			return;

		const core::rect<s32> &r = Rectangles[rn];

		if (center) {
			core::position2di p = pos[i];
			p -= r.getSize() / 2;

			currentBatch.positions.push_back(p);
			currentBatch.sourceRects.push_back(r);
		} else {
			currentBatch.positions.push_back(pos[i]);
			currentBatch.sourceRects.push_back(r);
		}
	}

	for (u32 i = 0; i < drawBatches.size(); i++) {
		if (!drawBatches[i].positions.empty() && !drawBatches[i].sourceRects.empty())
			Driver->draw2DImageBatch(getTexture(i), drawBatches[i].positions,
					drawBatches[i].sourceRects, clip, color, true);
	}
}

} // namespace gui
