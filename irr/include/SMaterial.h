// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "SColor.h"
#include "matrix4.h"
#include "irrMath.h"
#include "EMaterialTypes.h" // IWYU pragma: export
#include "EMaterialProps.h" // IWYU pragma: export
#include "SMaterialLayer.h"

namespace video
{
class ITexture;

//! Flag for MaterialTypeParam (in combination with EMT_ONETEXTURE_BLEND) or for BlendFactor
//! BlendFunc = source * sourceFactor + dest * destFactor
enum E_BLEND_FACTOR : u8
{
	EBF_ZERO = 0,            //!< src & dest	(0, 0, 0, 0)
	EBF_ONE,                 //!< src & dest	(1, 1, 1, 1)
	EBF_DST_COLOR,           //!< src	(destR, destG, destB, destA)
	EBF_ONE_MINUS_DST_COLOR, //!< src	(1-destR, 1-destG, 1-destB, 1-destA)
	EBF_SRC_COLOR,           //!< dest	(srcR, srcG, srcB, srcA)
	EBF_ONE_MINUS_SRC_COLOR, //!< dest	(1-srcR, 1-srcG, 1-srcB, 1-srcA)
	EBF_SRC_ALPHA,           //!< src & dest	(srcA, srcA, srcA, srcA)
	EBF_ONE_MINUS_SRC_ALPHA, //!< src & dest	(1-srcA, 1-srcA, 1-srcA, 1-srcA)
	EBF_DST_ALPHA,           //!< src & dest	(destA, destA, destA, destA)
	EBF_ONE_MINUS_DST_ALPHA, //!< src & dest	(1-destA, 1-destA, 1-destA, 1-destA)
	EBF_SRC_ALPHA_SATURATE   //!< src	(min(srcA, 1-destA), idem, ...)
};

//! Values defining the blend operation
enum E_BLEND_OPERATION : u8
{
	EBO_NONE = 0,    //!< No blending happens
	EBO_ADD,         //!< Default blending adds the color values
	EBO_SUBTRACT,    //!< This mode subtracts the color values
	EBO_REVSUBTRACT, //!< This modes subtracts destination from source
	EBO_MIN,         //!< Choose minimum value of each color channel
	EBO_MAX,         //!< Choose maximum value of each color channel
	EBO_MIN_FACTOR,  //!< Choose minimum value of each color channel after applying blend factors, not widely supported
	EBO_MAX_FACTOR,  //!< Choose maximum value of each color channel after applying blend factors, not widely supported
	EBO_MIN_ALPHA,   //!< Choose minimum value of each color channel based on alpha value, not widely supported
	EBO_MAX_ALPHA    //!< Choose maximum value of each color channel based on alpha value, not widely supported
};

//! MaterialTypeParam: e.g. DirectX: D3DTOP_MODULATE, D3DTOP_MODULATE2X, D3DTOP_MODULATE4X
enum E_MODULATE_FUNC : u8
{
	EMFN_MODULATE_1X = 1,
	EMFN_MODULATE_2X = 2,
	EMFN_MODULATE_4X = 4
};

//! Comparison function, e.g. for depth buffer test
enum E_COMPARISON_FUNC : u8
{
	//! Depth test disabled (disable also write to depth buffer)
	ECFN_DISABLED = 0,
	//! <= test, default for e.g. depth test
	ECFN_LESSEQUAL = 1,
	//! Exact equality
	ECFN_EQUAL = 2,
	//! exclusive less comparison, i.e. <
	ECFN_LESS,
	//! Succeeds almost always, except for exact equality
	ECFN_NOTEQUAL,
	//! >= test
	ECFN_GREATEREQUAL,
	//! inverse of <=
	ECFN_GREATER,
	//! test succeeds always
	ECFN_ALWAYS,
	//! Test never succeeds
	ECFN_NEVER
};

//! Enum values for enabling/disabling color planes for rendering
enum E_COLOR_PLANE : u8
{
	//! No color enabled
	ECP_NONE = 0,
	//! Alpha enabled
	ECP_ALPHA = 1,
	//! Red enabled
	ECP_RED = 2,
	//! Green enabled
	ECP_GREEN = 4,
	//! Blue enabled
	ECP_BLUE = 8,
	//! All colors, no alpha
	ECP_RGB = 14,
	//! All planes enabled
	ECP_ALL = 15
};

//! Source of the alpha value to take
/** This is currently only supported in EMT_ONETEXTURE_BLEND. You can use an
or'ed combination of values. Alpha values are modulated (multiplied). */
enum E_ALPHA_SOURCE : u8
{
	//! Use no alpha, somewhat redundant with other settings
	EAS_NONE = 0,
	//! Use vertex color alpha
	EAS_VERTEX_COLOR,
	//! Use texture alpha channel
	EAS_TEXTURE
};

//! Pack srcFact, dstFact, Modulate and alpha source to MaterialTypeParam or BlendFactor
/** alpha source can be an OR'ed combination of E_ALPHA_SOURCE values. */
inline f32 pack_textureBlendFunc(const E_BLEND_FACTOR srcFact, const E_BLEND_FACTOR dstFact,
		const E_MODULATE_FUNC modulate = EMFN_MODULATE_1X, const u32 alphaSource = EAS_TEXTURE)
{
	const u32 tmp = (alphaSource << 20) | (modulate << 16) | (srcFact << 12) | (dstFact << 8) | (srcFact << 4) | dstFact;
	return FR(tmp);
}

//! Pack srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, Modulate and alpha source to MaterialTypeParam or BlendFactor
/** alpha source can be an OR'ed combination of E_ALPHA_SOURCE values. */
inline f32 pack_textureBlendFuncSeparate(const E_BLEND_FACTOR srcRGBFact, const E_BLEND_FACTOR dstRGBFact,
		const E_BLEND_FACTOR srcAlphaFact, const E_BLEND_FACTOR dstAlphaFact,
		const E_MODULATE_FUNC modulate = EMFN_MODULATE_1X, const u32 alphaSource = EAS_TEXTURE)
{
	const u32 tmp = (alphaSource << 20) | (modulate << 16) | (srcAlphaFact << 12) | (dstAlphaFact << 8) | (srcRGBFact << 4) | dstRGBFact;
	return FR(tmp);
}

//! Unpack srcFact, dstFact, modulo and alphaSource factors
/** The fields don't use the full byte range, so we could pack even more... */
inline void unpack_textureBlendFunc(E_BLEND_FACTOR &srcFact, E_BLEND_FACTOR &dstFact,
		E_MODULATE_FUNC &modulo, u32 &alphaSource, const f32 param)
{
	const u32 state = IR(param);
	alphaSource = (state & 0x00F00000) >> 20;
	modulo = E_MODULATE_FUNC((state & 0x000F0000) >> 16);
	srcFact = E_BLEND_FACTOR((state & 0x000000F0) >> 4);
	dstFact = E_BLEND_FACTOR((state & 0x0000000F));
}

//! Unpack srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulo and alphaSource factors
/** The fields don't use the full byte range, so we could pack even more... */
inline void unpack_textureBlendFuncSeparate(E_BLEND_FACTOR &srcRGBFact, E_BLEND_FACTOR &dstRGBFact,
		E_BLEND_FACTOR &srcAlphaFact, E_BLEND_FACTOR &dstAlphaFact,
		E_MODULATE_FUNC &modulo, u32 &alphaSource, const f32 param)
{
	const u32 state = IR(param);
	alphaSource = (state & 0x00F00000) >> 20;
	modulo = E_MODULATE_FUNC((state & 0x000F0000) >> 16);
	srcAlphaFact = E_BLEND_FACTOR((state & 0x0000F000) >> 12);
	dstAlphaFact = E_BLEND_FACTOR((state & 0x00000F00) >> 8);
	srcRGBFact = E_BLEND_FACTOR((state & 0x000000F0) >> 4);
	dstRGBFact = E_BLEND_FACTOR((state & 0x0000000F));
}

//! has blend factor alphablending
inline bool textureBlendFunc_hasAlpha(const E_BLEND_FACTOR factor)
{
	switch (factor) {
	case EBF_SRC_ALPHA:
	case EBF_ONE_MINUS_SRC_ALPHA:
	case EBF_DST_ALPHA:
	case EBF_ONE_MINUS_DST_ALPHA:
	case EBF_SRC_ALPHA_SATURATE:
		return true;
	default:
		return false;
	}
}

//! These flags are used to specify the anti-aliasing and smoothing modes
/** Techniques supported are multisampling, geometry smoothing, and alpha
to coverage.
Some drivers don't support a per-material setting of the anti-aliasing
modes. In those cases, FSAA/multisampling is defined by the device mode
chosen upon creation via SIrrCreationParameters.
*/
enum E_ANTI_ALIASING_MODE : u8
{
	//! Use to turn off anti-aliasing for this material
	EAAM_OFF = 0,
	//! Default anti-aliasing mode
	EAAM_SIMPLE = 1,
	//! High-quality anti-aliasing, not always supported, automatically enables SIMPLE mode
	EAAM_QUALITY = 3,
	//! Enhanced anti-aliasing for transparent materials
	/** Usually used with EMT_TRANSPARENT_ALPHA_CHANNEL_REF and multisampling. */
	EAAM_ALPHA_TO_COVERAGE = 4
};

//! Names for polygon offset direction
const c8 *const PolygonOffsetDirectionNames[] = {
		"Back",
		"Front",
		0,
	};

//! For SMaterial.ZWriteEnable
enum E_ZWRITE : u8
{
	//! zwrite always disabled for this material
	EZW_OFF = 0,

