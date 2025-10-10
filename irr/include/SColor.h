// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
#include "irrMath.h"

namespace video
{
//! An enum for the color format of textures used by the Irrlicht Engine.
/** A color format specifies how color information is stored.
	NOTE: Byte order in memory is usually flipped (it's probably correct in bitmap files, but flipped on reading).
	So for example ECF_A8R8G8B8 is BGRA in memory same as in DX9's D3DFMT_A8R8G8B8 format.
*/
enum ECOLOR_FORMAT
{
	//! 16 bit color format used by the software driver.
	/** It is thus preferred by all other irrlicht engine video drivers.
	There are 5 bits for every color component, and a single bit is left
	for alpha information. */
	ECF_A1R5G5B5 = 0,

	//! Standard 16 bit color format.
	ECF_R5G6B5,

	//! 24 bit color, no alpha channel, but 8 bit for red, green and blue.
	//! Warning: 24 bit formats tend to be badly supported. Depending on driver it's usually converted to another
	//           format or even not working at all. It's mostly better to use 16-bit or 32-bit formats.
	ECF_R8G8B8,

	//! Default 32 bit color format. 8 bits are used for every component: red, green, blue and alpha.
	//! Warning: This tends to be BGRA in memory (it's ARGB on file, but with usual big-endian memory it's flipped)
	ECF_A8R8G8B8,

	/** The following formats may only be used for render target textures. */

	/** Floating point formats. */

	//! 16 bit format using 16 bits for the red channel.
	ECF_R16F,

	//! 32 bit format using 16 bits for the red and green channels.
	ECF_G16R16F,

	//! 64 bit format using 16 bits for the red, green, blue and alpha channels.
	ECF_A16B16G16R16F,

	//! 32 bit format using 32 bits for the red channel.
	ECF_R32F,

	//! 64 bit format using 32 bits for the red and green channels.
	ECF_G32R32F,

	//! 128 bit format using 32 bits for the red, green, blue and alpha channels.
	ECF_A32B32G32R32F,

	/** Unsigned normalized integer formats. */

	//! 8 bit format using 8 bits for the red channel.
	ECF_R8,

	//! 16 bit format using 8 bits for the red and green channels.
	ECF_R8G8,

	//! 16 bit format using 16 bits for the red channel.
	ECF_R16,

	//! 32 bit format using 16 bits for the red and green channels.
	ECF_R16G16,

	//! 32 bit format using 10 bits for R, G, B and 2 for alpha.
	ECF_A2R10G10B10,

	/** Depth and stencil formats. */

	//! 16 bit format using 16 bits for depth.
	ECF_D16,

	//! 32 bit(?) format using 24 bits for depth.
	ECF_D24,

	//! 32 bit format using 32 bits for depth.
	ECF_D32,

	//! 32 bit format using 24 bits for depth and 8 bits for stencil.
	ECF_D24S8,

