// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "imagesource.h"

#include <IFileSystem.h>
#include "imagefilters.h"
#include "mesh.h"
#include "renderingengine.h"
#include "settings.h"
#include "texturepaths.h"
#include "irrlicht_changes/printing.h"
#include "irr_ptr.h"
#include "util/base64.h"
#include "util/numeric.h"
#include "util/strfnd.h"


////////////////////////////////
// SourceImageCache Functions //
////////////////////////////////

SourceImageCache::~SourceImageCache() {
	for (auto &m_image : m_images) {
		m_image.second->drop();
	}
	m_images.clear();
}

void SourceImageCache::insert(const std::string &name, video::IImage *img, bool prefer_local)
{
	assert(img); // Pre-condition
	// Remove old image
	auto n = m_images.find(name);
	if (n != m_images.end()){
		if (n->second)
			n->second->drop();
	}

	video::IImage* toadd = img;
	bool need_to_grab = true;

	// Try to use local texture instead if asked to
	if (prefer_local) {
		bool is_base_pack;
		std::string path = getTexturePath(name, &is_base_pack);
		// Ignore base pack
		if (!path.empty() && !is_base_pack) {
			video::IImage *img2 = RenderingEngine::get_video_driver()->
				createImageFromFile(path.c_str());
			if (img2){
				toadd = img2;
				need_to_grab = false;
			}
		}
	}

	if (need_to_grab)
		toadd->grab();
	m_images[name] = toadd;
}

video::IImage* SourceImageCache::get(const std::string &name)
{
	auto n = m_images.find(name);
	if (n != m_images.end())
		return n->second;
	return nullptr;
}

// Primarily fetches from cache, secondarily tries to read from filesystem
video::IImage* SourceImageCache::getOrLoad(const std::string &name)
{
	auto n = m_images.find(name);
	if (n != m_images.end()){
		n->second->grab(); // Grab for caller
		return n->second;
	}
	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	std::string path = getTexturePath(name);
	if (path.empty()) {
		infostream << "SourceImageCache::getOrLoad(): No path found for \""
				<< name << "\"" << std::endl;
		return nullptr;
	}
	infostream << "SourceImageCache::getOrLoad(): Loading path \"" << path
			<< "\"" << std::endl;
	video::IImage *img = driver->createImageFromFile(path.c_str());

	if (img){
		m_images[name] = img;
		img->grab(); // Grab for caller
	}
	return img;
}


////////////////////////////
// Image Helper Functions //
////////////////////////////


/** Draw an image on top of another one with gamma-incorrect alpha compositing
 *
 * \tparam overlay If enabled, only modify pixels in dst which are fully opaque.
 *   Defaults to false.
 * \param src Top image. This image must have the ECF_A8R8G8B8 color format.
 * \param dst Bottom image.
 *   The top image is drawn onto this base image in-place.
 * \param src_pos Offset into the Top image
 * \param dst_pos Offset into the Bottom image
 * \param size Size limit of the copied area
*/
template<bool overlay = false>
static void blit_with_alpha2(video::IImage *src, video::IImage *dst,
	v2s32 src_pos, v2s32 dst_pos, v2u32 size);

template<bool overlay = false>
static inline void blit_with_alpha(video::IImage *src, video::IImage *dst,
	v2s32 dst_pos, v2u32 size)
{
	blit_with_alpha2<overlay>(src, dst, v2s32(), dst_pos, size);
}

// Apply a color to an image.  Uses an int (0-255) to calculate the ratio.
// If the ratio is 255 or -1 and keep_alpha is true, then it multiples the
// color alpha with the destination alpha.
// Otherwise, any pixels that are not fully transparent get the color alpha.
static void apply_colorize(video::IImage *dst, v2u32 dst_pos, v2u32 size,
		const video::SColor color, int ratio, bool keep_alpha);

// paint a texture using the given color
static void apply_multiplication(video::IImage *dst, v2u32 dst_pos, v2u32 size,
		const video::SColor color);

// Perform a Screen blend with the given color. The opposite effect of a
// Multiply blend.
static void apply_screen(video::IImage *dst, v2u32 dst_pos, v2u32 size,
		const video::SColor color);

// Adjust the hue, saturation, and lightness of destination. Like
// "Hue-Saturation" in GIMP.
// If colorize is true then the image will be converted to a grayscale
// image as though seen through a colored glass, like "Colorize" in GIMP.
static void apply_hue_saturation(video::IImage *dst, v2u32 dst_pos, v2u32 size,
		s32 hue, s32 saturation, s32 lightness, bool colorize);

// Apply an overlay blend to an images.
// Overlay blend combines Multiply and Screen blend modes.The parts of the top
// layer where the base layer is light become lighter, the parts where the base
// layer is dark become darker.Areas where the base layer are mid grey are
// unaffected.An overlay with the same picture looks like an S - curve.
static void apply_overlay(video::IImage *overlay, video::IImage *dst,
		v2s32 overlay_pos, v2s32 dst_pos, v2u32 size, bool hardlight);

// Adjust the brightness and contrast of the base image. Conceptually like
// "Brightness-Contrast" in GIMP but allowing brightness to be wound all the
// way up to white or down to black.
static void apply_brightness_contrast(video::IImage *dst, v2u32 dst_pos, v2u32 size,
		s32 brightness, s32 contrast);

// Apply a mask to an image
static void apply_mask(video::IImage *mask, video::IImage *dst,
		v2s32 mask_pos, v2s32 dst_pos, v2u32 size);

// Draw or overlay a crack
static void draw_crack(video::IImage *crack, video::IImage *dst,
		bool use_overlay, s32 frame_count, s32 progression,
		video::IVideoDriver *driver, u8 tiles = 1);

// Brighten image
static void brighten(video::IImage *image);
// Parse a transform name
static u32 parseImageTransform(std::string_view s);
// Apply transform to image dimension
static core::dimension2du imageTransformDimension(u32 transform, core::dimension2du dim);
// Apply transform to image data
static void imageTransform(u32 transform, video::IImage *src, video::IImage *dst);

inline static void applyShadeFactor(video::SColor &color, u32 factor)
{
	u32 f = core::clamp<u32>(factor, 0, 256);
	color.setRed(color.getRed() * f / 256);
	color.setGreen(color.getGreen() * f / 256);
	color.setBlue(color.getBlue() * f / 256);
}

static video::IImage *createInventoryCubeImage(
	video::IImage *top, video::IImage *left, video::IImage *right)
{
	core::dimension2du size_top = top->getDimension();
	core::dimension2du size_left = left->getDimension();
	core::dimension2du size_right = right->getDimension();

	u32 size = npot2(std::max({
			size_top.Width, size_top.Height,
			size_left.Width, size_left.Height,
			size_right.Width, size_right.Height,
	}));

	// It must be divisible by 4, to let everything work correctly.
	// But it is a power of 2, so being at least 4 is the same.
	// And the resulting texture should't be too large as well.
	size = core::clamp<u32>(size, 4, 64);

	// With such parameters, the cube fits exactly, touching each image line
	// from `0` to `cube_size - 1`. (Note that division is exact here).
	u32 cube_size = 9 * size;
	u32 offset = size / 2;

	video::IVideoDriver *driver = RenderingEngine::get_video_driver();

	auto lock_image = [size, driver] (video::IImage *&image) -> const u32 * {
		image->grab();
		core::dimension2du dim = image->getDimension();
		video::ECOLOR_FORMAT format = image->getColorFormat();
		if (dim.Width != size || dim.Height != size || format != video::ECF_A8R8G8B8) {
			video::IImage *scaled = driver->createImage(video::ECF_A8R8G8B8, {size, size});
			image->copyToScaling(scaled);
			image->drop();
			image = scaled;
		}
		sanity_check(image->getPitch() == 4 * size);
		return reinterpret_cast<u32 *>(image->getData());
	};
	auto free_image = [] (video::IImage *image) -> void {
		image->drop();
	};

	video::IImage *result = driver->createImage(video::ECF_A8R8G8B8, {cube_size, cube_size});
	sanity_check(result->getPitch() == 4 * cube_size);
	result->fill(video::SColor(0x00000000u));
	u32 *target = reinterpret_cast<u32 *>(result->getData());

	// Draws single cube face
	// `shade_factor` is face brightness, in range [0.0, 1.0]
	// (xu, xv, x1; yu, yv, y1) form coordinate transformation matrix
	// `offsets` list pixels to be drawn for single source pixel
	auto draw_image = [=] (video::IImage *image, float shade_factor,
			s16 xu, s16 xv, s16 x1,
			s16 yu, s16 yv, s16 y1,
			std::initializer_list<v2s16> offsets) -> void {
		u32 brightness = core::clamp<u32>(256 * shade_factor, 0, 256);
		const u32 *source = lock_image(image);
		for (u16 v = 0; v < size; v++) {
			for (u16 u = 0; u < size; u++) {
				video::SColor pixel(*source);
				applyShadeFactor(pixel, brightness);
				s16 x = xu * u + xv * v + x1;
				s16 y = yu * u + yv * v + y1;
				for (const auto &off : offsets)
					target[(y + off.Y) * cube_size + (x + off.X) + offset] = pixel.color;
				source++;
			}
		}
		free_image(image);
	};

	draw_image(top, 1.000000f,
			4, -4, 4 * (size - 1),
			2, 2, 0,
			{
				                {2, 0}, {3, 0}, {4, 0}, {5, 0},
				{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1},
				                {2, 2}, {3, 2}, {4, 2}, {5, 2},
			});

	draw_image(left, 0.836660f,
			4, 0, 0,
			2, 5, 2 * size,
			{
				{0, 0}, {1, 0},
				{0, 1}, {1, 1}, {2, 1}, {3, 1},
				{0, 2}, {1, 2}, {2, 2}, {3, 2},
				{0, 3}, {1, 3}, {2, 3}, {3, 3},
				{0, 4}, {1, 4}, {2, 4}, {3, 4},
				                {2, 5}, {3, 5},
			});

	draw_image(right, 0.670820f,
			4, 0, 4 * size,
			-2, 5, 4 * size - 2,
			{
				                {2, 0}, {3, 0},
				{0, 1}, {1, 1}, {2, 1}, {3, 1},
				{0, 2}, {1, 2}, {2, 2}, {3, 2},
				{0, 3}, {1, 3}, {2, 3}, {3, 3},
				{0, 4}, {1, 4}, {2, 4}, {3, 4},
				{0, 5}, {1, 5},
			});

	return result;
}

