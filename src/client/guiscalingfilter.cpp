// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 Aaron Suen <warr1024@gmail.com>

#include "guiscalingfilter.h"
#include "imagefilters.h"
#include "porting.h"
#include "settings.h"
#include "util/numeric.h"
#include <cstdio>
#include "client/renderingengine.h"
#include <IImage.h>
#include <ITexture.h>
#include <IVideoDriver.h>

/* Maintain a static cache to store the images that correspond to textures
 * in a format that's manipulable by code.  Some platforms exhibit issues
 * converting textures back into images repeatedly, and some don't even
 * allow it at all.
 */
static std::map<io::path, video::IImage *> g_imgCache;

/* Maintain a static cache of all pre-scaled textures.  These need to be
 * cleared as well when the cached images.
 */
static std::map<io::path, video::ITexture *> g_txrCache;

/* Manually insert an image into the cache, useful to avoid texture-to-image
 * conversion whenever we can intercept it.
 */
void guiScalingCache(const io::path &key, video::IVideoDriver *driver, video::IImage *value)
{
	if (!g_settings->getBool("gui_scaling_filter"))
		return;

	if (g_imgCache.find(key) != g_imgCache.end())
		return; // Already cached.

	video::IImage *copied = driver->createImage(value->getColorFormat(),
			value->getDimension());
	value->copyTo(copied);
	g_imgCache[key] = copied;
}

// Manually clear the cache, e.g. when switching to different worlds.
void guiScalingCacheClear()
{
	for (auto &it : g_imgCache) {
		if (it.second)
			it.second->drop();
	}
	g_imgCache.clear();
	for (auto &it : g_txrCache) {
		if (it.second)
			RenderingEngine::get_video_driver()->removeTexture(it.second);
	}
	g_txrCache.clear();
}

/* Get a cached, high-quality pre-scaled texture for display purposes.  If the
 * texture is not already cached, attempt to create it.  Returns a pre-scaled texture,
 * or the original texture if unable to pre-scale it.
 */
video::ITexture *guiScalingResizeCached(video::IVideoDriver *driver,
		video::ITexture *src, const core::rect<s32> &srcrect,
		const core::rect<s32> &destrect)
{
	if (src == NULL)
		return src;

	if (!g_settings->getBool("gui_scaling_filter"))
		return src;

	// Calculate scaled texture name.
	char rectstr[200];
	porting::mt_snprintf(rectstr, sizeof(rectstr), "%d:%d:%d:%d:%d:%d",
		srcrect.UpperLeftCorner.X,
		srcrect.UpperLeftCorner.Y,
		srcrect.getWidth(),
		srcrect.getHeight(),
		destrect.getWidth(),
		destrect.getHeight());
	io::path origname = src->getName().getPath();
	io::path scalename = origname + "@guiScalingFilter:" + rectstr;

	// Search for existing scaled texture.
	auto it_txr = g_txrCache.find(scalename);
	video::ITexture *scaled = (it_txr != g_txrCache.end()) ? it_txr->second : nullptr;
	if (scaled)
		return scaled;

	// Try to find the texture converted to an image in the cache.
	// If the image was not found, try to extract it from the texture.
	auto it_img = g_imgCache.find(origname);
	video::IImage *srcimg = (it_img != g_imgCache.end()) ? it_img->second : nullptr;
	if (!srcimg) {
		// Download image from GPU
		srcimg = driver->createImageFromData(src->getColorFormat(),
			src->getSize(), src->lock(video::ETLM_READ_ONLY), false);
		src->unlock();
		g_imgCache[origname] = srcimg;
	}

	// Create a new destination image and scale the source into it.
	imageCleanTransparent(srcimg, 0);

	if (destrect.getWidth() <= 0 || destrect.getHeight() <= 0) {
		errorstream << "Attempted to scale texture to invalid size " << scalename.c_str() << std::endl;
		// Avoid log spam by reusing and displaying the original texture
		src->grab();
		g_txrCache[scalename] = src;
		return src;
	}
	video::IImage *destimg = driver->createImage(src->getColorFormat(),
			core::dimension2d<u32>((u32)destrect.getWidth(),
			(u32)destrect.getHeight()));
	imageScaleNNAA(srcimg, srcrect, destimg);

	// Some platforms are picky about textures being powers of 2, so expand
	// the image dimensions to the next power of 2, if necessary.
	if (!driver->queryFeature(video::EVDF_TEXTURE_NPOT)) {
		video::IImage *po2img = driver->createImage(src->getColorFormat(),
				core::dimension2d<u32>(npot2((u32)destrect.getWidth()),
				npot2((u32)destrect.getHeight())));
		po2img->fill(video::SColor(0, 0, 0, 0));
		destimg->copyTo(po2img);
		destimg->drop();
		destimg = po2img;
	}

	// Convert the scaled image back into a texture.
	scaled = driver->addTexture(scalename, destimg);
	destimg->drop();
	g_txrCache[scalename] = scaled;

	return scaled;
}

/* Convenience wrapper for guiScalingResizeCached that accepts parameters that
 * are available at GUI imagebutton creation time.
 */
video::ITexture *guiScalingImageButton(video::IVideoDriver *driver,
		video::ITexture *src, s32 width, s32 height)
{
	if (src == NULL)
		return src;
	return guiScalingResizeCached(driver, src,
		core::rect<s32>(0, 0, src->getSize().Width, src->getSize().Height),
		core::rect<s32>(0, 0, width, height));
}

