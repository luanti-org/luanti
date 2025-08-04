// Copyright (C) 2017 Michael Zeilfelder
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

// Can be included from different ES versions
// (this is also the reason why this file is header-only as correct OGL ES headers have to be included first)

#include "irrMath.h"
#include "COpenGLCoreFeature.h"

namespace video
{

class COGLESCoreExtensionHandler
{
public:
	// Enums used internally to check for ES extensions quickly.
	// We buffer all extensions on start once in an array.
	// All OpenGL ES versions share the same extensions (WebGL adds it's own extensions on top of ES2)
	enum EOGLESFeatures
	{
		// If you update this enum also update the corresponding OGLESFeatureStrings string-array
		IRR_GL_APPLE_texture_format_BGRA8888,
		IRR_GL_EXT_blend_minmax,
		IRR_GL_EXT_texture_format_BGRA8888,
		IRR_GL_EXT_texture_rg,
		IRR_GL_OES_depth_texture,
		IRR_GL_OES_element_index_uint,
		IRR_GL_OES_packed_depth_stencil,
		IRR_GL_OES_texture_float,
		IRR_GL_OES_texture_half_float,

		IRR_OGLES_Feature_Count
	};

	COGLESCoreExtensionHandler() :
			MaxAnisotropy(1), MaxIndices(0xffff),
			MaxTextureSize(1), MaxArrayTextureLayers(1),
			MaxTextureLODBias(0.f), StencilBuffer(false)
	{
		for (u32 i = 0; i < IRR_OGLES_Feature_Count; ++i)
			FeatureAvailable[i] = false;

		DimAliasedLine[0] = 1.f;
		DimAliasedLine[1] = 1.f;
		DimAliasedPoint[0] = 1.f;
		DimAliasedPoint[1] = 1.f;
	}

	virtual ~COGLESCoreExtensionHandler() {}

	const COpenGLCoreFeature &getFeature() const
	{
		return Feature;
	}

	void dump() const
	{
		for (u32 i = 0; i < IRR_OGLES_Feature_Count; ++i)
			os::Printer::log(getFeatureString(i), FeatureAvailable[i] ? " true" : " false");
	}

protected:
	const char *getFeatureString(size_t index) const
	{
		// One for each EOGLESFeatures
		static const char *const OGLESFeatureStrings[IRR_OGLES_Feature_Count] = {
				"GL_APPLE_texture_format_BGRA8888",
				"GL_EXT_blend_minmax",
				"GL_EXT_texture_format_BGRA8888",
				"GL_EXT_texture_rg",
				"GL_OES_depth_texture",
				"GL_OES_element_index_uint",
				"GL_OES_packed_depth_stencil",
				"GL_OES_texture_float",
				"GL_OES_texture_half_float",
			};

		return OGLESFeatureStrings[index];
	}

	COpenGLCoreFeature Feature;

	u8 MaxAnisotropy;
	u32 MaxIndices;
	u32 MaxTextureSize;
	u32 MaxArrayTextureLayers;
	f32 MaxTextureLODBias;
	//! Minimal and maximal supported thickness for lines without smoothing
	float DimAliasedLine[2];
	//! Minimal and maximal supported thickness for points without smoothing
	float DimAliasedPoint[2];
	bool StencilBuffer;
	bool FeatureAvailable[IRR_OGLES_Feature_Count];
};
}
