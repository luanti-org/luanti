// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "texturesource.h"

#include <IVideoDriver.h>
#include "guiscalingfilter.h"
#include "imagefilters.h"
#include "imagesource.h"
#include "renderingengine.h"
#include "settings.h"
#include "texturepaths.h"
#include "util/thread.h"


// Stores internal information about a texture.
struct TextureInfo
{
	std::string name;
	video::ITexture *texture = nullptr;

	// Stores source image names which ImageSource::generateImage used.
	std::set<std::string> sourceImages{};
};

// Stores internal information about a texture image.
struct ImageInfo
{
	video::IImage *image = nullptr;
	std::set<std::string> sourceImages;
};

// TextureSource
class TextureSource final : public IWritableTextureSource
{
public:
	TextureSource();
	virtual ~TextureSource();

	/*
		Example case:
		Now, assume a texture with the id 1 exists, and has the name
		"stone.png^mineral1".
		Then a random thread calls getTextureId for a texture called
		"stone.png^mineral1^crack0".
		...Now, WTF should happen? Well:
		- getTextureId strips off stuff recursively from the end until
		  the remaining part is found, or nothing is left when
		  something is stripped out

		But it is slow to search for textures by names and modify them
		like that?
		- ContentFeatures is made to contain ids for the basic plain
		  textures
		- Crack textures can be slow by themselves, but the framework
		  must be fast.

		Example case #2:
		- Assume a texture with the id 1 exists, and has the name
		  "stone.png^mineral_coal.png".
		- Now getNodeTile() stumbles upon a node which uses
		  texture id 1, and determines that MATERIAL_FLAG_CRACK
		  must be applied to the tile
		- MapBlockMesh::animate() finds the MATERIAL_FLAG_CRACK and
		  has received the current crack level 0 from the client. It
		  finds out the name of the texture with getTextureName(1),
		  appends "^crack0" to it and gets a new texture id with
		  getTextureId("stone.png^mineral_coal.png^crack0").

	*/

	/*
		Gets a texture id from cache or
		- if main thread, generates the texture, adds to cache and returns id.
		- if other thread, adds to request queue and waits for main thread.

		The id 0 points to a NULL texture. It is returned in case of error.
	*/
	u32 getTextureId(const std::string &name);

	// Finds out the name of a cached texture.
	std::string getTextureName(u32 id);

	/*
		If texture specified by the name pointed by the id doesn't
		exist, create it, then return the cached texture.

		Can be called from any thread. If called from some other thread
		and not found in cache, the call is queued to the main thread
		for processing.
	*/
	video::ITexture* getTexture(u32 id);

	video::ITexture* getTexture(const std::string &name, u32 *id = NULL);

	/*
		Get a texture specifically intended for mesh
		application, i.e. not HUD, compositing, or other 2D
		use.  This texture may be a different size and may
		have had additional filters applied.
	*/
	video::ITexture* getTextureForMesh(const std::string &name, u32 *id);

	virtual Palette* getPalette(const std::string &name);

	bool isKnownSourceImage(const std::string &name)
	{
		bool is_known = false;
		bool cache_found = m_source_image_existence.get(name, &is_known);
		if (cache_found)
			return is_known;
		// Not found in cache; find out if a local file exists
		is_known = (!getTexturePath(name).empty());
		m_source_image_existence.set(name, is_known);
		return is_known;
	}

	// Processes queued texture requests from other threads.
	// Shall be called from the main thread.
	void processQueue();

	// Insert a source image into the cache without touching the filesystem.
	// Shall be called from the main thread.
	void insertSourceImage(const std::string &name, video::IImage *img);

	// Rebuild images and textures from the current set of source images
	// Shall be called from the main thread.
	void rebuildImagesAndTextures();

	video::SColor getTextureAverageColor(const std::string &name);

	void setImageCaching(bool enabled);

private:
	// Gets or generates an image for a texture string
	// Caller needs to drop the returned image
	video::IImage *getOrGenerateImage(const std::string &name,
		std::set<std::string> &source_image_names);

	// The id of the thread that is allowed to use irrlicht directly
	std::thread::id m_main_thread;

	// Generates and caches source images
	// This should be only accessed from the main thread
	ImageSource m_imagesource;

	// Is the image cache enabled?
	bool m_image_cache_enabled = false;
	// Caches finished texture images before they are uploaded to the GPU
	// (main thread use only)
	std::unordered_map<std::string, ImageInfo> m_image_cache;

