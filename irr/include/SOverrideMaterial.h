// Copyright (C) 2017 Michael Zeilfelder
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <vector>
#include "SMaterial.h"

namespace irr
{
namespace video
{

struct SOverrideMaterial
{
	//! The Material values
	SMaterial Material;

	//! Which values are overridden
	/** OR'ed values from E_MATERIAL_PROPS. */
	u32 EnableProps;

	//! For those properties in EnableProps which affect layers, set which of the layers are affected
	bool EnableLayerProps[MATERIAL_MAX_TEXTURES];

	//! Which textures are overridden
	bool EnableTextures[MATERIAL_MAX_TEXTURES];

	//! Overwrite complete layers (settings of EnableLayerProps and EnableTextures don't matter then for layer data)
	bool EnableLayers[MATERIAL_MAX_TEXTURES];

	//! Set in which render passes the material override is active.
	/** OR'ed values from E_SCENE_NODE_RENDER_PASS. */
	u16 EnablePasses;

	//! Global enable flag, overwritten by the SceneManager in each pass
	/** NOTE: This is generally _not_ set by users of the engine, but the
	Scenemanager uses the EnablePass array and sets Enabled to true if the
	Override material is enabled in the current pass.
	As user you generally _only_ set EnablePasses.
	The exception is when rendering without SceneManager but using draw calls in the VideoDriver. */
	bool Enabled;

	struct SMaterialTypeReplacement
	{
		SMaterialTypeReplacement(s32 original, u32 replacement) :
				Original(original), Replacement(replacement) {}
		SMaterialTypeReplacement(u32 replacement) :
				Original(-1), Replacement(replacement) {}

		//! SMaterial.MaterialType to replace.
		//! -1 for all types or a specific value to only replace that one (which is either one of E_MATERIAL_TYPE or a shader material id)
		s32 Original;

		//! MaterialType to used to override Original (either one of E_MATERIAL_TYPE or a shader material id)
		u32 Replacement;
	};

	//! To overwrite SMaterial::MaterialType
	std::vector<SMaterialTypeReplacement> MaterialTypes;

	//! Default constructor
	SOverrideMaterial() :
			EnableProps(0), EnablePasses(0), Enabled(false)
	{
	}

	//! disable overrides and reset all properties
	void reset()
	{
		EnableProps = 0;
		EnablePasses = 0;
		Enabled = false;
		for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
			EnableLayerProps[i] = true; // doesn't do anything unless EnableProps is set, just saying by default all texture layers are affected by properties
			EnableTextures[i] = false;
			EnableLayers[i] = false;
		}
		MaterialTypes.clear();
	}

	//! Apply the enabled overrides
	void apply(SMaterial &material)
	{
		if (Enabled) {
			for (const auto &mtr : MaterialTypes) {
				if (mtr.Original < 0 || mtr.Original == (s32)material.MaterialType)
					material.MaterialType = (E_MATERIAL_TYPE)mtr.Replacement;
			}
			for (u32 f = 0; f < 32; ++f) {
				const u32 num = (1 << f);
				if (EnableProps & num) {
					switch (num) {
					case EMP_WIREFRAME:
						material.Wireframe = Material.Wireframe;
						break;
					case EMP_POINTCLOUD:
						material.PointCloud = Material.PointCloud;
						break;
					case EMP_ZBUFFER:
						material.ZBuffer = Material.ZBuffer;
						break;
					case EMP_ZWRITE_ENABLE:
						material.ZWriteEnable = Material.ZWriteEnable;
						break;
					case EMP_BACK_FACE_CULLING:
						material.BackfaceCulling = Material.BackfaceCulling;
						break;
					case EMP_FRONT_FACE_CULLING:
						material.FrontfaceCulling = Material.FrontfaceCulling;
						break;
					case EMP_MIN_FILTER:
						for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
							if (EnableLayerProps[i]) {
								material.TextureLayers[i].MinFilter = Material.TextureLayers[i].MinFilter;
							}
						}
						break;
					case EMP_MAG_FILTER:
						for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
							if (EnableLayerProps[i]) {
								material.TextureLayers[i].MagFilter = Material.TextureLayers[i].MagFilter;
							}
						}
						break;
					case EMP_ANISOTROPIC_FILTER:
						for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
							if (EnableLayerProps[i]) {
								material.TextureLayers[i].AnisotropicFilter = Material.TextureLayers[i].AnisotropicFilter;
							}
						}
						break;
					case EMP_FOG_ENABLE:
						material.FogEnable = Material.FogEnable;
						break;
					case EMP_TEXTURE_WRAP:
						for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
							if (EnableLayerProps[i]) {
								material.TextureLayers[i].TextureWrapU = Material.TextureLayers[i].TextureWrapU;
								material.TextureLayers[i].TextureWrapV = Material.TextureLayers[i].TextureWrapV;
								material.TextureLayers[i].TextureWrapW = Material.TextureLayers[i].TextureWrapW;
							}
						}
						break;
					case EMP_ANTI_ALIASING:
						material.AntiAliasing = Material.AntiAliasing;
						break;
					case EMP_COLOR_MASK:
						material.ColorMask = Material.ColorMask;
						break;
					case EMP_USE_MIP_MAPS:
						material.UseMipMaps = Material.UseMipMaps;
						break;
					case EMP_BLEND_OPERATION:
						material.BlendOperation = Material.BlendOperation;
						break;
					case EMP_BLEND_FACTOR:
						material.BlendFactor = Material.BlendFactor;
						break;
					case EMP_POLYGON_OFFSET:
						material.PolygonOffsetDepthBias = Material.PolygonOffsetDepthBias;
						material.PolygonOffsetSlopeScale = Material.PolygonOffsetSlopeScale;
						break;
					}
				}
			}
			for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i) {
				if (EnableLayers[i]) {
					material.TextureLayers[i] = Material.TextureLayers[i];
				} else if (EnableTextures[i]) {
					material.TextureLayers[i].Texture = Material.TextureLayers[i].Texture;
				}
			}
		}
	}
};

} // end namespace video
} // end namespace irr