/* Replacement for driver->draw2DImage() that uses the high-quality pre-scaled
 * texture, if configured.
 */
void draw2DImageFilterScaled(video::IVideoDriver *driver, video::ITexture *txr,
		const core::rect<s32> &destrect, const core::rect<s32> &srcrect,
		const core::rect<s32> *cliprect, const video::SColor *const colors,
		bool usealpha)
{
	// 9-sliced images might calculate negative texture dimensions. Skip them.
	if (destrect.getWidth() <= 0 || destrect.getHeight() <= 0)
		return;

	// Attempt to pre-scale image in software in high quality.
	video::ITexture *scaled = guiScalingResizeCached(driver, txr, srcrect, destrect);
	if (scaled == NULL)
		return;

	// Correct source rect based on scaled image.
	const core::rect<s32> mysrcrect = (scaled != txr)
		? core::rect<s32>(0, 0, destrect.getWidth(), destrect.getHeight())
		: srcrect;

	driver->draw2DImage(scaled, destrect, mysrcrect, cliprect, colors, usealpha);
}

void draw2DImage9Slice(video::IVideoDriver *driver, video::ITexture *texture,
		const core::rect<s32> &destrect, const core::rect<s32> &srcrect,
		const core::rect<s32> &middlerect, const core::rect<s32> *cliprect,
		const video::SColor *const colors)
{
	// `-x` is interpreted as `w - x`
	core::rect<s32> middle = middlerect;

	if (middlerect.LowerRightCorner.X < 0)
		middle.LowerRightCorner.X += srcrect.getWidth();
	if (middlerect.LowerRightCorner.Y < 0)
		middle.LowerRightCorner.Y += srcrect.getHeight();

	if (colors) {
		// Indices: Top Left, Top Right, Bottom Right, Bottom Left.
		if (colors[0] != colors[1] || colors[0] != colors[2] || colors[0] != colors[3]) {
			warningstream << "Yet unsupported: different colors" << std::endl;
		}
	}

	const core::vector2di lower_right_offset = core::vector2di(srcrect.getWidth(),
			srcrect.getHeight()) - middle.LowerRightCorner;

	const auto &imgsize = texture->getOriginalSize();
	texture = guiScalingImageButton(driver, texture, imgsize.Width * 2, imgsize.Height * 2);

	/*
	Winding order: GL_CW
	Vertices:
	.0--- 1--- 2--- 3
	 |    |    |    |
	.4--- 5--- 6--- 7
	 |    |    |    |
	.8--- 9---10---11
	 |    |    |    |
	12---13---14---15
	*/
	std::array<video::S3DVertex, 16> vertices;
	std::array<u16,           9*2*3> indices; // 2 * 3 triangles per cell

	for (int y = 0; y < 4; ++y) {
		for (int x = 0; x < 4; ++x) {
			video::S3DVertex &vert = vertices[y * 4 + x];

			switch (x) {
			case 0:
				vert.Pos.X = destrect.UpperLeftCorner.X;
				vert.TCoords.X = 0.0f;
				break;

			case 1:
				vert.Pos.X = destrect.UpperLeftCorner.X + middle.UpperLeftCorner.X;
				vert.TCoords.X = (float)middle.UpperLeftCorner.X / imgsize.Width;
				break;

			case 2:
				vert.Pos.X = destrect.LowerRightCorner.X - lower_right_offset.X;
				vert.TCoords.X = 1.0f - (float)lower_right_offset.X / imgsize.Width;
				break;

			case 3:
				vert.Pos.X = destrect.LowerRightCorner.X;
				vert.TCoords.X = 1.0f;
				break;
			}

			switch (y) {
			case 0:
				vert.Pos.Y = destrect.UpperLeftCorner.Y;
				vert.TCoords.Y = 0.0f;
				break;

			case 1:
				vert.Pos.Y = destrect.UpperLeftCorner.Y + middle.UpperLeftCorner.Y;
				vert.TCoords.Y = (float)middle.UpperLeftCorner.Y / imgsize.Height;
				break;

			case 2:
				vert.Pos.Y = destrect.LowerRightCorner.Y - lower_right_offset.Y;
				vert.TCoords.Y = 1.0f - (float)lower_right_offset.Y / imgsize.Height;
				break;

			case 3:
				vert.Pos.Y = destrect.LowerRightCorner.Y;
				vert.TCoords.Y = 1.0f;
				break;
			}

			if (colors) {
				// SColor::getInterpolated() could be used here if ever needed.
				vert.Color = colors[0];
			}
		}
	}

	for (int y = 0; y < 3; ++y) {
		for (int x = 0; x < 3; ++x) {
			const int i = (y * 3 + x) * 2*3;
			// Upper right (e.g. 5,6,10)
			indices[i + 0] = (y * 4 + x) + 0;
			indices[i + 1] = (y * 4 + x) + 1;
			indices[i + 2] = (y * 4 + x) + 5;

			// Lower left (e.g. 10,9,5)
			indices[i + 3] = (y * 4 + x) + 5;
			indices[i + 4] = (y * 4 + x) + 4;
			indices[i + 5] = (y * 4 + x) + 0;
		}
	}

	video::SMaterial mat = driver->getMaterial2D();
	mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	mat.setTexture(0, texture);

	driver->setMaterial(mat);
	driver->draw2DVertexPrimitiveList(
		vertices.data(), vertices.size(),
		indices.data(), indices.size() / 3,
		video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT,
		cliprect
	);
}