static std::string unescape_string(std::string_view str, const char esc = '\\')
{
	std::string out;
	size_t pos = 0, cpos;
	out.reserve(str.size());
	while (1) {
		cpos = str.find_first_of(esc, pos);
		if (cpos == std::string::npos) {
			out += str.substr(pos);
			break;
		}
		out += str.substr(pos, cpos - pos);
		out += str[cpos + 1];
		pos = cpos + 2;
	}
	return out;
}

/*
	Replaces the smaller of the two images with one upscaled to match the
	dimensions of the other.
	Ensure no other references to these images are being held, as one may
	get dropped and switched with a new image.
*/
static void upscaleImagesToMatchLargest(video::IImage *& img1,
	video::IImage *& img2)
{
	core::dimension2d<u32> dim1 = img1->getDimension();
	core::dimension2d<u32> dim2 = img2->getDimension();

	if (dim1 == dim2) {
		// image dimensions match, no scaling required

	}
	else if (dim1.Width * dim1.Height < dim2.Width * dim2.Height) {
		// Upscale img1
		video::IImage *scaled_image = RenderingEngine::get_video_driver()->
			createImage(video::ECF_A8R8G8B8, dim2);
		img1->copyToScaling(scaled_image);
		img1->drop();
		img1 = scaled_image;

	} else {
		// Upscale img2
		video::IImage *scaled_image = RenderingEngine::get_video_driver()->
			createImage(video::ECF_A8R8G8B8, dim1);
		img2->copyToScaling(scaled_image);
		img2->drop();
		img2 = scaled_image;
	}
}

static void blitBaseImage(video::IImage* &src, video::IImage* &dst)
{
	//infostream<<"Blitting "<<part_of_name<<" on base"<<std::endl;
	upscaleImagesToMatchLargest(dst, src);

	// Size of the copied area
	core::dimension2d<u32> dim_dst = dst->getDimension();
	// Position to copy the blitted to in the base image
	core::position2d<s32> pos_to(0,0);

	blit_with_alpha(src, dst, pos_to, dim_dst);
}


namespace {

/** Calculate the result of the overlay texture modifier (`^`) for a single
 * pixel
 *
 * This is not alpha blending if both src and dst are semi-transparent. The
 * reason this is that an old implementation did it wrong, and fixing it would
 * break backwards compatibility (see #14847).
 *
 * \tparam overlay If enabled, only modify dst_col if it is fully opaque
 * \param src_col Color of the top pixel
 * \param dst_col Color of the bottom pixel. This color is modified in-place to
 *   store the result.
*/
template <bool overlay>
void blit_pixel(video::SColor src_col, video::SColor &dst_col)
{
	u8 dst_a = (u8)dst_col.getAlpha();
	if constexpr (overlay) {
		if (dst_a != 255)
			// The bottom pixel has transparency -> do nothing
			return;
	}
	u8 src_a = (u8)src_col.getAlpha();
	if (src_a == 0) {
		// A fully transparent pixel is on top -> do nothing
		return;
	}
	if (src_a == 255 || dst_a == 0) {
		// The top pixel is fully opaque or the bottom pixel is
		// fully transparent -> replace the color
		dst_col = src_col;
		return;
	}
	struct Color { u8 r, g, b; };
	Color src{(u8)src_col.getRed(), (u8)src_col.getGreen(),
		(u8)src_col.getBlue()};
	Color dst{(u8)dst_col.getRed(), (u8)dst_col.getGreen(),
		(u8)dst_col.getBlue()};
	if (dst_a == 255) {
		// A semi-transparent pixel is on top and an opaque one in
		// the bottom -> lerp r, g, and b
		dst.r = (dst.r * (255 - src_a) + src.r * src_a) / 255;
		dst.g = (dst.g * (255 - src_a) + src.g * src_a) / 255;
		dst.b = (dst.b * (255 - src_a) + src.b * src_a) / 255;
		dst_col.set(255, dst.r, dst.g, dst.b);
		return;
	}
	// A semi-transparent pixel is on top of a
	// semi-transparent pixel -> weird overlaying
	dst.r = (dst.r * (255 - src_a) + src.r * src_a) / 255;
	dst.g = (dst.g * (255 - src_a) + src.g * src_a) / 255;
	dst.b = (dst.b * (255 - src_a) + src.b * src_a) / 255;
	dst_a = dst_a + (255 - dst_a) * src_a * src_a / (255 * 255);
	dst_col.set(dst_a, dst.r, dst.g, dst.b);
}

}  // namespace (anonymous)

template<bool overlay>
static void blit_with_alpha2(video::IImage *src, video::IImage *dst,
	v2s32 src_pos, v2s32 dst_pos, v2u32 size)
{
	if (dst->getColorFormat() != video::ECF_A8R8G8B8)
		throw BaseException("blit_with_alpha() supports only ECF_A8R8G8B8 "
			"destination images.");

	if (size.X == 0 || size.Y == 0)
		return;

	const v2u32 src_dim = src->getDimension();
	const v2u32 dst_dim = dst->getDimension();

	// Convert source image if needed
	irr_ptr<video::IImage> converted;
	if (src->getColorFormat() != video::ECF_A8R8G8B8) {
		auto *driver = RenderingEngine::get_video_driver();
		converted.reset(driver->createImage(video::ECF_A8R8G8B8, src_dim));
		sanity_check(converted);
		src->copyTo(converted.get());
		src = converted.get();
	}

	// A negative offset is the same as shifting the other position.
	// equivalent: if (src_pos < 0) { dst_pos += -src_pos; src_pos = 0; }
	dst_pos -= componentwise_min(src_pos, {0,0});
	src_pos = componentwise_max(src_pos, {0,0});
	// and in reverse
	src_pos -= componentwise_min(dst_pos, {0,0});
	dst_pos = componentwise_max(dst_pos, {0,0});

	assert(src_pos.X >= 0 && src_pos.Y >= 0);
	assert(dst_pos.X >= 0 && dst_pos.Y >= 0);
	const v2u32 src_pos_u = v2u32::from(src_pos);
	const v2u32 dst_pos_u = v2u32::from(dst_pos);

	// Out of bounds?
	if (src_pos_u.X >= src_dim.X || src_pos_u.Y >= src_dim.Y)
		return;
	if (dst_pos_u.X >= dst_dim.X || dst_pos_u.Y >= dst_dim.Y)
		return;

	// Truncate blit area as needed
	size = componentwise_min(size,
		componentwise_min(src_dim - src_pos_u, dst_dim - dst_pos_u));

	// Do it!
	video::SColor *pixels_src =
		reinterpret_cast<video::SColor *>(src->getData());
	video::SColor *pixels_dst =
		reinterpret_cast<video::SColor *>(dst->getData());
	for (u32 y0 = 0; y0 < size.Y; ++y0) {
		size_t i_src = (src_pos_u.Y + y0) * src_dim.X + src_pos_u.X;
		size_t i_dst = (dst_pos_u.Y + y0) * dst_dim.X + dst_pos_u.X;
		for (u32 x0 = 0; x0 < size.X; ++x0) {
			blit_pixel<overlay>(pixels_src[i_src++], pixels_dst[i_dst++]);
		}
	}
}