	//! Unknown color format:
	ECF_UNKNOWN
};

//! Names for ECOLOR_FORMAT types
const c8 *const ColorFormatNames[] = {
		"A1R5G5B5",
		"R5G6B5",
		"R8G8B8",
		"A8R8G8B8",
		"R16F",
		"G16R16F",
		"A16B16G16R16F",
		"R32F",
		"G32R32F",
		"A32B32G32R32F",
		"R8",
		"R8G8",
		"R16",
		"R16G16",
		"A2R10G10B10",
		"D16",
		"D24",
		"D32",
		"D24S8",
		"UNKNOWN",
		0,
	};

static_assert(sizeof(ColorFormatNames) / sizeof(ColorFormatNames[0])
	== ECF_UNKNOWN + 2, "name table size mismatch");

inline const c8 *ColorFormatName(ECOLOR_FORMAT format)
{
	return ColorFormatNames[format < ECF_UNKNOWN ? format : ECF_UNKNOWN];
}

//! Creates a 16 bit A1R5G5B5 color
inline u16 RGBA16(u32 r, u32 g, u32 b, u32 a = 0xFF)
{
	return (u16)((a & 0x80) << 8 |
				 (r & 0xF8) << 7 |
				 (g & 0xF8) << 2 |
				 (b & 0xF8) >> 3);
}

//! Creates a 16 bit A1R5G5B5 color
inline u16 RGB16(u32 r, u32 g, u32 b)
{
	return RGBA16(r, g, b);
}

//! Creates a 16bit A1R5G5B5 color, based on 16bit input values
inline u16 RGB16from16(u16 r, u16 g, u16 b)
{
	return (0x8000 |
			(r & 0x1F) << 10 |
			(g & 0x1F) << 5 |
			(b & 0x1F));
}

//! Converts a 32bit (X8R8G8B8) color to a 16bit A1R5G5B5 color
inline u16 X8R8G8B8toA1R5G5B5(u32 color)
{
	return (u16)(0x8000 |
				 (color & 0x00F80000) >> 9 |
				 (color & 0x0000F800) >> 6 |
				 (color & 0x000000F8) >> 3);
}

//! Converts a 32bit (A8R8G8B8) color to a 16bit A1R5G5B5 color
inline u16 A8R8G8B8toA1R5G5B5(u32 color)
{
	return (u16)((color & 0x80000000) >> 16 |
				 (color & 0x00F80000) >> 9 |
				 (color & 0x0000F800) >> 6 |
				 (color & 0x000000F8) >> 3);
}

//! Converts a 32bit (A8R8G8B8) color to a 16bit R5G6B5 color
inline u16 A8R8G8B8toR5G6B5(u32 color)
{
	return (u16)((color & 0x00F80000) >> 8 |
				 (color & 0x0000FC00) >> 5 |
				 (color & 0x000000F8) >> 3);
}

//! Convert A8R8G8B8 Color from A1R5G5B5 color
/** build a nicer 32bit Color by extending dest lower bits with source high bits. */
inline u32 A1R5G5B5toA8R8G8B8(u16 color)
{
	return (((-((s32)color & 0x00008000) >> (s32)31) & 0xFF000000) |
			((color & 0x00007C00) << 9) | ((color & 0x00007000) << 4) |
			((color & 0x000003E0) << 6) | ((color & 0x00000380) << 1) |
			((color & 0x0000001F) << 3) | ((color & 0x0000001C) >> 2));
}

//! Returns A8R8G8B8 Color from R5G6B5 color
inline u32 R5G6B5toA8R8G8B8(u16 color)
{
	return 0xFF000000 |
		   ((color & 0xF800) << 8) |
		   ((color & 0x07E0) << 5) |
		   ((color & 0x001F) << 3);
}

//! Returns A1R5G5B5 Color from R5G6B5 color
inline u16 R5G6B5toA1R5G5B5(u16 color)
{
	return 0x8000 | (((color & 0xFFC0) >> 1) | (color & 0x1F));
}

//! Returns R5G6B5 Color from A1R5G5B5 color
inline u16 A1R5G5B5toR5G6B5(u16 color)
{
	return (((color & 0x7FE0) << 1) | (color & 0x1F));
}

//! Returns the alpha component from A1R5G5B5 color
/** In Irrlicht, alpha refers to opacity.
\return The alpha value of the color. 0 is transparent, 1 is opaque. */
inline u32 getAlpha(u16 color)
{
	return ((color >> 15) & 0x1);
}

//! Returns the red component from A1R5G5B5 color.
/** Shift left by 3 to get 8 bit value. */
inline u32 getRed(u16 color)
{
	return ((color >> 10) & 0x1F);
}

//! Returns the green component from A1R5G5B5 color
/** Shift left by 3 to get 8 bit value. */
inline u32 getGreen(u16 color)
{
	return ((color >> 5) & 0x1F);
}

//! Returns the blue component from A1R5G5B5 color
/** Shift left by 3 to get 8 bit value. */
inline u32 getBlue(u16 color)
{
	return (color & 0x1F);
}

//! Class representing a 32 bit ARGB color.
/** The color values for alpha, red, green, and blue are
stored in a single u32. So all four values may be between 0 and 255.
Alpha in Irrlicht is opacity, so 0 is fully transparent, 255 is fully opaque (solid).
This class is used by most parts of the Irrlicht Engine
to specify a color. Another way is using the class SColorf, which
stores the color values in 4 floats.
This class must consist of only one u32 and must not use virtual functions.
*/
class SColor
{
public:
	//! Constructor of the Color. Does nothing.
	/** The color value is not initialized to save time. */
	SColor() {}