	// Rebuild images and textures from the current set of source images
	// Shall be called from the main thread.
	// You ARE expected to be holding m_textureinfo_cache_mutex
	void rebuildTexture(video::IVideoDriver *driver, TextureInfo &ti);

	// Generate a texture
	u32 generateTexture(const std::string &name);

	// Thread-safe cache of what source images are known (true = known)
	MutexedMap<std::string, bool> m_source_image_existence;

	// A texture id is index in this array.
	// The first position contains a NULL texture.
	std::vector<TextureInfo> m_textureinfo_cache;
	// Maps a texture name to an index in the former.
	std::unordered_map<std::string, u32> m_name_to_id;
	// The two former containers are behind this mutex
	std::mutex m_textureinfo_cache_mutex;

	// Queued texture fetches (to be processed by the main thread)
	RequestQueue<std::string, u32, std::thread::id, u8> m_get_texture_queue;

	// Textures that have been overwritten with other ones
	// but can't be deleted because the ITexture* might still be used
	std::vector<video::ITexture*> m_texture_trash;

	// Maps image file names to loaded palettes.
	std::unordered_map<std::string, Palette> m_palettes;

	// Cached from settings for making textures from meshes
	bool mesh_filter_needed;
};

IWritableTextureSource *createTextureSource()
{
	return new TextureSource();
}

TextureSource::TextureSource()
{
	m_main_thread = std::this_thread::get_id();

	// Add a NULL TextureInfo as the first index, named ""
	m_textureinfo_cache.emplace_back(TextureInfo{""});
	m_name_to_id[""] = 0;

	// Cache some settings
	// Note: Since this is only done once, the game must be restarted
	// for these settings to take effect.
	mesh_filter_needed =
			g_settings->getBool("mip_map") ||
			g_settings->getBool("trilinear_filter") ||
			g_settings->getBool("bilinear_filter") ||
			g_settings->getBool("anisotropic_filter");
}

TextureSource::~TextureSource()
{
	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	u32 textures_before = driver->getTextureCount();

	for (const auto &it : m_image_cache) {
		assert(it.second.image);
		it.second.image->drop();
	}

	for (const auto &iter : m_textureinfo_cache) {
		if (iter.texture)
			driver->removeTexture(iter.texture);
	}
	m_textureinfo_cache.clear();

	for (auto t : m_texture_trash) {
		driver->removeTexture(t);
	}

	infostream << "~TextureSource() before cleanup: " << textures_before
			<< " after: " << driver->getTextureCount() << std::endl;
}

video::IImage *TextureSource::getOrGenerateImage(const std::string &name,
		std::set<std::string> &source_image_names)
{
	auto it = m_image_cache.find(name);
	if (it != m_image_cache.end()) {
		source_image_names = it->second.sourceImages;
		it->second.image->grab();
		return it->second.image;
	}

	std::set<std::string> tmp;
	auto *img = m_imagesource.generateImage(name, tmp);
	if (img && m_image_cache_enabled) {
		img->grab();
		m_image_cache[name] = {img, tmp};
	}
	source_image_names = std::move(tmp);
	return img;
}

u32 TextureSource::getTextureId(const std::string &name)
{
	{ // See if texture already exists
		MutexAutoLock lock(m_textureinfo_cache_mutex);
		auto n = m_name_to_id.find(name);
		if (n != m_name_to_id.end())
			return n->second;
	}

	// Get texture
	if (std::this_thread::get_id() == m_main_thread) {
		return generateTexture(name);
	}


	infostream << "getTextureId(): Queued: name=\"" << name << "\"" << std::endl;

	// We're gonna ask the result to be put into here
	static thread_local ResultQueue<std::string, u32, std::thread::id, u8> result_queue;

	// Throw a request in
	m_get_texture_queue.add(name, std::this_thread::get_id(), 0, &result_queue);

	try {
		while(true) {
			// Wait for result for up to 1 seconds (empirical value)
			GetResult<std::string, u32, std::thread::id, u8>
				result = result_queue.pop_front(1000);

			if (result.key == name) {
				return result.item;
			}
		}
	} catch(ItemNotFoundException &e) {
		errorstream << "Waiting for texture " << name << " timed out." << std::endl;
		return 0;
	}

	infostream << "getTextureId(): Failed" << std::endl;

	return 0;
}

