// Copyright (C) 2006-2012 Luke Hoschke
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

// B3D Mesh loader
// File format designed by Mark Sibly for the Blitz3D engine and has been
// declared public domain

#pragma once

#include "SMaterial.h"
#include "irrMath.h"

namespace scene
{

struct SB3dChunkHeader
{
	c8 name[4];
	s32 size;
};

struct SB3dChunk
{
	SB3dChunk(const SB3dChunkHeader &header, long sp) :
			length(header.size + 8), startposition(sp)
	{
		length = core::max_(length, 8);
		name[0] = header.name[0];
		name[1] = header.name[1];
		name[2] = header.name[2];
		name[3] = header.name[3];
	}
	c8 name[4];
	s32 length;
	long startposition;
};

struct SB3dTexture
{
	std::string TextureName;
	s32 Flags;
	s32 Blend;
	f32 Xpos;
	f32 Ypos;
	f32 Xscale;
	f32 Yscale;
	f32 Angle;
};

struct SB3dMaterial
{
	SB3dMaterial() :
			red(1.0f), green(1.0f),
			blue(1.0f), alpha(1.0f), shininess(0.0f), blend(1),
			fx(0)
	{
		for (u32 i = 0; i < video::MATERIAL_MAX_TEXTURES; ++i)
			Textures[i] = 0;
	}
	video::SMaterial Material;
	f32 red, green, blue, alpha;
	f32 shininess;
	s32 blend, fx;
	SB3dTexture *Textures[video::MATERIAL_MAX_TEXTURES];
};

} // end namespace scene
