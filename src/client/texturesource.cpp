// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "texturesource.h"

#include <cassert>
#include <IVideoDriver.h>
#include "guiscalingfilter.h"
#include "imagefilters.h"
#include "imagesource.h"
#include "porting.h"
#include "renderingengine.h"
#include "settings.h"
#include "texturepaths.h"
#include "util/thread.h"

// Represents a to-be-generated texture for queuing purposes
struct TextureRequest
{
	video::E_TEXTURE_TYPE type = video::ETT_2D;
	std::vector<std::string> images;

	void print(std::ostream &to) const {
		if (images.size() == 1) {
			to << "image=\"" << images[0] << "\"";
		} else {
			to << "images={";
			for (auto &image : images)
				to << "\"" << image << "\" ";
			to << "}";
		}
	}

	bool operator==(const TextureRequest &other) const {
		return type == other.type && images == other.images;
	}
	bool operator!=(const TextureRequest &other) const {
		return !(*this == other);
	}
};

// Stores internal information about a texture.
struct TextureInfo
{
	// Type the texture should have (when created)
	video::E_TEXTURE_TYPE type = video::ETT_2D;

	// Name of the texture
	// For standard textures this is equivalent to images[0]
	std::string name;

	// Name of the images that comprise this texture
	// (multiple for array textures)
	std::vector<std::string> images;

	video::ITexture *texture = nullptr;

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

	u32 getTextureId(const std::string &name);

	std::string getTextureName(u32 id);

	video::ITexture* getTexture(u32 id);

	video::ITexture* getTexture(const std::string &name, u32 *id = nullptr);

	video::ITexture *addArrayTexture(
		const std::vector<std::string> &images, u32 *id = nullptr);

	bool needFilterForMesh() const {
		return mesh_filter_needed;
	}

	Palette *getPalette(const std::string &name);

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

	core::dimension2du getTextureDimensions(const std::string &image);

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

	// Rebuild a single texture
	void rebuildTexture(video::IVideoDriver *driver, TextureInfo &ti);

	// Process texture request
	u32 processRequestQueued(const TextureRequest &req);

	// Process texture request directly (main thread only)
	u32 processRequest(const TextureRequest &req);

	// Generate standard texture
	u32 generateTexture(const std::string &name);

	// Generate array texture
	u32 generateArrayTexture(const std::vector<std::string> &names);

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
	RequestQueue<TextureRequest, u32, std::thread::id, char> m_get_texture_queue;

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
	m_textureinfo_cache.emplace_back(TextureInfo{video::ETT_2D, "", {}});
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
		std::set copy(it->second.sourceImages);
		source_image_names.merge(copy);
		it->second.image->grab();
		return it->second.image;
	}

	std::set<std::string> tmp;
	auto *img = m_imagesource.generateImage(name, tmp);
	if (img && m_image_cache_enabled) {
		img->grab();
		m_image_cache[name] = {img, tmp};
	}
	source_image_names.merge(tmp);
	return img;
}

u32 TextureSource::processRequestQueued(const TextureRequest &req)
{
	if (std::this_thread::get_id() == m_main_thread) {
		// Generate directly
		return processRequest(req);
	}

	infostream << "TextureSource: Queued: ";
	req.print(infostream);
	infostream << std::endl;

	// We're gonna ask the result to be put into here
	static thread_local decltype(m_get_texture_queue)::result_queue_type result_queue;

	// Throw a request in
	m_get_texture_queue.add(req, std::this_thread::get_id(), 0, &result_queue);

	try {
		// Wait for result for up to 1 seconds (empirical value)
		auto result = result_queue.pop_front(1000);

		assert(result.key == req);
		return result.item;
	} catch (ItemNotFoundException &e) {
		errorstream << "TextureSource: Waiting for texture ";
		req.print(infostream);
		infostream << " timed out." << std::endl;
		return 0;
	}
}

u32 TextureSource::getTextureId(const std::string &name)
{
	{ // See if texture already exists
		MutexAutoLock lock(m_textureinfo_cache_mutex);
		auto n = m_name_to_id.find(name);
		if (n != m_name_to_id.end())
			return n->second;
	}

	TextureRequest req{video::ETT_2D, {name}};

	return processRequestQueued(req);
}

video::ITexture *TextureSource::addArrayTexture(
	const std::vector<std::string> &images, u32 *ret_id)
{
	if (images.empty())
		return NULL;

	TextureRequest req{video::ETT_2D_ARRAY, images};

	u32 id = processRequestQueued(req);
	if (ret_id)
		*ret_id = id;
	return getTexture(id);
}

