// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIFontBitmap.h"
#include "irrString.h"
#include "IReadFile.h"
#include "irrArray.h"
#include <map>

namespace video
{
class IVideoDriver;
class IImage;
}

namespace gui
{

class IGUIEnvironment;

class CGUIFont : public IGUIFontBitmap
{
public:
	//! constructor
	CGUIFont(IGUIEnvironment *env, const io::path &filename);

	//! destructor
	virtual ~CGUIFont();

	//! loads a font from a texture file
	bool load(const io::path &filename);

	//! loads a font from a texture file
	bool load(io::IReadFile *file);

	//! draws an text and clips it to the specified rectangle if wanted
	virtual void draw(const core::stringw &text, const core::rect<s32> &position,
			video::SColor color, bool hcenter = false,
			bool vcenter = false, const core::rect<s32> *clip = 0) override;

	//! returns the dimension of a text
	core::dimension2d<u32> getDimension(const wchar_t *text) const override;

	//! Calculates the index of the character in the text which is on a specific position.
	s32 getCharacterFromPos(const wchar_t *text, s32 pixel_x) const override;

	//! Returns the type of this font
	EGUI_FONT_TYPE getType() const override { return EGFT_BITMAP; }

	//! set an Pixel Offset on Drawing ( scale position on width )
	void setKerningWidth(s32 kerning) override;
	void setKerningHeight(s32 kerning) override;

	//! set an Pixel Offset on Drawing ( scale position on width )
	core::vector2di getKerning(const wchar_t thisLetter, const wchar_t previousLetter) const override;

	//! gets the sprite bank
	IGUISpriteBank *getSpriteBank() const override;

	//! returns the sprite number from a given character
	u32 getSpriteNoFromChar(const wchar_t *c) const override;

	void setInvisibleCharacters(const wchar_t *s) override;

private:
	struct SFontArea
	{
		SFontArea() :
				underhang(0), overhang(0), width(0), spriteno(0) {}
		s32 underhang;
		s32 overhang;
		s32 width;
		u32 spriteno;
	};

	//! load & prepare font from ITexture
	bool loadTexture(video::IImage *image, const io::path &name);

	void readPositions(video::IImage *texture, s32 &lowerRightPositions);

	s32 getAreaFromCharacter(const wchar_t c) const;
	void setMaxHeight();

	void pushTextureCreationFlags(bool (&flags)[3]);
	void popTextureCreationFlags(const bool (&flags)[3]);

	core::array<SFontArea> Areas;
	std::map<wchar_t, s32> CharacterMap;
	video::IVideoDriver *Driver;
	IGUISpriteBank *SpriteBank;
	IGUIEnvironment *Environment;
	u32 WrongCharacter;
	s32 MaxHeight;
	s32 GlobalKerningWidth, GlobalKerningHeight;

	core::stringw Invisible;
};

} // end namespace gui