	//! This is the default setting for SMaterial and tries to handle things automatically.
	//! This is what you want to set to enable zwriting.
	//! Usually zwriting is enabled non-transparent materials - as far as Irrlicht can recognize those.
	//! Basically Irrlicht tries to handle the zwriting for you and assumes transparent materials don't need it.
	//! This is addionally affected by IVideoDriver::setAllowZWriteOnTransparent
	EZW_AUTO,

	//! zwrite always enabled for this material
	EZW_ON
};

//! Names for E_ZWRITE
const c8 *const ZWriteNames[] = {
		"Off",
		"Auto",
		"On",
		0,
	};

//! Maximum number of texture an SMaterial can have.
/** SMaterial might ignore some textures in most function, like assignment and comparison,
	when SIrrlichtCreationParameters::MaxTextureUnits is set to a lower number.
*/
constexpr static u32 MATERIAL_MAX_TEXTURES = 4;

//! Struct for holding parameters for a material renderer
// Note for implementors: Serialization is in CNullDriver
class SMaterial
{
public:
	//! Default constructor. Creates a solid material
	SMaterial() :
			MaterialType(EMT_SOLID), ColorParam(0, 0, 0, 0),
			MaterialTypeParam(0.0f), Thickness(1.0f), BlendFactor(0.0f),
			PolygonOffsetDepthBias(0.f), PolygonOffsetSlopeScale(0.f),
			ZBuffer(ECFN_LESSEQUAL), AntiAliasing(EAAM_SIMPLE), ColorMask(ECP_ALL),
			BlendOperation(EBO_NONE), Wireframe(false), PointCloud(false),
			ZWriteEnable(EZW_AUTO),
			BackfaceCulling(true), FrontfaceCulling(false), FogEnable(false),
			UseMipMaps(true)
	{
	}

