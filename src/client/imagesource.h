// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <IImage.h>
#include <map>
#include <set>
#include <string>

using namespace irr;

// This file is only used for internal generation of images.
// Use texturesource.h to handle textures.

// A cache used for storing source images.
// (A "source image" is an unmodified image directly taken from the filesystem.)
// Does not contain modified images.
class SourceImageCache {
public:
	~SourceImageCache();

	void insert(const std::string &name, video::IImage *img, bool prefer_local);

	video::IImage* get(const std::string &name);

	// Primarily fetches from cache, secondarily tries to read from filesystem.
	video::IImage *getOrLoad(const std::string &name);
private:
	std::map<std::string, video::IImage*> m_images;
};

// Generates images using texture modifiers, and caches source images.
struct ImageSource {
	ImageSource();

	/*! Generates an image from a full string like
	 * "stone.png^mineral_coal.png^[crack:1:0".
	 * The returned Image should be dropped.
	 * source_image_names is important to determine when to flush the image from a cache (dynamic media)
	 */
	video::IImage* generateImage(std::string_view name, std::set<std::string> &source_image_names);

	// Insert a source image into the cache without touching the filesystem.
	void insertSourceImage(const std::string &name, video::IImage *img, bool prefer_local);

	// TODO should probably be moved elsewhere
	static video::SColor getImageAverageColor(const video::IImage &image);

private:

	// Generate image based on a string like "stone.png" or "[crack:1:0".
	// If baseimg is NULL, it is created. Otherwise stuff is made on it.
	// source_image_names is important to determine when to flush the image from a cache (dynamic media).
	bool generateImagePart(std::string_view part_of_name, video::IImage *& baseimg,
			std::set<std::string> &source_image_names);

	// Cached settings needed for making textures from meshes
	bool m_setting_mipmap;
	bool m_setting_trilinear_filter;
	bool m_setting_bilinear_filter;
	bool m_setting_anisotropic_filter;

	// Cache of source images
	SourceImageCache m_sourcecache;
};
