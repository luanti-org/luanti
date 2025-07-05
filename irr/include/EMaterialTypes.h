// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"

namespace video
{

//! Abstracted and easy to use fixed function/programmable pipeline material modes.
enum E_MATERIAL_TYPE
{
	//! Standard solid material.
	/** Only first texture is used, which is supposed to be the
	diffuse material. */
	EMT_SOLID = 0,

	//! Makes the material transparent based on the texture alpha channel.
	/** The final color is blended together from the destination
	color and the texture color, using the alpha channel value as
	blend factor. Only first texture is used. If you are using
	this material with small textures, it is a good idea to load
	the texture in 32 bit mode
	(video::IVideoDriver::setTextureCreationFlag()). Also, an alpha
	ref is used, which can be manipulated using
	SMaterial::MaterialTypeParam. This value controls how sharp the
	edges become when going from a transparent to a solid spot on
	the texture. */
	EMT_TRANSPARENT_ALPHA_CHANNEL,

	//! Makes the material transparent based on the texture alpha channel.
	/** If the alpha channel value is greater than 127, a
	pixel is written to the target, otherwise not. This
	material does not use alpha blending and is a lot faster
	than EMT_TRANSPARENT_ALPHA_CHANNEL. It is ideal for drawing
	stuff like leaves of plants, because the borders are not
	blurry but sharp. Only first texture is used. If you are
	using this material with small textures and 3d object, it
	is a good idea to load the texture in 32 bit mode
	(video::IVideoDriver::setTextureCreationFlag()). */
	EMT_TRANSPARENT_ALPHA_CHANNEL_REF,

	//! Makes the material transparent based on the vertex alpha value.
	EMT_TRANSPARENT_VERTEX_ALPHA,

	//! BlendFunc = source * sourceFactor + dest * destFactor ( E_BLEND_FUNC )
	/** Using only first texture. Generic blending method.
	The blend function is set to SMaterial::MaterialTypeParam with
	pack_textureBlendFunc (for 2D) or pack_textureBlendFuncSeparate (for 3D). */
	EMT_ONETEXTURE_BLEND,

	//! This value is not used. It only forces this enumeration to compile to 32 bit.
	EMT_FORCE_32BIT = 0x7fffffff
};

//! Array holding the built in material type names
const char *const sBuiltInMaterialTypeNames[] = {
		"solid",
		"trans_alphach",
		"trans_alphach_ref",
		"trans_vertex_alpha",
		"onetexture_blend",
		0,
	};

constexpr u32 numBuiltInMaterials =
		sizeof(sBuiltInMaterialTypeNames) / sizeof(char *) - 1;

} // end namespace video