	//! Constructs the color from 4 values representing the alpha, red, green and blue component.
	/** Must be values between 0 and 255. */
	constexpr SColor(u32 a, u32 r, u32 g, u32 b) :
			color(((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff)) {}

	//! Constructs the color from a 32 bit value. Could be another color.
	constexpr SColor(u32 clr) :
			color(clr) {}

	//! Returns the alpha component of the color.
	/** The alpha component defines how opaque a color is.
	\return The alpha value of the color. 0 is fully transparent, 255 is fully opaque. */
	u32 getAlpha() const { return color >> 24; }

	//! Returns the red component of the color.
	/** \return Value between 0 and 255, specifying how red the color is.
	0 means no red, 255 means full red. */
	u32 getRed() const { return (color >> 16) & 0xff; }

	//! Returns the green component of the color.
	/** \return Value between 0 and 255, specifying how green the color is.
	0 means no green, 255 means full green. */
	u32 getGreen() const { return (color >> 8) & 0xff; }

	//! Returns the blue component of the color.
	/** \return Value between 0 and 255, specifying how blue the color is.
	0 means no blue, 255 means full blue. */
	u32 getBlue() const { return color & 0xff; }

	//! Get an approximate brightness value of the color in the range [0,255]
	f32 getBrightness() const
	{
		return 0.3f * getRed() + 0.59f * getGreen() + 0.11f * getBlue();
	}

	//! Sets the alpha component of the Color.
	/** The alpha component defines how transparent a color should be.
	\param a The alpha value of the color. 0 is fully transparent, 255 is fully opaque. */
	void setAlpha(u32 a) { color = ((a & 0xff) << 24) | (color & 0x00ffffff); }

	//! Sets the red component of the Color.
	/** \param r: Has to be a value between 0 and 255.
	0 means no red, 255 means full red. */
	void setRed(u32 r) { color = ((r & 0xff) << 16) | (color & 0xff00ffff); }

	//! Sets the green component of the Color.
	/** \param g: Has to be a value between 0 and 255.
	0 means no green, 255 means full green. */
	void setGreen(u32 g) { color = ((g & 0xff) << 8) | (color & 0xffff00ff); }

	//! Sets the blue component of the Color.
	/** \param b: Has to be a value between 0 and 255.
	0 means no blue, 255 means full blue. */
	void setBlue(u32 b) { color = (b & 0xff) | (color & 0xffffff00); }

	//! Calculates a 16 bit A1R5G5B5 value of this color.
	/** \return 16 bit A1R5G5B5 value of this color. */
	u16 toA1R5G5B5() const { return A8R8G8B8toA1R5G5B5(color); }

	//! Converts color to OpenGL color format
	/** From ARGB to RGBA in 4 byte components for endian aware
	passing to OpenGL
	\param dest: address where the 4x8 bit OpenGL color is stored. */
	void toOpenGLColor(u8 *dest) const
	{
		*dest = (u8)getRed();
		*++dest = (u8)getGreen();
		*++dest = (u8)getBlue();
		*++dest = (u8)getAlpha();
	}

	//! Sets all four components of the color at once.
	/** Constructs the color from 4 values representing the alpha,
	red, green and blue components of the color. Must be values
	between 0 and 255.
	\param a: Alpha component of the color. The alpha component
	defines how transparent a color should be. Has to be a value
	between 0 and 255. 255 means not transparent (opaque), 0 means
	fully transparent.
	\param r: Sets the red component of the Color. Has to be a
	value between 0 and 255. 0 means no red, 255 means full red.
	\param g: Sets the green component of the Color. Has to be a
	value between 0 and 255. 0 means no green, 255 means full
	green.
	\param b: Sets the blue component of the Color. Has to be a
	value between 0 and 255. 0 means no blue, 255 means full blue. */
	void set(u32 a, u32 r, u32 g, u32 b)
	{
		color = (((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff));
	}
	void set(u32 col) { color = col; }

	//! Compares the color to another color.
	/** \return True if the colors are the same, and false if not. */
	bool operator==(const SColor &other) const { return other.color == color; }

	//! Compares the color to another color.
	/** \return True if the colors are different, and false if they are the same. */
	bool operator!=(const SColor &other) const { return other.color != color; }

	//! comparison operator
	/** \return True if this color is smaller than the other one */
	bool operator<(const SColor &other) const { return (color < other.color); }

	//! Adds two colors in a gamma-incorrect way
	/** \param other Color to add to this color
	\return Sum of the two non-linear colors, clamped to 0..255 values */
	SColor operator+(const SColor &other) const
	{
		return SColor(core::min_(getAlpha() + other.getAlpha(), 255u),
				core::min_(getRed() + other.getRed(), 255u),
				core::min_(getGreen() + other.getGreen(), 255u),
				core::min_(getBlue() + other.getBlue(), 255u));
	}

	//! Interpolates the color with a f32 value to another color
	/** Note that the interpolation is neither physically nor perceptually
	linear since it happens directly in the sRGB color space.
	\param other: Other color
	\param d: value between 0.0f and 1.0f. d=0 returns other, d=1 returns this, values between interpolate.
	\return Interpolated color. */
	SColor getInterpolated(const SColor &other, f32 d) const
	{
		d = core::clamp(d, 0.f, 1.f);
		const f32 inv = 1.0f - d;
		return SColor((u32)core::round32(other.getAlpha() * inv + getAlpha() * d),
				(u32)core::round32(other.getRed() * inv + getRed() * d),
				(u32)core::round32(other.getGreen() * inv + getGreen() * d),
				(u32)core::round32(other.getBlue() * inv + getBlue() * d));
	}

	//! set the color by expecting data in the given format
	/** \param data: must point to valid memory containing color information in the given format
		\param format: tells the format in which data is available
	*/
	void setData(const void *data, ECOLOR_FORMAT format)
	{
		switch (format) {
		case ECF_A1R5G5B5:
			color = A1R5G5B5toA8R8G8B8(*(u16 *)data);
			break;
		case ECF_R5G6B5:
			color = R5G6B5toA8R8G8B8(*(u16 *)data);
			break;
		case ECF_A8R8G8B8:
			color = *(u32 *)data;
			break;
		case ECF_R8G8B8: {
			const u8 *p = (u8 *)data;
			set(255, p[0], p[1], p[2]);
		} break;
		default:
			color = 0xffffffff;
			break;
		}
	}

	//! Write the color to data in the defined format
	/** \param data: target to write the color. Must contain sufficiently large memory to receive the number of bytes neede for format
		\param format: tells the format used to write the color into data
	*/
	void getData(void *data, ECOLOR_FORMAT format) const
	{
		switch (format) {
		case ECF_A1R5G5B5: {
			u16 *dest = (u16 *)data;
			*dest = video::A8R8G8B8toA1R5G5B5(color);
		} break;

		case ECF_R5G6B5: {
			u16 *dest = (u16 *)data;
			*dest = video::A8R8G8B8toR5G6B5(color);
		} break;

		case ECF_R8G8B8: {
			u8 *dest = (u8 *)data;
			dest[0] = (u8)getRed();
			dest[1] = (u8)getGreen();
			dest[2] = (u8)getBlue();
		} break;

		case ECF_A8R8G8B8: {
			u32 *dest = (u32 *)data;
			*dest = color;
		} break;

		default:
			break;
		}
	}

	//! color in A8R8G8B8 Format
	u32 color;
};

//! Class representing a color with four floats.
/** The color values for red, green, blue
and alpha are each stored in a 32 bit floating point variable.
So all four values may be between 0.0f and 1.0f.
Another, faster way to define colors is using the class SColor, which
stores the color values in a single 32 bit integer.
*/
class SColorf
{
public:
	//! Default constructor for SColorf.
	/** Sets red, green and blue to 0.0f and alpha to 1.0f. */
	SColorf() :
			r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}

	//! Constructs a color from up to four color values: red, green, blue, and alpha.
	/** \param r: Red color component. Should be a value between
	0.0f meaning no red and 1.0f, meaning full red.
	\param g: Green color component. Should be a value between 0.0f
	meaning no green and 1.0f, meaning full green.
	\param b: Blue color component. Should be a value between 0.0f
	meaning no blue and 1.0f, meaning full blue.
	\param a: Alpha color component of the color. The alpha
	component defines how transparent a color should be. Has to be
	a value between 0.0f and 1.0f, 1.0f means not transparent
	(opaque), 0.0f means fully transparent. */
	SColorf(f32 r, f32 g, f32 b, f32 a = 1.0f) :
			r(r), g(g), b(b), a(a) {}

	//! Constructs a color from 32 bit Color without gamma correction
	/** \param c: 32 bit color from which this SColorf class is
	constructed from. */
	SColorf(SColor c)
	{
		const f32 inv = 1.0f / 255.0f;
		r = c.getRed() * inv;
		g = c.getGreen() * inv;
		b = c.getBlue() * inv;
		a = c.getAlpha() * inv;
	}

	//! Converts this color to a SColor without gamma correction
	SColor toSColor() const
	{
		return SColor((u32)core::round32(a * 255.0f), (u32)core::round32(r * 255.0f), (u32)core::round32(g * 255.0f), (u32)core::round32(b * 255.0f));
	}

	//! Sets three color components to new values at once.
	/** \param rr: Red color component. Should be a value between 0.0f meaning
	no red (=black) and 1.0f, meaning full red.
	\param gg: Green color component. Should be a value between 0.0f meaning
	no green (=black) and 1.0f, meaning full green.
	\param bb: Blue color component. Should be a value between 0.0f meaning
	no blue (=black) and 1.0f, meaning full blue. */
	void set(f32 rr, f32 gg, f32 bb)
	{
		r = rr;
		g = gg;
		b = bb;
	}

	//! Sets all four color components to new values at once.
	/** \param aa: Alpha component. Should be a value between 0.0f meaning
	fully transparent and 1.0f, meaning opaque.
	\param rr: Red color component. Should be a value between 0.0f meaning
	no red and 1.0f, meaning full red.
	\param gg: Green color component. Should be a value between 0.0f meaning
	no green and 1.0f, meaning full green.
	\param bb: Blue color component. Should be a value between 0.0f meaning
	no blue and 1.0f, meaning full blue. */
	void set(f32 aa, f32 rr, f32 gg, f32 bb)
	{
		a = aa;
		r = rr;
		g = gg;
		b = bb;
	}

	//! Interpolates the color with a f32 value to another color
	/** Note that the interpolation is neither physically nor perceptually
	linear if it happens directly in the sRGB color space.
	\param other: Other color
	\param d: value between 0.0f and 1.0f
	\return Interpolated color. */
	SColorf getInterpolated(const SColorf &other, f32 d) const
	{
		d = core::clamp(d, 0.f, 1.f);
		const f32 inv = 1.0f - d;
		return SColorf(other.r * inv + r * d,
				other.g * inv + g * d, other.b * inv + b * d, other.a * inv + a * d);
	}

	//! Returns the alpha component of the color in the range 0.0 (transparent) to 1.0 (opaque)
	f32 getAlpha() const { return a; }

	//! Returns the red component of the color in the range 0.0 to 1.0
	f32 getRed() const { return r; }

	//! Returns the green component of the color in the range 0.0 to 1.0
	f32 getGreen() const { return g; }

	//! Returns the blue component of the color in the range 0.0 to 1.0
	f32 getBlue() const { return b; }

	//! red color component
	f32 r;

	//! green color component
	f32 g;

	//! blue component
	f32 b;

	//! alpha color component
	f32 a;
};

//! Class representing a color in HSL format
/** The color values for hue, saturation, luminance
are stored in 32bit floating point variables. Hue is in range [0,360],
Luminance and Saturation are in percent [0,100]
*/
class SColorHSL
{
public:
	constexpr SColorHSL(f32 h = 0.f, f32 s = 0.f, f32 l = 0.f) :
			Hue(h), Saturation(s), Luminance(l) {}

