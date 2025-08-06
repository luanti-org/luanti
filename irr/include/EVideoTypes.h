// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "SMaterial.h" // MATERIAL_MAX_TEXTURES

namespace video
{

//! enumeration for geometry transformation states
enum E_TRANSFORMATION_STATE
{
	//! View transformation
	ETS_VIEW = 0,
	//! World transformation
	ETS_WORLD,
	//! Projection transformation
	ETS_PROJECTION,
	//! Texture 0 transformation
	//! Use E_TRANSFORMATION_STATE(ETS_TEXTURE_0 + texture_number) to access other texture transformations
	ETS_TEXTURE_0,
	//! Only used internally
	ETS_COUNT = ETS_TEXTURE_0 + MATERIAL_MAX_TEXTURES
};

//! Special render targets, which usually map to dedicated hardware
/** These render targets (besides 0 and 1) need not be supported by gfx cards */
enum E_RENDER_TARGET
{
	//! Render target is the main color frame buffer
	ERT_FRAME_BUFFER = 0,
	//! Render target is a render texture
	ERT_RENDER_TEXTURE,
	//! Multi-Render target textures
	ERT_MULTI_RENDER_TEXTURES,
	//! Render target is the main color frame buffer
	ERT_STEREO_LEFT_BUFFER,
	//! Render target is the right color buffer (left is the main buffer)
	ERT_STEREO_RIGHT_BUFFER,
	//! Render to both stereo buffers at once
	ERT_STEREO_BOTH_BUFFERS,
	//! Auxiliary buffer 0
	ERT_AUX_BUFFER0,
	//! Auxiliary buffer 1
	ERT_AUX_BUFFER1,
	//! Auxiliary buffer 2
	ERT_AUX_BUFFER2,
	//! Auxiliary buffer 3
	ERT_AUX_BUFFER3,
	//! Auxiliary buffer 4
	ERT_AUX_BUFFER4
};

//! Enum for the flags of clear buffer
enum E_CLEAR_BUFFER_FLAG
{
	ECBF_NONE = 0,
	ECBF_COLOR = 1,
	ECBF_DEPTH = 2,
	ECBF_STENCIL = 4,
	ECBF_ALL = ECBF_COLOR | ECBF_DEPTH | ECBF_STENCIL
};

//! Enum for the types of fog distributions to choose from
enum E_FOG_TYPE
{
	EFT_FOG_EXP = 0,
	EFT_FOG_LINEAR,
	EFT_FOG_EXP2
};

} // end namespace video