/*
	Apply color to destination, using a weighted interpolation blend
*/
static void apply_colorize(video::IImage *dst, v2u32 dst_pos, v2u32 size,
		const video::SColor color, int ratio, bool keep_alpha)
{
	u32 alpha = color.getAlpha();
	video::SColor dst_c;
	if ((ratio == -1 && alpha == 255) || ratio == 255) { // full replacement of color
		if (keep_alpha) { // replace the color with alpha = dest alpha * color alpha
			dst_c = color;
			for (u32 y = dst_pos.Y; y < dst_pos.Y + size.Y; y++)
			for (u32 x = dst_pos.X; x < dst_pos.X + size.X; x++) {
				u32 dst_alpha = dst->getPixel(x, y).getAlpha();
				if (dst_alpha > 0) {
					dst_c.setAlpha(dst_alpha * alpha / 255);
					dst->setPixel(x, y, dst_c);
				}
			}
		} else { // replace the color including the alpha
			for (u32 y = dst_pos.Y; y < dst_pos.Y + size.Y; y++)
			for (u32 x = dst_pos.X; x < dst_pos.X + size.X; x++)
				if (dst->getPixel(x, y).getAlpha() > 0)
					dst->setPixel(x, y, color);
		}
	} else {  // interpolate between the color and destination
		float interp = (ratio == -1 ? color.getAlpha() / 255.0f : ratio / 255.0f);
		for (u32 y = dst_pos.Y; y < dst_pos.Y + size.Y; y++)
		for (u32 x = dst_pos.X; x < dst_pos.X + size.X; x++) {
			dst_c = dst->getPixel(x, y);
			if (dst_c.getAlpha() > 0) {
				dst_c = color.getInterpolated(dst_c, interp);
				dst->setPixel(x, y, dst_c);
			}
		}
	}
}

/*
	Apply color to destination, using a Multiply blend mode
*/
static void apply_multiplication(video::IImage *dst, v2u32 dst_pos, v2u32 size,
		const video::SColor color)
{
	video::SColor dst_c;

	for (u32 y = dst_pos.Y; y < dst_pos.Y + size.Y; y++)
	for (u32 x = dst_pos.X; x < dst_pos.X + size.X; x++) {
		dst_c = dst->getPixel(x, y);
		dst_c.set(
				dst_c.getAlpha(),
				(dst_c.getRed() * color.getRed()) / 255,
				(dst_c.getGreen() * color.getGreen()) / 255,
				(dst_c.getBlue() * color.getBlue()) / 255
				);
		dst->setPixel(x, y, dst_c);
	}
}

/*
	Apply color to destination, using a Screen blend mode
*/
static void apply_screen(video::IImage *dst, v2u32 dst_pos, v2u32 size,
		const video::SColor color)
{
	video::SColor dst_c;

	for (u32 y = dst_pos.Y; y < dst_pos.Y + size.Y; y++)
	for (u32 x = dst_pos.X; x < dst_pos.X + size.X; x++) {
		dst_c = dst->getPixel(x, y);
		dst_c.set(
			dst_c.getAlpha(),
			255 - ((255 - dst_c.getRed())   * (255 - color.getRed()))   / 255,
			255 - ((255 - dst_c.getGreen()) * (255 - color.getGreen())) / 255,
			255 - ((255 - dst_c.getBlue())  * (255 - color.getBlue()))  / 255
		);
		dst->setPixel(x, y, dst_c);
	}
}

/*
	Adjust the hue, saturation, and lightness of destination. Like
	"Hue-Saturation" in GIMP, but with 0 as the mid-point.
	Hue should be from -180 to +180, or from 0 to 360.
	Saturation and Lightness are percentages.
	Lightness is from -100 to +100.
	Saturation goes down to -100 (fully desaturated) but can go above 100,
	allowing for even muted colors to become saturated.

	If colorize is true then saturation is from 0 to 100, and destination will
	be converted to a grayscale image as seen through a colored glass, like
	"Colorize" in GIMP.
*/
static void apply_hue_saturation(video::IImage *dst, v2u32 dst_pos, v2u32 size,
	s32 hue, s32 saturation, s32 lightness, bool colorize)
{
	video::SColorf colorf;
	video::SColorHSL hsl;
	f32 norm_s = core::clamp(saturation, -100, 1000) / 100.0f;
	f32 norm_l = core::clamp(lightness,  -100, 100) / 100.0f;

	if (colorize) {
		hsl.Saturation = core::clamp((f32)saturation, 0.0f, 100.0f);
	}

	for (u32 y = dst_pos.Y; y < dst_pos.Y + size.Y; y++)
		for (u32 x = dst_pos.X; x < dst_pos.X + size.X; x++) {

			if (colorize) {
				f32 lum = dst->getPixel(x, y).getBrightness() / 255.0f;

				if (norm_l < 0) {
					lum *= norm_l + 1.0f;
				} else {
					lum = lum * (1.0f - norm_l) + norm_l;
				}
				hsl.Hue = 0;
				hsl.Luminance = lum * 100;

			} else {
				// convert the RGB to HSL
				colorf = video::SColorf(dst->getPixel(x, y));
				hsl.fromRGB(colorf);

				if (norm_l < 0) {
					hsl.Luminance *= norm_l + 1.0f;
				} else{
					hsl.Luminance = hsl.Luminance + norm_l * (100.0f - hsl.Luminance);
				}

				// Adjusting saturation in the same manner as lightness resulted in
				// muted colors being affected too much and bright colors not
				// affected enough, so I'm borrowing a leaf out of gimp's book and
				// using a different scaling approach for saturation.
				// https://github.com/GNOME/gimp/blob/6cc1e035f1822bf5198e7e99a53f7fa6e281396a/app/operations/gimpoperationhuesaturation.c#L139-L145=
				// This difference is why values over 100% are not necessary for
				// lightness but are very useful with saturation. An alternative UI
				// approach would be to have an upper saturation limit of 100, but
				// multiply positive values by ~3 to make it a more useful positive
				// range scale.
				hsl.Saturation *= norm_s + 1.0f;
				hsl.Saturation = core::clamp(hsl.Saturation, 0.0f, 100.0f);
			}

			// Apply the specified HSL adjustments
			hsl.Hue = fmodf(hsl.Hue + hue, 360);
			if (hsl.Hue < 0)
				hsl.Hue += 360;

			// Convert back to RGB
			hsl.toRGB(colorf);
			dst->setPixel(x, y, colorf.toSColor());
		}
}


/*
	Apply an Overlay blend to destination
	If hardlight is true then swap the dst & blend images (a hardlight blend)
*/
static void apply_overlay(video::IImage *blend, video::IImage *dst,
	v2s32 blend_pos, v2s32 dst_pos, v2u32 size, bool hardlight)
{
	video::IImage *blend_layer = hardlight ? dst : blend;
	video::IImage *base_layer  = hardlight ? blend : dst;
	v2s32 blend_layer_pos = hardlight ? dst_pos : blend_pos;
	v2s32 base_layer_pos  = hardlight ? blend_pos : dst_pos;

	for (u32 y = 0; y < size.Y; y++)
	for (u32 x = 0; x < size.X; x++) {
		s32 base_x = x + base_layer_pos.X;
		s32 base_y = y + base_layer_pos.Y;

		video::SColor blend_c =
			blend_layer->getPixel(x + blend_layer_pos.X, y + blend_layer_pos.Y);
		video::SColor base_c = base_layer->getPixel(base_x, base_y);
		f32 blend_r = blend_c.getRed()   / 255.0f;
		f32 blend_g = blend_c.getGreen() / 255.0f;
		f32 blend_b = blend_c.getBlue()  / 255.0f;
		f32 base_r = base_c.getRed()   / 255.0f;
		f32 base_g = base_c.getGreen() / 255.0f;
		f32 base_b = base_c.getBlue()  / 255.0f;

		base_c.set(
			base_c.getAlpha(),
			// Do a Multiply blend if less that 0.5, otherwise do a Screen blend
			(u32)((base_r < 0.5f ? 2 * base_r * blend_r : 1 - 2 * (1 - base_r) * (1 - blend_r)) * 255),
			(u32)((base_g < 0.5f ? 2 * base_g * blend_g : 1 - 2 * (1 - base_g) * (1 - blend_g)) * 255),
			(u32)((base_b < 0.5f ? 2 * base_b * blend_b : 1 - 2 * (1 - base_b) * (1 - blend_b)) * 255)
		);
		dst->setPixel(base_x, base_y, base_c);
	}
}