	void fromRGB(const SColorf &color);
	void toRGB(SColorf &color) const;

	f32 Hue;
	f32 Saturation;
	f32 Luminance;

private:
	inline f32 toRGB1(f32 rm1, f32 rm2, f32 rh) const;
};

inline void SColorHSL::fromRGB(const SColorf &color)
{
	const f32 maxVal = core::max_(color.getRed(), color.getGreen(), color.getBlue());
	const f32 minVal = (f32)core::min_(color.getRed(), color.getGreen(), color.getBlue());
	Luminance = (maxVal + minVal) * 50;
	if (core::equals(maxVal, minVal)) {
		Hue = 0.f;
		Saturation = 0.f;
		return;
	}

	const f32 delta = maxVal - minVal;
	if (Luminance <= 50) {
		Saturation = (delta) / (maxVal + minVal);
	} else {
		Saturation = (delta) / (2 - maxVal - minVal);
	}
	Saturation *= 100;

	if (core::equals(maxVal, color.getRed()))
		Hue = (color.getGreen() - color.getBlue()) / delta;
	else if (core::equals(maxVal, color.getGreen()))
		Hue = 2 + ((color.getBlue() - color.getRed()) / delta);
	else // blue is max
		Hue = 4 + ((color.getRed() - color.getGreen()) / delta);

	Hue *= 60.0f;
	while (Hue < 0.f)
		Hue += 360;
}

inline void SColorHSL::toRGB(SColorf &color) const
{
	const f32 l = Luminance / 100;
	if (core::iszero(Saturation)) { // grey
		color.set(l, l, l);
		return;
	}

	f32 rm2;

	if (Luminance <= 50) {
		rm2 = l + l * (Saturation / 100);
	} else {
		rm2 = l + (1 - l) * (Saturation / 100);
	}

	const f32 rm1 = 2.0f * l - rm2;

	const f32 h = Hue / 360.0f;
	color.set(toRGB1(rm1, rm2, h + 1.f / 3.f),
			toRGB1(rm1, rm2, h),
			toRGB1(rm1, rm2, h - 1.f / 3.f));
}

// algorithm from Foley/Van-Dam
inline f32 SColorHSL::toRGB1(f32 rm1, f32 rm2, f32 rh) const
{
	if (rh < 0)
		rh += 1;
	if (rh > 1)
		rh -= 1;

	if (rh < 1.f / 6.f)
		rm1 = rm1 + (rm2 - rm1) * rh * 6.f;
	else if (rh < 0.5f)
		rm1 = rm2;
	else if (rh < 2.f / 3.f)
		rm1 = rm1 + (rm2 - rm1) * ((2.f / 3.f) - rh) * 6.f;

	return rm1;
}

} // end namespace video
