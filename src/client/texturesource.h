// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include <SColor.h>
#include <dimension2d.h>
#include <string>
#include <vector>

namespace video
{
	class IImage;
	class ITexture;
}

typedef std::vector<video::SColor> Palette;

/*
	TextureSource creates and caches textures, which are created from images.

	Terminology:
	texture string = e.g. "dirt.png^grass_side.png"
	texture name = can be the same as the texture string
	               or something abstract like "<texture12>"
	texture ID = unique numeric identifier for a texture
	standard texture = refers to a normal 2D texture as you would expect.
	                   depending on the support, 2D array textures can exist too.
*/

class ISimpleTextureSource
{
public:
	ISimpleTextureSource() = default;
	virtual ~ISimpleTextureSource() = default;

	/// @brief Generates a texture string into a standard texture
	virtual video::ITexture *getTexture(
			const std::string &name, u32 *id = nullptr) = 0;
};

class ITextureSource : public ISimpleTextureSource
{
public:
	ITextureSource() = default;
	virtual ~ITextureSource() = default;

	using ISimpleTextureSource::getTexture;

	/// @brief Generates a texture string into a standard texture
	/// @return its ID
	virtual u32 getTextureId(const std::string &image)=0;

	/// @brief Returns name of existing texture by ID
	/// @warning Use sparingly. Mostly useful for debugging.
	virtual std::string getTextureName(u32 id)=0;

	/// @brief Returns existing texture by ID
	virtual video::ITexture *getTexture(u32 id)=0;

	/// @brief Generates texture string(s) into an array texture
	/// @note Unlike the other getters this will always add a *new* texture.
	/// @return its ID
	virtual video::ITexture *addArrayTexture(
		const std::vector<std::string> &images, u32 *id = nullptr) = 0;

	/**
	 * @brief Generates a texture string into a standard texture
	 * Filters will be applied to make the texture suitable for mipmapping and
	 * linear filtering during rendering.
	 * @return its ID
	 */
	inline video::ITexture *getTextureForMesh(
			const std::string &image, u32 *id = nullptr)
	{
		if (needFilterForMesh() && !image.empty())
			return getTexture(image + FILTER_FOR_MESH, id);
		return getTexture(image, id);
	}

	/// @return true if getTextureForMesh will apply a filter
	virtual bool needFilterForMesh() const = 0;

	/// Filter needed for mesh-suitable textures, including leading ^
	static constexpr const char *FILTER_FOR_MESH = "^[applyfiltersformesh";

	/**
	 * Returns a palette from the given texture string.
	 * The pointer is valid until the texture source is
	 * destructed.
	 * Must be called from the main thread.
	 */
	virtual Palette *getPalette(const std::string &image) = 0;

	/// @brief Check if given image name exists
	virtual bool isKnownSourceImage(const std::string &name)=0;

	/// @brief Return dimensions of a texture string
	/// (will avoid actually creating the texture)
	virtual core::dimension2du getTextureDimensions(const std::string &image)=0;

	/// @brief Return average color of a texture string
	virtual video::SColor getTextureAverageColor(const std::string &image)=0;

	// Note: this method is here because caching is the decision of the
	// API user, even if his access is read-only.

	/**
	 * Enables or disables the caching of finished texture images.
	 * This can be useful if you want to call getTextureAverageColor without
	 * duplicating work.
	 * @note Disabling caching will flush the cache.
	 */
	virtual void setImageCaching(bool enabled) {};
};

class IWritableTextureSource : public ITextureSource
{
public:
	IWritableTextureSource() = default;
	virtual ~IWritableTextureSource() = default;

	/// @brief Fulfil texture requests from other threads
	virtual void processQueue()=0;

	/**
	 * @brief Inserts a source image. Must be called from the main thread.
	 * Takes ownership of @p img
	 */
	virtual void insertSourceImage(const std::string &name, video::IImage *img)=0;

	/**
	 * Rebuilds all textures (in case-source images have changed)
	 * @note This won't invalidate old ITexture's, but may or may not reuse them.
	 * So you have to re-get all textures anyway.
	 */
	virtual void rebuildImagesAndTextures()=0;
};

IWritableTextureSource *createTextureSource();