/*
	Adjust the brightness and contrast of the base image.

	Conceptually like GIMP's "Brightness-Contrast" feature but allows brightness to be
	wound all the way up to white or down to black.
*/
static void apply_brightness_contrast(video::IImage *dst, v2u32 dst_pos, v2u32 size,
	s32 brightness, s32 contrast)
{
	// Only allow normalized contrast to get as high as 127/128 to avoid infinite slope.
	// (we could technically allow -128/128 here as that would just result in 0 slope)
	f32 norm_c = core::clamp(contrast,   -127, 127) / 128.0f;
	f32 norm_b = core::clamp(brightness, -127, 127) / 127.0f;

	// Scale brightness so its range is -127.5 to 127.5, otherwise brightness
	// adjustments will outputs values from 0.5 to 254.5 instead of 0 to 255.
	f32 scaled_b = brightness * 127.5f / 127;

	// Calculate a contrast slope such that that no colors will get clamped due
	// to the brightness setting.
	// This allows the texture modifier to used as a brightness modifier without
	// the user having to calculate a contrast to avoid clipping at that brightness.
	f32 slope = 1 - std::fabs(norm_b);

	// Apply the user's contrast adjustment to the calculated slope, such that
	// -127 will make it near-vertical and +127 will make it horizontal
	f32 angle = std::atan(slope);
	angle += norm_c <= 0
		? norm_c * angle // allow contrast slope to be lowered to 0
		: norm_c * (M_PI_2 - angle); // allow contrast slope to be raised almost vert.
	slope = std::tan(angle);

	f32 c = slope <= 1
		? -slope * 127.5f + 127.5f + scaled_b    // shift up/down when slope is horiz.
		: -slope * (127.5f - scaled_b) + 127.5f; // shift left/right when slope is vert.

	// add 0.5 to c so that when the final result is cast to int, it is effectively
	// rounded rather than trunc'd.
	c += 0.5f;

	video::SColor dst_c;
	for (u32 y = dst_pos.Y; y < dst_pos.Y + size.Y; y++)
	for (u32 x = dst_pos.X; x < dst_pos.X + size.X; x++) {
		dst_c = dst->getPixel(x, y);

		dst_c.set(
			dst_c.getAlpha(),
			core::clamp((int)(slope * dst_c.getRed()   + c), 0, 255),
			core::clamp((int)(slope * dst_c.getGreen() + c), 0, 255),
			core::clamp((int)(slope * dst_c.getBlue()  + c), 0, 255)
		);
		dst->setPixel(x, y, dst_c);
	}
}

/*
	Apply mask to destination
*/
static void apply_mask(video::IImage *mask, video::IImage *dst,
		v2s32 mask_pos, v2s32 dst_pos, v2u32 size)
{
	for (u32 y0 = 0; y0 < size.Y; y0++) {
		for (u32 x0 = 0; x0 < size.X; x0++) {
			s32 mask_x = x0 + mask_pos.X;
			s32 mask_y = y0 + mask_pos.Y;
			s32 dst_x = x0 + dst_pos.X;
			s32 dst_y = y0 + dst_pos.Y;
			video::SColor mask_c = mask->getPixel(mask_x, mask_y);
			video::SColor dst_c = dst->getPixel(dst_x, dst_y);
			dst_c.color &= mask_c.color;
			dst->setPixel(dst_x, dst_y, dst_c);
		}
	}
}

static video::IImage *create_crack_image(video::IImage *crack, s32 frame_index,
		core::dimension2d<u32> size, u8 tiles, video::IVideoDriver *driver)
{
	core::dimension2d<u32> strip_size = crack->getDimension();

	if (tiles == 0 || strip_size.getArea() == 0)
		return nullptr;

	core::dimension2d<u32> frame_size(strip_size.Width, strip_size.Width);
	core::dimension2d<u32> tile_size(size / tiles);
	s32 frame_count = strip_size.Height / strip_size.Width;
	if (frame_index >= frame_count)
		frame_index = frame_count - 1;
	core::rect<s32> frame(v2s32(0, frame_index * frame_size.Height), frame_size);
	video::IImage *result = nullptr;

	// extract crack frame
	video::IImage *crack_tile = driver->createImage(video::ECF_A8R8G8B8, tile_size);
	if (!crack_tile)
		return nullptr;
	if (tile_size == frame_size) {
		crack->copyTo(crack_tile, v2s32(0, 0), frame);
	} else {
		video::IImage *crack_frame = driver->createImage(video::ECF_A8R8G8B8, frame_size);
		if (!crack_frame)
			goto exit__has_tile;
		crack->copyTo(crack_frame, v2s32(0, 0), frame);
		crack_frame->copyToScaling(crack_tile);
		crack_frame->drop();
	}
	if (tiles == 1)
		return crack_tile;

	// tile it
	result = driver->createImage(video::ECF_A8R8G8B8, size);
	if (!result)
		goto exit__has_tile;
	result->fill({});
	for (u8 i = 0; i < tiles; i++)
		for (u8 j = 0; j < tiles; j++)
			crack_tile->copyTo(result, v2s32(i * tile_size.Width, j * tile_size.Height));

exit__has_tile:
	crack_tile->drop();
	return result;
}

static void draw_crack(video::IImage *crack, video::IImage *dst,
		bool use_overlay, s32 frame_count, s32 progression,
		video::IVideoDriver *driver, u8 tiles)
{
	// Dimension of destination image
	core::dimension2d<u32> dim_dst = dst->getDimension();
	// Limit frame_count
	if (frame_count > (s32) dim_dst.Height)
		frame_count = dim_dst.Height;
	if (frame_count < 1)
		frame_count = 1;
	// Dimension of the scaled crack stage,
	// which is the same as the dimension of a single destination frame
	core::dimension2d<u32> frame_size(
		dim_dst.Width,
		dim_dst.Height / frame_count
	);
	video::IImage *crack_scaled = create_crack_image(crack, progression,
			frame_size, tiles, driver);
	if (!crack_scaled)
		return;

	auto blit = use_overlay ? blit_with_alpha<true> : blit_with_alpha<false>;
	for (s32 i = 0; i < frame_count; ++i) {
		v2s32 dst_pos(0, frame_size.Height * i);
		blit(crack_scaled, dst, dst_pos, frame_size);
	}

	crack_scaled->drop();
}

static void brighten(video::IImage *image)
{
	if (!image)
		return;

	core::dimension2d<u32> dim = image->getDimension();

	for (u32 y=0; y<dim.Height; y++)
	for (u32 x=0; x<dim.Width; x++)
	{
		video::SColor c = image->getPixel(x,y);
		c.setRed(127.5f + 0.5f * c.getRed());
		c.setGreen(127.5f + 0.5f * c.getGreen());
		c.setBlue(127.5f + 0.5f * c.getBlue());
		image->setPixel(x,y,c);
	}
}

static u32 parseImageTransform(std::string_view s)
{
	int total_transform = 0;

	std::string transform_names[8];
	transform_names[0] = "i";
	transform_names[1] = "r90";
	transform_names[2] = "r180";
	transform_names[3] = "r270";
	transform_names[4] = "fx";
	transform_names[6] = "fy";

	std::size_t pos = 0;
	while(pos < s.size())
	{
		int transform = -1;
		for (int i = 0; i <= 7; ++i)
		{
			const std::string &name_i = transform_names[i];

			if (s[pos] == ('0' + i))
			{
				transform = i;
				pos++;
				break;
			}

			if (!(name_i.empty()) && lowercase(s.substr(pos, name_i.size())) == name_i) {
				transform = i;
				pos += name_i.size();
				break;
			}
		}
		if (transform < 0)
			break;

		// Multiply total_transform and transform in the group D4
		int new_total = 0;
		if (transform < 4)
			new_total = (transform + total_transform) % 4;
		else
			new_total = (transform - total_transform + 8) % 4;
		if ((transform >= 4) ^ (total_transform >= 4))
			new_total += 4;

		total_transform = new_total;
	}
	return total_transform;
}

static core::dimension2du imageTransformDimension(u32 transform, core::dimension2du dim)
{
	if (transform % 2 == 0)
		return dim;

	return core::dimension2du(dim.Height, dim.Width);
}