	//! Texture layer array.
	SMaterialLayer TextureLayers[MATERIAL_MAX_TEXTURES];

	//! Type of the material. Specifies how everything is blended together
	E_MATERIAL_TYPE MaterialType;

	//! Custom color parameter, can be used by custom shader materials.
	// See MainShaderConstantSetter in Luanti.
	SColor ColorParam;

	//! Free parameter, dependent on the material type.
	/** Mostly ignored, used for example in
	EMT_TRANSPARENT_ALPHA_CHANNEL and EMT_ONETEXTURE_BLEND. */
	f32 MaterialTypeParam;

	//! Thickness of non-3dimensional elements such as lines and points.
	f32 Thickness;

	//! Store the blend factors
	/** textureBlendFunc/textureBlendFuncSeparate functions should be used to write
	properly blending factors to this parameter.
	Due to historical reasons this parameter is not used for material type
	EMT_ONETEXTURE_BLEND which uses MaterialTypeParam instead for the blend factor.
	It's generally used only for materials without any blending otherwise (like EMT_SOLID).
	It's main use is to allow having shader materials which can enable/disable
	blending after they have been created.
	When you set this you usually also have to set BlendOperation to a value != EBO_NONE
	(setting it to EBO_ADD is probably the most common one value). */
	f32 BlendFactor;