u32 TextureSource::processRequest(const TextureRequest &req)
{
	if (req.type == video::ETT_2D) {
		assert(req.images.size() == 1);
		return generateTexture(req.images[0]);
	}

	if (req.type == video::ETT_2D_ARRAY) {
		assert(!req.images.empty());
		return generateArrayTexture(req.images);
	}

	errorstream << "TextureSource::processRequest(): unknown type "
			<< (int)req.type << std::endl;
	return 0;
}

u32 TextureSource::generateArrayTexture(const std::vector<std::string> &images)
{
	std::set<std::string> source_image_names;
	std::vector<video::IImage*> imgs;
	const auto &drop_imgs = [&imgs] () {
		for (auto *img : imgs) {
			if (img)
				img->drop();
		}
		imgs.clear();
	};
	for (auto &name : images) {
		video::IImage *img = getOrGenerateImage(name, source_image_names);
		if (!img) {
			// Since the caller needs to make sure of the dimensions beforehand
			// anyway, this should not ever happen. So the "unhelpful" error is ok.
			errorstream << "generateArrayTexture(): one of " << images.size()
				<< " images failed to generate, aborting." << std::endl;
			drop_imgs();
			return 0;
		}
		imgs.push_back(img);
	}
	assert(!imgs.empty());

	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	sanity_check(driver);
	assert(driver->queryFeature(video::EVDF_TEXTURE_2D_ARRAY));

	MutexAutoLock lock(m_textureinfo_cache_mutex);
	const u32 id = m_textureinfo_cache.size();
	std::string name;
	{ // automatically choose a name
		char buf[64];
		porting::mt_snprintf(buf, sizeof(buf), "array#%u %ux%ux%u", id,
			imgs[0]->getDimension().Width, imgs[0]->getDimension().Height,
			imgs.size());
		name = buf;
	}

	video::ITexture *tex = driver->addArrayTexture(name, imgs.data(), imgs.size());
	drop_imgs();

	if (!tex) {
		warningstream << "generateArrayTexture(): failed to upload texture \""
				<< name << "\"" << std::endl;
	}

	// Add texture to caches (add NULL textures too)

	TextureInfo ti{video::ETT_2D_ARRAY, name, images, tex, std::move(source_image_names)};
	m_textureinfo_cache.emplace_back(std::move(ti));
	m_name_to_id[name] = id;

	return id;
}

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
	sanity_check(std::this_thread::get_id() == m_main_thread);

	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	sanity_check(driver);

	std::set<std::string> source_image_names;
	video::IImage *img = getOrGenerateImage(name, source_image_names);

	video::ITexture *tex = nullptr;

	if (img) {
		// Create texture from resulting image
		tex = driver->addTexture(name.c_str(), img);
		guiScalingCache(io::path(name.c_str()), driver, img);
		img->drop();
	}
	if (!tex) {
		warningstream << "generateTexture(): failed to upload texture \""
				<< name << "\"" << std::endl;
	}

	// Add texture to caches (add NULL textures too)

	MutexAutoLock lock(m_textureinfo_cache_mutex);

	const u32 id = m_textureinfo_cache.size();
	TextureInfo ti{video::ETT_2D, name, {name}, tex, std::move(source_image_names)};
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
	while (!m_get_texture_queue.empty()) {
		auto request = m_get_texture_queue.pop();

		m_get_texture_queue.pushResult(request, processRequest(request.key));
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

	if (ti.type != video::ETT_2D) {
		// It's unclear how this idea is supposed to work with array textures,
		// since after a rebuild the dimensions of some images can mismatch
		// so that creating an array is no longer possible.
		infostream << "TextureSource::rebuildTexture(): "
			"Refusing to rebuild array texture" << std::endl;
		return;
	}

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
	if (name.empty())
		return {0, 0, 0, 0};

	std::set<std::string> unused;
	auto *image = getOrGenerateImage(name, unused);
	if (!image)
		return {0, 0, 0, 0};

	video::SColor c = imageAverageColor(image);
	image->drop();

	return c;
}

core::dimension2du TextureSource::getTextureDimensions(const std::string &name)
{
	assert(std::this_thread::get_id() == m_main_thread);

	core::dimension2du ret;
	if (!name.empty()) {
		std::set<std::string> unused;
		auto *image = getOrGenerateImage(name, unused);
		if (image) {
			ret = image->getDimension();
			image->drop();
		}
	}

	return ret;
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