static void imageTransform(u32 transform, video::IImage *src, video::IImage *dst)
{
	if (!src || !dst)
		return;

	core::dimension2d<u32> dstdim = dst->getDimension();

	// Pre-conditions
	assert(dstdim == imageTransformDimension(transform, src->getDimension()));
	assert(transform <= 7);

	/*
		Compute the transformation from source coordinates (sx,sy)
		to destination coordinates (dx,dy).
	*/
	int sxn = 0;
	int syn = 2;
	if (transform == 0)         // identity
		sxn = 0, syn = 2;  //   sx = dx, sy = dy
	else if (transform == 1)    // rotate by 90 degrees ccw
		sxn = 3, syn = 0;  //   sx = (H-1) - dy, sy = dx
	else if (transform == 2)    // rotate by 180 degrees
		sxn = 1, syn = 3;  //   sx = (W-1) - dx, sy = (H-1) - dy
	else if (transform == 3)    // rotate by 270 degrees ccw
		sxn = 2, syn = 1;  //   sx = dy, sy = (W-1) - dx
	else if (transform == 4)    // flip x
		sxn = 1, syn = 2;  //   sx = (W-1) - dx, sy = dy
	else if (transform == 5)    // flip x then rotate by 90 degrees ccw
		sxn = 2, syn = 0;  //   sx = dy, sy = dx
	else if (transform == 6)    // flip y
		sxn = 0, syn = 3;  //   sx = dx, sy = (H-1) - dy
	else if (transform == 7)    // flip y then rotate by 90 degrees ccw
		sxn = 3, syn = 1;  //   sx = (H-1) - dy, sy = (W-1) - dx

	for (u32 dy=0; dy<dstdim.Height; dy++)
	for (u32 dx=0; dx<dstdim.Width; dx++)
	{
		u32 entries[4] = {dx, dstdim.Width-1-dx, dy, dstdim.Height-1-dy};
		u32 sx = entries[sxn];
		u32 sy = entries[syn];
		video::SColor c = src->getPixel(sx,sy);
		dst->setPixel(dx,dy,c);
	}
}


///////////////////////////
// ImageSource Functions //
///////////////////////////

#define CHECK_BASEIMG() \
	do { \
		if (!baseimg) { \
			errorstream << "generateImagePart(): baseimg == NULL" \
					<< " for part_of_name=\"" << part_of_name \
					<< "\", cancelling." << std::endl; \
			return false; \
		} \
	} while(0)

#define COMPLAIN_INVALID(description) \
	do { \
		errorstream << "generateImagePart(): invalid " << (description) \
			<< " for part_of_name=\"" << part_of_name \
			<< "\", cancelling." << std::endl; \
		return false; \
	} while(0)

#define CHECK_DIM(w, h) \
	do { \
		if ((w) <= 0 || (w) > MAX_IMAGE_DIMENSION) \
			COMPLAIN_INVALID("width"); \
		if ((h) <= 0 || (h) > MAX_IMAGE_DIMENSION) \
			COMPLAIN_INVALID("height"); \
	} while(0)

