// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include <SColor.h>
#include <string>
#include <vector>

namespace irr::video
{
	class IImage;
	class ITexture;
}

typedef std::vector<video::SColor> Palette;

/*
	TextureSource creates and caches textures.
*/

class ISimpleTextureSource
{
public:
	ISimpleTextureSource() = default;

	virtual ~ISimpleTextureSource() = default;

	virtual video::ITexture* getTexture(
			const std::string &name, u32 *id = nullptr) = 0;
};

class ITextureSource : public ISimpleTextureSource
{
public:
	ITextureSource() = default;

	virtual ~ITextureSource() = default;

	virtual u32 getTextureId(const std::string &name)=0;
	virtual std::string getTextureName(u32 id)=0;
	virtual video::ITexture* getTexture(u32 id)=0;
	virtual video::ITexture* getTexture(
			const std::string &name, u32 *id = nullptr)=0;
	virtual video::ITexture* getTextureForMesh(
			const std::string &name, u32 *id = nullptr) = 0;
	/*!
	 * Returns a palette from the given texture name.
	 * The pointer is valid until the texture source is
	 * destructed.
	 * Should be called from the main thread.
	 */
	virtual Palette* getPalette(const std::string &name) = 0;
	virtual bool isKnownSourceImage(const std::string &name)=0;
	virtual video::ITexture* getNormalTexture(const std::string &name)=0;
	virtual video::SColor getTextureAverageColor(const std::string &name)=0;
};

class IWritableTextureSource : public ITextureSource
{
public:
	IWritableTextureSource() = default;

	virtual ~IWritableTextureSource() = default;

	virtual u32 getTextureId(const std::string &name)=0;
	virtual std::string getTextureName(u32 id)=0;
	virtual video::ITexture* getTexture(u32 id)=0;
	virtual video::ITexture* getTexture(
			const std::string &name, u32 *id = nullptr)=0;
	virtual bool isKnownSourceImage(const std::string &name)=0;

	virtual void processQueue()=0;
	virtual void insertSourceImage(const std::string &name, video::IImage *img)=0;
	virtual void rebuildImagesAndTextures()=0;
	virtual video::ITexture* getNormalTexture(const std::string &name)=0;
	virtual video::SColor getTextureAverageColor(const std::string &name)=0;
};

IWritableTextureSource *createTextureSource();