	//! A constant z-buffer offset for a polygon/line/point
	/** The range of the value is driver specific.
	On OpenGL you get units which are multiplied by the smallest value that is guaranteed to produce a resolvable offset. */
	f32 PolygonOffsetDepthBias;

	//! Variable Z-Buffer offset based on the slope of the polygon.
	/** For polygons looking flat at a camera you could use 0 (for example in a 2D game)
	But in most cases you will have polygons rendered at a certain slope.
	The driver will calculate the slope for you and this value allows to scale that slope.
	The complete polygon offset is: PolygonOffsetSlopeScale*slope + PolygonOffsetDepthBias
	A good default here is to use 1.f if you want to push the polygons away from the camera
	and -1.f to pull them towards the camera.  */
	f32 PolygonOffsetSlopeScale;

	//! Is the ZBuffer enabled? Default: ECFN_LESSEQUAL
	/** If you want to disable depth test for this material
	just set this parameter to ECFN_DISABLED. */
	E_COMPARISON_FUNC ZBuffer : 4;

	//! Sets the antialiasing mode
	/** Default is EAAM_SIMPLE, i.e. simple multi-sample anti-aliasing. */
	E_ANTI_ALIASING_MODE AntiAliasing : 4;

	//! Defines the enabled color planes
	/** Values are defined as or'ed values of the E_COLOR_PLANE enum.
	Only enabled color planes will be rendered to the current render
	target. Typical use is to disable all colors when rendering only to
	depth or stencil buffer, or using Red and Green for Stereo rendering. */
	E_COLOR_PLANE ColorMask : 4;

	//! Store the blend operation of choice
	E_BLEND_OPERATION BlendOperation : 4;

	//! Draw as wireframe or filled triangles? Default: false
	bool Wireframe : 1;

	//! Draw as point cloud or filled triangles? Default: false
	bool PointCloud : 1;

	//! Is the zbuffer writable or is it read-only. Default: EZW_AUTO.
	/** If this parameter is not EZW_OFF, you probably also want to set ZBuffer
	to values other than ECFN_DISABLED */
	E_ZWRITE ZWriteEnable : 2;

	//! Is backface culling enabled? Default: true
	bool BackfaceCulling : 1;

	//! Is frontface culling enabled? Default: false
	bool FrontfaceCulling : 1;

	//! Is fog enabled? Default: false
	bool FogEnable : 1;

	//! Shall mipmaps be used if available
	/** Sometimes, disabling mipmap usage can be useful. Default: true */
	bool UseMipMaps : 1;

	//! Execute a function on all texture layers.
	/** Useful for setting properties which are not per material, but per
	texture layer, e.g. bilinear filtering. */
	template <typename F>
	void forEachTexture(F &&fn)
	{
		for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; i++) {
			fn(TextureLayers[i]);
		}
	}

	//! Gets the texture transformation matrix for level i
	/** \param i The desired level. Must not be larger than MATERIAL_MAX_TEXTURES
	\return Texture matrix for texture level i. */
	core::matrix4 &getTextureMatrix(u32 i)
	{
		return TextureLayers[i].getTextureMatrix();
	}

	//! Gets the immutable texture transformation matrix for level i
	/** \param i The desired level.
	\return Texture matrix for texture level i, or identity matrix for levels larger than MATERIAL_MAX_TEXTURES. */
	const core::matrix4 &getTextureMatrix(u32 i) const
	{
		if (i < MATERIAL_MAX_TEXTURES)
			return TextureLayers[i].getTextureMatrix();
		else
			return core::IdentityMatrix;
	}

	//! Sets the i-th texture transformation matrix
	/** \param i The desired level.
	\param mat Texture matrix for texture level i. */
	void setTextureMatrix(u32 i, const core::matrix4 &mat)
	{
		if (i >= MATERIAL_MAX_TEXTURES)
			return;
		TextureLayers[i].setTextureMatrix(mat);
	}