bool ImageSource::generateImagePart(std::string_view part_of_name,
		video::IImage *& baseimg, std::set<std::string> &source_image_names)
{
	const char escape = '\\'; // same as in generateImage()
	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	sanity_check(driver);

	if (baseimg && (baseimg->getDimension().Width == 0 ||
			baseimg->getDimension().Height == 0)) {
		errorstream << "generateImagePart(): baseimg is zero-sized?!"
			<< std::endl;
		baseimg->drop();
		baseimg = nullptr;
	}

	// Stuff starting with [ are special commands
	if (part_of_name.empty() || part_of_name[0] != '[') {
		std::string part_s(part_of_name);
		source_image_names.insert(part_s);
		video::IImage *image = m_sourcecache.getOrLoad(part_s);

		if (!image) {
			// Do not create the dummy texture
			if (part_of_name.empty())
				return true;

			errorstream << "generateImagePart(): Could not load image \""
				<< part_of_name << "\" while building texture; "
				"Creating a dummy image" << std::endl;

			core::dimension2d<u32> dim(1,1);
			image = driver->createImage(video::ECF_A8R8G8B8, dim);
			sanity_check(image != NULL);
			image->setPixel(0,0, video::SColor(255,myrand()%256,
					myrand()%256,myrand()%256));
		}

		// load as base or blit
		if (!baseimg)
		{
			/*
				Copy it this way to get an alpha channel.
				Otherwise images with alpha cannot be blitted on
				images that don't have alpha in the original file.
			*/
			baseimg = driver->createImage(video::ECF_A8R8G8B8, image->getDimension());
			image->copyTo(baseimg);
		}
		// Else blit on base.
		else
		{
			blitBaseImage(image, baseimg);
		}

		image->drop();
	}
	else
	{
		// A special texture modification

		/*
			[crack:N:P
			[cracko:N:P
			Adds a cracking texture
			N = animation frame count, P = crack progression
		*/
		if (str_starts_with(part_of_name, "[crack"))
		{
			CHECK_BASEIMG();

			// Crack image number and overlay option
			// Format: crack[o][:<tiles>]:<frame_count>:<frame>
			bool use_overlay = (part_of_name[6] == 'o');
			Strfnd sf(part_of_name);
			sf.next(":");
			s32 frame_count = stoi(sf.next(":"));
			s32 progression = stoi(sf.next(":"));
			s32 tiles = 1;
			// Check whether there is the <tiles> argument, that is,
			// whether there are 3 arguments. If so, shift values
			// as the first and not the last argument is optional.
			auto s = sf.next(":");
			if (!s.empty()) {
				tiles = frame_count;
				frame_count = progression;
				progression = stoi(s);
			}

			if (progression >= 0) {
				/*
					Load crack image.

					It is an image with a number of cracking stages
					horizontally tiled.
				*/
				video::IImage *img_crack = m_sourcecache.getOrLoad(
					"crack_anylength.png");

				if (img_crack) {
					draw_crack(img_crack, baseimg,
						use_overlay, frame_count,
						progression, driver, tiles);
					img_crack->drop();
				}
			}
		}
		/*
			[combine:WxH:X,Y=filename:X,Y=filename2
			Creates a bigger texture from any amount of smaller ones
		*/
		else if (str_starts_with(part_of_name, "[combine"))
		{
			Strfnd sf(part_of_name);
			sf.next(":");
			u32 w0 = stoi(sf.next("x"));
			u32 h0 = stoi(sf.next(":"));
			if (!baseimg) {
				CHECK_DIM(w0, h0);
				baseimg = driver->createImage(video::ECF_A8R8G8B8, {w0, h0});
				baseimg->fill(video::SColor(0,0,0,0));
			}

			while (!sf.at_end()) {
				v2s32 pos_base;
				pos_base.X = stoi(sf.next(","));
				pos_base.Y = stoi(sf.next("="));
				std::string filename = unescape_string(sf.next_esc(":", escape), escape);

				auto basedim = baseimg->getDimension();
				if (pos_base.X > (s32)basedim.Width || pos_base.Y > (s32)basedim.Height) {
					warningstream << "generateImagePart(): Skipping \""
						<< filename << "\" as it's out-of-bounds " << pos_base
						<< " for [combine" << std::endl;
					continue;
				}
				tracestream << "Adding \"" << filename << "\" to combined "
					<< pos_base << std::endl;

				video::IImage *img = generateImage(filename, source_image_names);
				if (!img) {
					errorstream << "generateImagePart(): Failed to load image \""
						<< filename << "\" for [combine" << std::endl;
					continue;
				}
				const auto dim = img->getDimension();
				if (pos_base.X + dim.Width <= 0 || pos_base.Y + dim.Height <= 0) {
					warningstream << "generateImagePart(): Skipping \""
						<< filename << "\" as it's out-of-bounds " << pos_base
						<< " for [combine" << std::endl;
					img->drop();
					continue;
				}

				blit_with_alpha(img, baseimg, pos_base, dim);
				img->drop();
			}
		}
		/*
			[fill:WxH:color
			[fill:WxH:X,Y:color
			Creates a texture of the given size and color, optionally with an <x>,<y>
			position. An alpha value may be specified in the `Colorstring`.
		*/
		else if (str_starts_with(part_of_name, "[fill"))
		{
			u32 x = 0;
			u32 y = 0;

			Strfnd sf(part_of_name);
			sf.next(":");
			u32 width  = stoi(sf.next("x"));
			u32 height = stoi(sf.next(":"));
			std::string color_or_x = sf.next(",");

			video::SColor color;
			if (!parseColorString(color_or_x, color, true)) {
				x = stoi(color_or_x);
				y = stoi(sf.next(":"));
				std::string color_str = sf.next(":");

				if (!parseColorString(color_str, color, false))
					return false;
			}
			core::dimension2d<u32> dim(width, height);

			CHECK_DIM(dim.Width, dim.Height);
			if (baseimg) {
				auto basedim = baseimg->getDimension();
				if (x >= basedim.Width || y >= basedim.Height)
					COMPLAIN_INVALID("X or Y offset");
			}

			video::IImage *img = driver->createImage(video::ECF_A8R8G8B8, dim);
			img->fill(color);

			if (baseimg == nullptr) {
				baseimg = img;
			} else {
				blit_with_alpha(img, baseimg, v2s32(x, y), dim);
				img->drop();
			}
		}
		/*
			[brighten
		*/
		else if (str_starts_with(part_of_name, "[brighten"))
		{
			CHECK_BASEIMG();

			brighten(baseimg);
		}
		/*
			[noalpha
			Make image completely opaque.
			Used for the leaves texture when in old leaves mode, so
			that the transparent parts don't look completely black
			when simple alpha channel is used for rendering.
		*/
		else if (str_starts_with(part_of_name, "[noalpha"))
		{
			CHECK_BASEIMG();
			core::dimension2d<u32> dim = baseimg->getDimension();

			// Set alpha to full
			for (u32 y=0; y<dim.Height; y++)
			for (u32 x=0; x<dim.Width; x++)
			{
				video::SColor c = baseimg->getPixel(x,y);
				c.setAlpha(255);
				baseimg->setPixel(x,y,c);
			}
		}
		/*
			[makealpha:R,G,B
			Convert one color to transparent.
		*/
		else if (str_starts_with(part_of_name, "[makealpha:"))
		{
			CHECK_BASEIMG();

			Strfnd sf(part_of_name.substr(11));
			u32 r1 = stoi(sf.next(","));
			u32 g1 = stoi(sf.next(","));
			u32 b1 = stoi(sf.next(""));

			core::dimension2d<u32> dim = baseimg->getDimension();

			for (u32 y=0; y<dim.Height; y++)
			for (u32 x=0; x<dim.Width; x++)
			{
				video::SColor c = baseimg->getPixel(x,y);
				u32 r = c.getRed();
				u32 g = c.getGreen();
				u32 b = c.getBlue();
				if (!(r == r1 && g == g1 && b == b1))
					continue;
				c.setAlpha(0);
				baseimg->setPixel(x,y,c);
			}
		}
		/*
			[transformN
			Rotates and/or flips the image.

			N can be a number (between 0 and 7) or a transform name.
			Rotations are counter-clockwise.
			0  I      identity
			1  R90    rotate by 90 degrees
			2  R180   rotate by 180 degrees
			3  R270   rotate by 270 degrees
			4  FX     flip X
			5  FXR90  flip X then rotate by 90 degrees
			6  FY     flip Y
			7  FYR90  flip Y then rotate by 90 degrees

			Note: Transform names can be concatenated to produce
			their product (applies the first then the second).
			The resulting transform will be equivalent to one of the
			eight existing ones, though (see: dihedral group).
		*/
		else if (str_starts_with(part_of_name, "[transform"))
		{
			CHECK_BASEIMG();

			u32 transform = parseImageTransform(part_of_name.substr(10));
			core::dimension2d<u32> dim = imageTransformDimension(
					transform, baseimg->getDimension());
			video::IImage *image = driver->createImage(
					baseimg->getColorFormat(), dim);
			sanity_check(image != NULL);
			imageTransform(transform, baseimg, image);
			baseimg->drop();
			baseimg = image;
		}
		/*
			[inventorycube{topimage{leftimage{rightimage
			In every subimage, replace ^ with &.
			Create an "inventory cube".
			NOTE: This should be used only on its own.
			Example (a grass block (not actually used in game):
			"[inventorycube{grass.png{mud.png&grass_side.png{mud.png&grass_side.png"
		*/
		else if (str_starts_with(part_of_name, "[inventorycube"))
		{
			if (baseimg) {
				errorstream<<"generateImagePart(): baseimg != NULL "
						<<"for part_of_name=\""<<part_of_name
						<<"\", cancelling."<<std::endl;
				return false;
			}

			std::string part_s(part_of_name);
			str_replace(part_s, '&', '^');

			Strfnd sf(part_s);
			sf.next("{");
			std::string imagename_top = sf.next("{");
			std::string imagename_left = sf.next("{");
			std::string imagename_right = sf.next("{");

			// Generate images for the faces of the cube
			video::IImage *img_top = generateImage(imagename_top, source_image_names);
			video::IImage *img_left = generateImage(imagename_left, source_image_names);
			video::IImage *img_right = generateImage(imagename_right, source_image_names);

			if (!img_top || !img_left || !img_right) {
				errorstream << "generateImagePart(): Failed to create textures"
						<< " for inventorycube \"" << part_of_name << "\""
						<< std::endl;
				return false;
			}

			baseimg = createInventoryCubeImage(img_top, img_left, img_right);

			// Face images are not needed anymore
			img_top->drop();
			img_left->drop();
			img_right->drop();

			return true;
		}
		/*
			[lowpart:percent:filename
			Adds the lower part of a texture
		*/
		else if (str_starts_with(part_of_name, "[lowpart:"))
		{
			Strfnd sf(part_of_name);
			sf.next(":");
			u32 percent = stoi(sf.next(":"), 0, 100);
			std::string filename = unescape_string(sf.next_esc(":", escape), escape);

			video::IImage *img = generateImage(filename, source_image_names);
			if (img) {
				core::dimension2d<u32> dim = img->getDimension();
				if (!baseimg) {
					baseimg = driver->createImage(video::ECF_A8R8G8B8, dim);
					baseimg->fill(video::SColor(0,0,0,0));
				}

				v2s32 clippos(0, 0);
				clippos.Y = dim.Height * (100-percent) / 100;
				core::dimension2du clipdim = dim;
				clipdim.Height = clipdim.Height * percent / 100 + 1;
				blit_with_alpha2(img, baseimg, clippos, clippos, clipdim);
				img->drop();
			}
		}
		/*
			[verticalframe:N:I
			Crops a frame of a vertical animation.
			N = frame count, I = frame index
		*/
		else if (str_starts_with(part_of_name, "[verticalframe:"))
		{
			CHECK_BASEIMG();

			Strfnd sf(part_of_name);
			sf.next(":");
			u32 frame_count = stoi(sf.next(":"));
			u32 frame_index = stoi(sf.next(":"));

			if (frame_count == 0){
				errorstream << "generateImagePart(): invalid frame_count "
						<< "for part_of_name=\"" << part_of_name
						<< "\", using frame_count = 1 instead." << std::endl;
				frame_count = 1;
			}
			if (frame_index >= frame_count)
				frame_index = frame_count - 1;

			v2u32 frame_size = baseimg->getDimension();
			frame_size.Y /= frame_count;
			if (frame_size.Y == 0)
				frame_size.Y = 1;

			video::IImage *img = driver->createImage(video::ECF_A8R8G8B8,
					frame_size);

			core::dimension2d<u32> dim = frame_size;
			core::position2d<s32> pos_dst(0, 0);
			core::position2d<s32> pos_src(0, frame_index * frame_size.Y);
			baseimg->copyTo(img, pos_dst,
					core::rect<s32>(pos_src, dim), nullptr);
			// Replace baseimg
			baseimg->drop();
			baseimg = img;
		}
		/*
			[mask:filename
			Applies a mask to an image
		*/
		else if (str_starts_with(part_of_name, "[mask:"))
		{
			CHECK_BASEIMG();

			Strfnd sf(part_of_name);
			sf.next(":");
			std::string filename = unescape_string(sf.next_esc(":", escape), escape);

			video::IImage *img = generateImage(filename, source_image_names);
			if (img) {
				upscaleImagesToMatchLargest(baseimg, img);

				apply_mask(img, baseimg, v2s32(0, 0), v2s32(0, 0),
						img->getDimension());
				img->drop();
			} else {
				errorstream << "generateImagePart(): Failed to load image \""
						<< filename << "\" for [mask" << std::endl;
			}
		}
		/*
		[multiply:color
		or
		[screen:color
			Multiply and Screen blend modes are basic blend modes for darkening and lightening
			images, respectively.
			A Multiply blend multiplies a given color to every pixel of an image.
			A Screen blend has the opposite effect to a Multiply blend.
			color = color as ColorString
		*/
		else if (str_starts_with(part_of_name, "[multiply:") ||
		         str_starts_with(part_of_name, "[screen:")) {
			Strfnd sf(part_of_name);
			sf.next(":");
			std::string color_str = sf.next(":");

			CHECK_BASEIMG();

			video::SColor color;

			if (!parseColorString(color_str, color, false))
				return false;
			if (str_starts_with(part_of_name, "[multiply:")) {
				apply_multiplication(baseimg, v2u32(0, 0),
					baseimg->getDimension(), color);
			} else {
				apply_screen(baseimg, v2u32(0, 0), baseimg->getDimension(), color);
			}
		}
		/*
			[colorize:color:ratio
			Overlays image with given color
			color = color as ColorString
			ratio = optional string "alpha", or a weighting between 0 and 255
		*/
		else if (str_starts_with(part_of_name, "[colorize:"))
		{
			Strfnd sf(part_of_name);
			sf.next(":");
			std::string color_str = sf.next(":");
			std::string ratio_str = sf.next(":");

			CHECK_BASEIMG();

			video::SColor color;
			int ratio = -1;
			bool keep_alpha = false;

			if (!parseColorString(color_str, color, false))
				return false;

			if (is_number(ratio_str))
				ratio = mystoi(ratio_str, 0, 255);
			else if (ratio_str == "alpha")
				keep_alpha = true;

			apply_colorize(baseimg, v2u32(0, 0), baseimg->getDimension(), color, ratio, keep_alpha);
		}
		/*
			[applyfiltersformesh
			Internal modifier
		*/
		else if (str_starts_with(part_of_name, "[applyfiltersformesh"))
		{
			/* IMPORTANT: When changing this, getTextureForMesh() needs to be
			 * updated too. */

			CHECK_BASEIMG();

			/* Apply the "clean transparent" filter, if necessary
			 * This is needed since filtering will sample parts of the image
			 * that are transparent and PNG optimizers often discard the color
			 * information in those parts. */
			if (m_setting_mipmap || m_setting_bilinear_filter ||
				m_setting_trilinear_filter || m_setting_anisotropic_filter) {
				/* Note: in theory we should pass either 0 or 127 depending on
				 * if the texture is used with an ALPHA or ALPHA_REF material,
				 * however we don't have this information here.
				 * It doesn't matter in practice. */
				imageCleanTransparent(baseimg, 0);
			}

			/* Upscale textures to user's requested minimum size.  This is a trick to make
			 * filters look as good on low-res textures as on high-res ones, by making
			 * low-res textures BECOME high-res ones. This is helpful for worlds that
			 * mix high- and low-res textures, or for mods with least-common-denominator
			 * textures that don't have the resources to offer high-res alternatives.
			 *
			 * Q: why not just enable/disable filtering depending on texture size?
			 * A: large texture resolutions apparently allow getting rid of the Moire
			 *    effect way better than anisotropic filtering alone could.
			 *    see <https://github.com/luanti-org/luanti/issues/15604> and related
			 *    linked discussions.
			 */
			const bool filter = m_setting_trilinear_filter || m_setting_bilinear_filter;
			if (filter) {
				const f32 scaleto = rangelim(g_settings->getU16("texture_min_size"),
					TEXTURE_FILTER_MIN_SIZE, 16384);

				/* Calculate integer-scaling needed to make the shortest texture
				 * dimension equal to the target minimum. If this is e.g. a
				 * vertical frames animation, the short dimension will be the real size.
				 */
				const auto &dim = baseimg->getDimension();
				u32 xscale = std::ceil(scaleto / dim.Width);
				u32 yscale = std::ceil(scaleto / dim.Height);
				const s32 scale = std::max(xscale, yscale);

				// Never downscale; only scale up by 2x or more.
				if (scale > 1) {
					u32 w = scale * dim.Width;
					u32 h = scale * dim.Height;
					const core::dimension2d<u32> newdim(w, h);
					if (w <= MAX_IMAGE_DIMENSION && h <= MAX_IMAGE_DIMENSION) {
						video::IImage *newimg = driver->createImage(
								baseimg->getColorFormat(), newdim);
						baseimg->copyToScaling(newimg);
						baseimg->drop();
						baseimg = newimg;
					}
				}
			}
		}
		/*
			[resize:WxH
			Resizes the base image to the given dimensions
		*/
		else if (str_starts_with(part_of_name, "[resize"))
		{
			CHECK_BASEIMG();

			Strfnd sf(part_of_name);
			sf.next(":");
			u32 width = stoi(sf.next("x"));
			u32 height = stoi(sf.next(""));
			CHECK_DIM(width, height);

			video::IImage *image = driver->
				createImage(video::ECF_A8R8G8B8, {width, height});
			baseimg->copyToScaling(image);
			baseimg->drop();
			baseimg = image;
		}
		/*
			[opacity:R
			Makes the base image transparent according to the given ratio.
			R must be between 0 and 255.
			0 means totally transparent.
			255 means totally opaque.
		*/
		else if (str_starts_with(part_of_name, "[opacity:")) {
			CHECK_BASEIMG();

			Strfnd sf(part_of_name);
			sf.next(":");

			u32 ratio = mystoi(sf.next(""), 0, 255);

			core::dimension2d<u32> dim = baseimg->getDimension();

			for (u32 y = 0; y < dim.Height; y++)
			for (u32 x = 0; x < dim.Width; x++)
			{
				video::SColor c = baseimg->getPixel(x, y);
				c.setAlpha(floor((c.getAlpha() * ratio) / 255 + 0.5));
				baseimg->setPixel(x, y, c);
			}
		}
		/*
			[invert:mode
			Inverts the given channels of the base image.
			Mode may contain the characters "r", "g", "b", "a".
			Only the channels that are mentioned in the mode string
			will be inverted.
		*/
		else if (str_starts_with(part_of_name, "[invert:")) {
			CHECK_BASEIMG();

			Strfnd sf(part_of_name);
			sf.next(":");

			std::string mode = sf.next("");
			u32 mask = 0;
			if (mode.find('a') != std::string::npos)
				mask |= 0xff000000UL;
			if (mode.find('r') != std::string::npos)
				mask |= 0x00ff0000UL;
			if (mode.find('g') != std::string::npos)
				mask |= 0x0000ff00UL;
			if (mode.find('b') != std::string::npos)
				mask |= 0x000000ffUL;

			core::dimension2d<u32> dim = baseimg->getDimension();

			for (u32 y = 0; y < dim.Height; y++)
			for (u32 x = 0; x < dim.Width; x++)
			{
				video::SColor c = baseimg->getPixel(x, y);
				c.color ^= mask;
				baseimg->setPixel(x, y, c);
			}
		}
		/*
			[sheet:WxH:X,Y
			Retrieves a tile at position X,Y (in tiles)
			from the base image it assumes to be a
			tilesheet with dimensions W,H (in tiles).
		*/
		else if (str_starts_with(part_of_name, "[sheet:")) {
			CHECK_BASEIMG();

			Strfnd sf(part_of_name);
			sf.next(":");
			u32 w0 = stoi(sf.next("x"));
			u32 h0 = stoi(sf.next(":"));
			u32 x0 = stoi(sf.next(","));
			u32 y0 = stoi(sf.next(":"));

			CHECK_DIM(w0, h0);
			if (x0 >= w0 || y0 >= h0)
				COMPLAIN_INVALID("tile position (X,Y)");

			core::dimension2d<u32> img_dim = baseimg->getDimension();
			core::dimension2d<u32> tile_dim(v2u32(img_dim) / v2u32(w0, h0));
			if (tile_dim.Width == 0)
				tile_dim.Width = 1;
			if (tile_dim.Height == 0)
				tile_dim.Height = 1;

			video::IImage *img = driver->createImage(
					video::ECF_A8R8G8B8, tile_dim);

			v2u32 vdim(tile_dim);
			core::rect<s32> rect(v2s32(x0 * vdim.X, y0 * vdim.Y), tile_dim);
			baseimg->copyTo(img, v2s32(0), rect, nullptr);

			// Replace baseimg
			baseimg->drop();
			baseimg = img;
		}
		/*
			[png:base64
			Decodes a PNG image in base64 form.
			Use minetest.encode_png and minetest.encode_base64
			to produce a valid string.
		*/
		else if (str_starts_with(part_of_name, "[png:")) {
			std::string png;
			{
				auto blob = part_of_name.substr(5);
				if (!base64_is_valid(blob)) {
					errorstream << "generateImagePart(): "
								<< "malformed base64 in [png" << std::endl;
					return false;
				}
				png = base64_decode(blob);
			}

			auto *device = RenderingEngine::get_raw_device();
			auto *fs = device->getFileSystem();
			auto *vd = device->getVideoDriver();
			auto *memfile = fs->createMemoryReadFile(png.data(), png.size(), "[png_tmpfile");
			video::IImage* pngimg = vd->createImageFromFile(memfile);
			memfile->drop();

			if (!pngimg) {
				errorstream << "generateImagePart(): Invalid PNG data" << std::endl;
				return false;
			}

			// blit or use as base
			if (baseimg) {
				blitBaseImage(pngimg, baseimg);
				pngimg->drop();
			} else if (pngimg->getColorFormat() != video::ECF_A8R8G8B8) {
				baseimg = driver->createImage(video::ECF_A8R8G8B8, pngimg->getDimension());
				pngimg->copyTo(baseimg);
				pngimg->drop();
			} else {
				baseimg = pngimg;
			}
		}
		/*
			[hsl:hue:saturation:lightness
			or
			[colorizehsl:hue:saturation:lightness

			Adjust the hue, saturation, and lightness of the base image. Like
			"Hue-Saturation" in GIMP, but with 0 as the mid-point.
			Hue should be from -180 to +180, though 0 to 360 is also supported.
			Saturation and lightness are optional, with lightness from -100 to
			+100, and sauration from -100 to +100-or-higher.

			If colorize is true then saturation is from 0 to 100, and the image
			will be converted to a grayscale image as though seen through a
			colored glass, like	"Colorize" in GIMP.
		*/
		else if (str_starts_with(part_of_name, "[hsl:") ||
		         str_starts_with(part_of_name, "[colorizehsl:")) {

			CHECK_BASEIMG();

			bool colorize = str_starts_with(part_of_name, "[colorizehsl:");

			// saturation range is 0 to 100 when colorize is true
			s32 defaultSaturation = colorize ? 50 : 0;

			Strfnd sf(part_of_name);
			sf.next(":");
			s32 hue = mystoi(sf.next(":"), -180, 360);
			s32 saturation = sf.at_end() ? defaultSaturation : mystoi(sf.next(":"), -100, 1000);
			s32 lightness  = sf.at_end() ? 0 : mystoi(sf.next(":"), -100, 100);


			apply_hue_saturation(baseimg, v2u32(0, 0), baseimg->getDimension(),
				hue, saturation, lightness, colorize);
		}
		/*
			[overlay:filename
			or
			[hardlight:filename

			"A.png^[hardlight:B.png" is the same as "B.png^[overlay:A.Png"

			Applies an Overlay or Hard Light blend between two images, like the
			layer modes of the same names in GIMP.
			Overlay combines Multiply and Screen blend modes. The parts of the
			top layer where the base layer is light become lighter, the parts
			where the base layer is dark become darker. Areas where the base
			layer are mid grey are unaffected. An overlay with the same picture
			looks like an S-curve.

			Swapping the top layer and base layer is a Hard Light blend
		*/
		else if (str_starts_with(part_of_name, "[overlay:") ||
		         str_starts_with(part_of_name, "[hardlight:")) {

			CHECK_BASEIMG();

			Strfnd sf(part_of_name);
			sf.next(":");
			std::string filename = unescape_string(sf.next_esc(":", escape), escape);

			video::IImage *img = generateImage(filename, source_image_names);
			if (img) {
				upscaleImagesToMatchLargest(baseimg, img);

				bool hardlight = str_starts_with(part_of_name, "[hardlight:");
				apply_overlay(img, baseimg, v2s32(0, 0), v2s32(0, 0),
						img->getDimension(), hardlight);
				img->drop();
			} else {
				errorstream << "generateImage(): Failed to load image \""
					<< filename << "\" for [overlay or [hardlight" << std::endl;
			}
		}
		/*
			[contrast:C:B

			Adjust the brightness and contrast of the base image. Conceptually
			like GIMP's "Brightness-Contrast" feature but allows brightness to
			be wound all the way up to white or down to black.
			C and B are both values from -127 to +127.
			B is optional.
		*/
		else if (str_starts_with(part_of_name, "[contrast:")) {

			CHECK_BASEIMG();

			Strfnd sf(part_of_name);
			sf.next(":");
			s32 contrast = mystoi(sf.next(":"), -127, 127);
			s32 brightness = sf.at_end() ? 0 : mystoi(sf.next(":"), -127, 127);

			apply_brightness_contrast(baseimg, v2u32(0, 0),
				baseimg->getDimension(), brightness, contrast);
		}
		else
		{
			errorstream << "generateImagePart(): Invalid "
					" modification: \"" << part_of_name << "\"" << std::endl;
		}
	}

	return true;
}

