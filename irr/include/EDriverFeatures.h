// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

namespace video
{

//! enumeration for querying features of the video driver.
enum E_VIDEO_DRIVER_FEATURE
{
	//! Is driver able to render to a surface?
	EVDF_RENDER_TO_TARGET = 0,

	//! Is hardware transform and lighting supported?
	EVDF_HARDWARE_TL,

	//! Are multiple textures per material possible?
	EVDF_MULTITEXTURE,

	//! Is driver able to render with a bilinear filter applied?
	EVDF_BILINEAR_FILTER,

	//! Can the driver handle mip maps?
	EVDF_MIP_MAP,

	//! Can the driver update mip maps automatically?
	EVDF_MIP_MAP_AUTO_UPDATE,

	//! Are stencilbuffers switched on and does the device support stencil buffers?
	EVDF_STENCIL_BUFFER,

	//! Is Vertex Shader 1.1 supported?
	EVDF_VERTEX_SHADER_1_1,

	//! Is Vertex Shader 2.0 supported?
	EVDF_VERTEX_SHADER_2_0,

	//! Is Vertex Shader 3.0 supported?
	EVDF_VERTEX_SHADER_3_0,

	//! Is Pixel Shader 1.1 supported?
	EVDF_PIXEL_SHADER_1_1,

	//! Is Pixel Shader 1.2 supported?
	EVDF_PIXEL_SHADER_1_2,

	//! Is Pixel Shader 1.3 supported?
	EVDF_PIXEL_SHADER_1_3,

	//! Is Pixel Shader 1.4 supported?
	EVDF_PIXEL_SHADER_1_4,

	//! Is Pixel Shader 2.0 supported?
	EVDF_PIXEL_SHADER_2_0,

	//! Is Pixel Shader 3.0 supported?
	EVDF_PIXEL_SHADER_3_0,

	//! Are ARB vertex programs v1.0 supported?
	EVDF_ARB_VERTEX_PROGRAM_1,

	//! Are ARB fragment programs v1.0 supported?
	EVDF_ARB_FRAGMENT_PROGRAM_1,

	//! Is GLSL supported?
	EVDF_ARB_GLSL,

	//! Is HLSL supported?
	EVDF_HLSL,

	//! Are non-square textures supported?
	EVDF_TEXTURE_NSQUARE,

	//! Are non-power-of-two textures supported?
	EVDF_TEXTURE_NPOT,

	//! Are framebuffer objects supported?
	EVDF_FRAMEBUFFER_OBJECT,

	//! Are vertex buffer objects supported?
	EVDF_VERTEX_BUFFER_OBJECT,

	//! Supports Alpha To Coverage
	EVDF_ALPHA_TO_COVERAGE,

	//! Supports Color masks (disabling color planes in output)
	EVDF_COLOR_MASK,

	//! Supports multiple render targets at once
	EVDF_MULTIPLE_RENDER_TARGETS,

	//! Supports separate blend settings for multiple render targets
	EVDF_MRT_BLEND,

	//! Supports separate color masks for multiple render targets
	EVDF_MRT_COLOR_MASK,

	//! Supports separate blend functions for multiple render targets
	EVDF_MRT_BLEND_FUNC,

	//! Supports geometry shaders
	EVDF_GEOMETRY_SHADER,

	//! Supports occlusion queries
	EVDF_OCCLUSION_QUERY,

	//! Supports polygon offset/depth bias for avoiding z-fighting
	EVDF_POLYGON_OFFSET,

	//! Support for different blend functions. Without, only ADD is available
	EVDF_BLEND_OPERATIONS,

	//! Support for separate blending for RGB and Alpha.
	EVDF_BLEND_SEPARATE,

	//! Support for texture coord transformation via texture matrix
	EVDF_TEXTURE_MATRIX,

	//! Support for cube map textures.
	EVDF_TEXTURE_CUBEMAP,

	//! Support for filtering across different faces of the cubemap
	EVDF_TEXTURE_CUBEMAP_SEAMLESS,

	//! Support for clamping vertices beyond far-plane to depth instead of capping them.
	EVDF_DEPTH_CLAMP,

	//! Support for multisample textures.
	EVDF_TEXTURE_MULTISAMPLE,

	//! Support for 2D array textures.
	EVDF_TEXTURE_2D_ARRAY,

	//! Only used for counting the elements of this enum
	EVDF_COUNT
};

} // end namespace video