// This method generates all the textures
u32 TextureSource::generateTexture(const std::string &name)
{
	// Empty name means texture 0
	if (name.empty()) {
		infostream << "generateTexture(): name is empty" << std::endl;
		return 0;
	}

	{ // See if texture already exists
		MutexAutoLock lock(m_textureinfo_cache_mutex);
		auto n = m_name_to_id.find(name);
		if (n != m_name_to_id.end())
			return n->second;
	}

	// Calling only allowed from main thread
	if (std::this_thread::get_id() != m_main_thread) {
		errorstream << "TextureSource::generateTexture() "
				"called not from main thread" << std::endl;
		return 0;
	}

	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	sanity_check(driver);

	// passed into texture info for dynamic media tracking
	std::set<std::string> source_image_names;
	video::IImage *img = getOrGenerateImage(name, source_image_names);

	video::ITexture *tex = nullptr;

	if (img) {
		// Create texture from resulting image
		tex = driver->addTexture(name.c_str(), img);
		guiScalingCache(io::path(name.c_str()), driver, img);
		img->drop();
	}

	// Add texture to caches (add NULL textures too)

	MutexAutoLock lock(m_textureinfo_cache_mutex);

	u32 id = m_textureinfo_cache.size();
	TextureInfo ti{name, tex, std::move(source_image_names)};
	m_textureinfo_cache.emplace_back(std::move(ti));
	m_name_to_id[name] = id;

	return id;
}

std::string TextureSource::getTextureName(u32 id)
{
	MutexAutoLock lock(m_textureinfo_cache_mutex);

	if (id >= m_textureinfo_cache.size()) {
		errorstream << "TextureSource::getTextureName(): id=" << id
				<< " >= m_textureinfo_cache.size()=" << m_textureinfo_cache.size()
				<< std::endl;
		return "";
	}

	return m_textureinfo_cache[id].name;
}

video::ITexture* TextureSource::getTexture(u32 id)
{
	MutexAutoLock lock(m_textureinfo_cache_mutex);

	if (id >= m_textureinfo_cache.size())
		return nullptr;

	return m_textureinfo_cache[id].texture;
}

video::ITexture* TextureSource::getTexture(const std::string &name, u32 *id)
{
	u32 actual_id = getTextureId(name);
	if (id)
		*id = actual_id;

	return getTexture(actual_id);
}

video::ITexture* TextureSource::getTextureForMesh(const std::string &name, u32 *id)
{
	// Avoid duplicating texture if it won't actually change
	if (mesh_filter_needed && !name.empty())
		return getTexture(name + "^[applyfiltersformesh", id);
	return getTexture(name, id);
}

Palette* TextureSource::getPalette(const std::string &name)
{
	// Only the main thread may load images
	sanity_check(std::this_thread::get_id() == m_main_thread);

	if (name.empty())
		return nullptr;

	auto it = m_palettes.find(name);
	if (it == m_palettes.end()) {
		// Create palette
		std::set<std::string> source_image_names; // unused, sadly.
		video::IImage *img = getOrGenerateImage(name, source_image_names);
		if (!img) {
			warningstream << "TextureSource::getPalette(): palette \"" << name
				<< "\" could not be loaded." << std::endl;
			return nullptr;
		}
		Palette new_palette;
		u32 w = img->getDimension().Width;
		u32 h = img->getDimension().Height;
		// Real area of the image
		u32 area = h * w;
		if (area == 0)
			return nullptr;
		if (area > 256) {
			warningstream << "TextureSource::getPalette(): the specified"
				<< " palette image \"" << name << "\" is larger than 256"
				<< " pixels, using the first 256." << std::endl;
			area = 256;
		} else if (256 % area != 0)
			warningstream << "TextureSource::getPalette(): the "
				<< "specified palette image \"" << name << "\" does not "
				<< "contain power of two pixels." << std::endl;
		// We stretch the palette so it will fit 256 values
		// This many param2 values will have the same color
		u32 step = 256 / area;
		// For each pixel in the image
		for (u32 i = 0; i < area; i++) {
			video::SColor c = img->getPixel(i % w, i / w);
			// Fill in palette with 'step' colors
			for (u32 j = 0; j < step; j++)
				new_palette.push_back(c);
		}
		img->drop();
		// Fill in remaining elements
		while (new_palette.size() < 256)
			new_palette.emplace_back(0xFFFFFFFF);
		m_palettes[name] = new_palette;
		it = m_palettes.find(name);
	}
	if (it != m_palettes.end())
		return &((*it).second);
	return nullptr;
}

