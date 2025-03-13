// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

namespace irr
{
namespace video
{

//! Material properties
enum E_MATERIAL_PROP
{
	//! Corresponds to SMaterial::Wireframe.
	EMP_WIREFRAME = 0x1,

	//! Corresponds to SMaterial::PointCloud.
	EMP_POINTCLOUD = 0x2,

	//! Corresponds to SMaterial::ZBuffer.
	EMP_ZBUFFER = 0x10,

	//! Corresponds to SMaterial::ZWriteEnable.
	EMP_ZWRITE_ENABLE = 0x20,

	//! Corresponds to SMaterial::BackfaceCulling.
	EMP_BACK_FACE_CULLING = 0x40,

	//! Corresponds to SMaterial::FrontfaceCulling.
	EMP_FRONT_FACE_CULLING = 0x80,

	//! Corresponds to SMaterialLayer::MinFilter.
	EMP_MIN_FILTER = 0x100,

	//! Corresponds to SMaterialLayer::MagFilter.
	EMP_MAG_FILTER = 0x200,

	//! Corresponds to SMaterialLayer::AnisotropicFilter.
	EMP_ANISOTROPIC_FILTER = 0x400,

	//! Corresponds to SMaterial::FogEnable.
	EMP_FOG_ENABLE = 0x800,

	//! Corresponds to SMaterialLayer::TextureWrapU, TextureWrapV and
	//! TextureWrapW.
	EMP_TEXTURE_WRAP = 0x2000,

	//! Corresponds to SMaterial::AntiAliasing.
	EMP_ANTI_ALIASING = 0x4000,

	//! Corresponds to SMaterial::ColorMask.
	EMP_COLOR_MASK = 0x8000,

	//! Corresponds to SMaterial::UseMipMaps.
	EMP_USE_MIP_MAPS = 0x20000,

	//! Corresponds to SMaterial::BlendOperation.
	EMP_BLEND_OPERATION = 0x40000,

	//! Corresponds to SMaterial::PolygonOffsetFactor, PolygonOffsetDirection,
	//! PolygonOffsetDepthBias and PolygonOffsetSlopeScale.
	EMP_POLYGON_OFFSET = 0x80000,

	//! Corresponds to SMaterial::BlendFactor.
	EMP_BLEND_FACTOR = 0x100000,
};

} // end namespace video
} // end namespace irr