#undef CHECK_BASEIMG

#undef COMPLAIN_INVALID

#undef CHECK_DIM

ImageSource::ImageSource() :
		m_setting_mipmap{g_settings->getBool("mip_map")},
		m_setting_trilinear_filter{g_settings->getBool("trilinear_filter")},
		m_setting_bilinear_filter{g_settings->getBool("bilinear_filter")},
		m_setting_anisotropic_filter{g_settings->getBool("anisotropic_filter")}
{}

video::IImage* ImageSource::generateImage(std::string_view name,
		std::set<std::string> &source_image_names)
{
	// Get the base image

	const char separator = '^';
	const char escape = '\\';
	const char paren_open = '(';
	const char paren_close = ')';

	// Find last separator in the name
	s32 last_separator_pos = -1;
	u8 paren_bal = 0;
	for (s32 i = name.size() - 1; i >= 0; i--) {
		if (i > 0 && name[i-1] == escape)
			continue;
		switch (name[i]) {
		case separator:
			if (paren_bal == 0) {
				last_separator_pos = i;
				i = -1; // break out of loop
			}
			break;
		case paren_open:
			if (paren_bal == 0) {
				errorstream << "generateImage(): unbalanced parentheses"
						<< "(extranous '(') while generating texture \""
						<< name << "\"" << std::endl;
				return NULL;
			}
			paren_bal--;
			break;
		case paren_close:
			paren_bal++;
			break;
		default:
			break;
		}
	}
	if (paren_bal > 0) {
		errorstream << "generateImage(): unbalanced parentheses"
				<< "(missing matching '(') while generating texture \""
				<< name << "\"" << std::endl;
		return NULL;
	}


	video::IImage *baseimg = NULL;

	/*
		If separator was found, make the base image
		using a recursive call.
	*/
	if (last_separator_pos != -1) {
		baseimg = generateImage(name.substr(0, last_separator_pos), source_image_names);
	}

	/*
		Parse out the last part of the name of the image and act
		according to it
	*/

	auto last_part_of_name = name.substr(last_separator_pos + 1);

	/*
		If this name is enclosed in parentheses, generate it
		and blit it onto the base image
	*/
	if (last_part_of_name.empty()) {
		// keep baseimg == nullptr
	} else if (last_part_of_name[0] == paren_open
			&& last_part_of_name.back() == paren_close) {
		auto name2 = last_part_of_name.substr(1,
				last_part_of_name.size() - 2);
		video::IImage *tmp = generateImage(name2, source_image_names);
		if (!tmp) {
			errorstream << "generateImage(): "
				"Failed to generate \"" << name2 << "\"\n"
				"part of texture \"" << name << "\""
				<< std::endl;
			return NULL;
		}

		if (baseimg) {
			core::dimension2d<u32> dim = tmp->getDimension();
			blit_with_alpha(tmp, baseimg, v2s32(0, 0), dim);
			tmp->drop();
		} else {
			baseimg = tmp;
		}
	} else if (!generateImagePart(last_part_of_name, baseimg, source_image_names)) {
		// Generate image according to part of name
		errorstream << "generateImage(): "
				"Failed to generate \"" << last_part_of_name << "\"\n"
				"part of texture \"" << name << "\""
				<< std::endl;
	}

	// If no resulting image, print a warning
	if (!baseimg) {
		errorstream << "generateImage(): baseimg is NULL (attempted to"
				" create texture \"" << name << "\")" << std::endl;
	} else if (baseimg->getDimension().Width == 0 ||
			baseimg->getDimension().Height == 0) {
		errorstream << "generateImage(): zero-sized image was created?! "
			"(attempted to create texture \"" << name << "\")" << std::endl;
		baseimg->drop();
		baseimg = nullptr;
	}

	return baseimg;
}

void ImageSource::insertSourceImage(const std::string &name, video::IImage *img, bool prefer_local)
{
	m_sourcecache.insert(name, img, prefer_local);
}