void TextureSource::processQueue()
{
	// Fetch textures
	// NOTE: process outstanding requests from all mesh generation threads
	while (!m_get_texture_queue.empty()) {
		GetRequest<std::string, u32, std::thread::id, u8>
				request = m_get_texture_queue.pop();

		m_get_texture_queue.pushResult(request, generateTexture(request.key));
	}
}

void TextureSource::insertSourceImage(const std::string &name, video::IImage *img)
{
	sanity_check(std::this_thread::get_id() == m_main_thread);

	m_imagesource.insertSourceImage(name, img, true);
	m_source_image_existence.set(name, true);

	// now we need to check for any textures that need updating
	MutexAutoLock lock(m_textureinfo_cache_mutex);

	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	sanity_check(driver);

	// Recreate affected textures
	u32 affected = 0;
	for (TextureInfo &ti : m_textureinfo_cache) {
		if (ti.name.empty())
			continue; // Skip dummy entry
		// If the source image was used, we need to rebuild this texture
		if (ti.sourceImages.find(name) != ti.sourceImages.end()) {
			rebuildTexture(driver, ti);
			affected++;
		}
	}
	if (affected > 0)
		verbosestream << "TextureSource: inserting \"" << name << "\" caused rebuild of "
				<< affected << " textures." << std::endl;
}

void TextureSource::rebuildImagesAndTextures()
{
	MutexAutoLock lock(m_textureinfo_cache_mutex);

	/*
	 * Note: While it may become useful in the future, it's not clear what the
	 * current purpose of this function is. The client loads all media into a
	 * freshly created texture source, so the only two textures that will ever be
	 * rebuilt are 'progress_bar.png' and 'progress_bar_bg.png'.
	 */

	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	sanity_check(driver);

	infostream << "TextureSource: recreating " << m_textureinfo_cache.size()
			<< " textures" << std::endl;

	assert(!m_image_cache_enabled || m_image_cache.empty());

	// Recreate textures
	for (TextureInfo &ti : m_textureinfo_cache) {
		if (ti.name.empty())
			continue; // Skip dummy entry
		rebuildTexture(driver, ti);
	}

	// FIXME: we should rebuild palettes too
}

void TextureSource::rebuildTexture(video::IVideoDriver *driver, TextureInfo &ti)
{
	assert(!ti.name.empty());
	sanity_check(std::this_thread::get_id() == m_main_thread);

	std::set<std::string> source_image_names;
	video::IImage *img = getOrGenerateImage(ti.name, source_image_names);

	// Create texture from resulting image
	video::ITexture *t = nullptr, *t_old = ti.texture;
	if (!img) {
		// new texture becomes null
	} else if (t_old && t_old->getColorFormat() == img->getColorFormat() && t_old->getSize() == img->getDimension()) {
		// can replace texture in-place
		std::swap(t, t_old);
		void *ptr = t->lock(video::ETLM_WRITE_ONLY);
		if (ptr) {
			memcpy(ptr, img->getData(), img->getImageDataSizeInBytes());
			t->unlock();
			t->regenerateMipMapLevels();
		} else {
			warningstream << "TextureSource::rebuildTexture(): lock failed for \""
				<< ti.name << "\"" << std::endl;
		}
	} else {
		// create new one
		t = driver->addTexture(ti.name.c_str(), img);
	}
	if (img)
		guiScalingCache(io::path(ti.name.c_str()), driver, img);

	// Replace texture info
	if (img)
		img->drop();
	ti.texture = t;
	ti.sourceImages = std::move(source_image_names);
	if (t_old)
		m_texture_trash.push_back(t_old);
}

video::SColor TextureSource::getTextureAverageColor(const std::string &name)
{
	assert(std::this_thread::get_id() == m_main_thread);

	std::set<std::string> unused;
	auto *image = getOrGenerateImage(name, unused);
	if (!image)
		return {0, 0, 0, 0};

	video::SColor c = imageAverageColor(image);
	image->drop();

	return c;
}

void TextureSource::setImageCaching(bool enabled)
{
	m_image_cache_enabled = enabled;
	if (!enabled) {
		for (const auto &it : m_image_cache) {
			assert(it.second.image);
			it.second.image->drop();
		}
		m_image_cache.clear();
	}
}