	//! Gets the i-th texture
	/** \param i The desired level.
	\return Texture for texture level i, if defined, else 0. */
	ITexture *getTexture(u32 i) const
	{
		return i < MATERIAL_MAX_TEXTURES ? TextureLayers[i].Texture : 0;
	}

	//! Sets the i-th texture
	/** If i>=MATERIAL_MAX_TEXTURES this setting will be ignored.
	\param i The desired level.
	\param tex Texture for texture level i. */
	void setTexture(u32 i, ITexture *tex)
	{
		if (i >= MATERIAL_MAX_TEXTURES)
			return;
		TextureLayers[i].Texture = tex;
	}

	//! Inequality operator
	/** \param b Material to compare to.
	\return True if the materials differ, else false. */
	inline bool operator!=(const SMaterial &b) const
	{
		bool different =
				MaterialType != b.MaterialType ||
				ColorParam != b.ColorParam ||
				MaterialTypeParam != b.MaterialTypeParam ||
				Thickness != b.Thickness ||
				Wireframe != b.Wireframe ||
				PointCloud != b.PointCloud ||
				ZBuffer != b.ZBuffer ||
				ZWriteEnable != b.ZWriteEnable ||
				BackfaceCulling != b.BackfaceCulling ||
				FrontfaceCulling != b.FrontfaceCulling ||
				FogEnable != b.FogEnable ||
				AntiAliasing != b.AntiAliasing ||
				ColorMask != b.ColorMask ||
				BlendOperation != b.BlendOperation ||
				BlendFactor != b.BlendFactor ||
				PolygonOffsetDepthBias != b.PolygonOffsetDepthBias ||
				PolygonOffsetSlopeScale != b.PolygonOffsetSlopeScale ||
				UseMipMaps != b.UseMipMaps;
		if (different)
			return true;
		for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
			if (TextureLayers[i] != b.TextureLayers[i])
				return true;
		}
		return false;
	}

	//! Equality operator
	/** \param b Material to compare to.
	\return True if the materials are equal, else false. */
	inline bool operator==(const SMaterial &b) const
	{
		return !(b != *this);
	}

	//! Check if material needs alpha blending
	bool isAlphaBlendOperation() const
	{
		if (BlendOperation != EBO_NONE && BlendFactor != 0.f) {
			E_BLEND_FACTOR srcRGBFact = EBF_ZERO;
			E_BLEND_FACTOR dstRGBFact = EBF_ZERO;
			E_BLEND_FACTOR srcAlphaFact = EBF_ZERO;
			E_BLEND_FACTOR dstAlphaFact = EBF_ZERO;
			E_MODULATE_FUNC modulo = EMFN_MODULATE_1X;
			u32 alphaSource = 0;

			unpack_textureBlendFuncSeparate(srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulo, alphaSource, BlendFactor);

			if (textureBlendFunc_hasAlpha(srcRGBFact) || textureBlendFunc_hasAlpha(dstRGBFact) ||
					textureBlendFunc_hasAlpha(srcAlphaFact) || textureBlendFunc_hasAlpha(dstAlphaFact)) {
				return true;
			}
		}
		return false;
	}

	//! Check for some fixed-function transparent types. Still used internally, but might be deprecated soon.
	//! You probably should not use this anymore, IVideoDriver::needsTransparentRenderPass is more useful in most situations
	//! as it asks the material renders directly what they do with the material.
	bool isTransparent() const
	{
		if (MaterialType == EMT_TRANSPARENT_ALPHA_CHANNEL ||
				MaterialType == EMT_TRANSPARENT_VERTEX_ALPHA)
			return true;

		return false;
	}
};

//! global const identity Material
extern const SMaterial IdentityMaterial;

} // end namespace video

template<>
struct std::hash<video::SMaterial>
{
	/// @brief std::hash specialization for video::SMaterial
	std::size_t operator()(const video::SMaterial &m) const noexcept
	{
		std::size_t ret = 0;
		for (auto h : { // the three members most likely to differ
			std::hash<video::ITexture*>{}(m.getTexture(0)),
			std::hash<int>{}(m.MaterialType),
			std::hash<u32>{}(m.ColorParam.color)
		}) {
			ret += h;
			ret ^= (ret << 6) + (ret >> 2); // distribute bits
		}
		return ret;
	}
};
