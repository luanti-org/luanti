// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "EMaterialTypes.h"
#include "EPrimitiveTypes.h"
#include "path.h"

namespace video
{

class IShaderConstantSetCallBack;

//! Interface making it possible to create and use programs running on the GPU.
class IGPUProgrammingServices
{
public:
	//! Destructor
	virtual ~IGPUProgrammingServices() {}

	//! Adds a new high-level shading material renderer to the VideoDriver.
	/**
	\param vertexShaderProgram String containing the source of the vertex
	shader program. This can be 0 if no vertex program shall be used.
	\param pixelShaderProgram String containing the source of the pixel
	shader program. This can be 0 if no pixel shader shall be used.
	\param geometryShaderProgram String containing the source of the
	geometry shader program. This can be 0 if no geometry shader shall be
	used.
	\param shaderName Name of the shader for debug purposes
	\param inType Type of vertices passed to geometry shader
	\param outType Type of vertices created by geometry shader
	\param verticesOut Maximal number of vertices created by geometry
	shader. If 0, maximal number supported is assumed.
	\param callback Pointer to an implementation of
	IShaderConstantSetCallBack in which you can set the needed vertex,
	pixel, and geometry shader program constants. Set this to 0 if you
	don't need this.
	\param baseMaterial Base material which renderstates will be used to
	shade the material.
	\param userData a user data int. This int can be set to any value and
	will be set as parameter in the callback method when calling
	OnSetConstants(). In this way it is easily possible to use the same
	callback method for multiple materials and distinguish between them
	during the call.
	\return Number of the material type which can be set in
	SMaterial::MaterialType to use the renderer. -1 is returned if an error
	occurred, e.g. if a shader program could not be compiled or a compile
	target is not reachable. The error strings are then printed to the
	error log and can be caught with a custom event receiver. */
	virtual s32 addHighLevelShaderMaterial(
			const c8 *vertexShaderProgram,
			const c8 *pixelShaderProgram,
			const c8 *geometryShaderProgram,
			const c8 *shaderName = nullptr,
			scene::E_PRIMITIVE_TYPE inType = scene::EPT_TRIANGLES,
			scene::E_PRIMITIVE_TYPE outType = scene::EPT_TRIANGLE_STRIP,
			u32 verticesOut = 0,
			IShaderConstantSetCallBack *callback = nullptr,
			E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
			s32 userData = 0) = 0;

	//! convenience function for use without geometry shaders
	s32 addHighLevelShaderMaterial(
			const c8 *vertexShaderProgram,
			const c8 *pixelShaderProgram = nullptr,
			const c8 *shaderName = nullptr,
			IShaderConstantSetCallBack *callback = nullptr,
			E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
			s32 userData = 0)
	{
		return addHighLevelShaderMaterial(
				vertexShaderProgram, pixelShaderProgram,
				nullptr, shaderName,
				scene::EPT_TRIANGLES, scene::EPT_TRIANGLE_STRIP, 0,
				callback, baseMaterial, userData);
	}

	//! Like addHighLevelShaderMaterial(), but loads from files.
	/** \param vertexShaderProgramFileName Text file containing the source
	of the vertex shader program. Set to empty string if no vertex shader
	shall be created.
	\param pixelShaderProgramFileName Text file containing the source of
	the pixel shader program. Set to empty string if no pixel shader shall
	be created.
	\param geometryShaderProgramFileName Name of the source of
	the geometry shader program. Set to empty string if no geometry shader
	shall be created.
	\param shaderName Name of the shader for debug purposes
	\param inType Type of vertices passed to geometry shader
	\param outType Type of vertices created by geometry shader
	\param verticesOut Maximal number of vertices created by geometry
	shader. If 0, maximal number supported is assumed.
	\param callback Pointer to an implementation of
	IShaderConstantSetCallBack in which you can set the needed vertex,
	pixel, and geometry shader program constants. Set this to 0 if you
	don't need this.
	\param baseMaterial Base material which renderstates will be used to
	shade the material.
	\param userData a user data int. This int can be set to any value and
	will be set as parameter in the callback method when calling
	OnSetConstants(). In this way it is easily possible to use the same
	callback method for multiple materials and distinguish between them
	during the call.
	\return Number of the material type which can be set in
	SMaterial::MaterialType to use the renderer. -1 is returned if an error
	occurred, e.g. if a shader program could not be compiled or a compile
	target is not reachable. The error strings are then printed to the
	error log and can be caught with a custom event receiver. */
	virtual s32 addHighLevelShaderMaterialFromFiles(
			const io::path &vertexShaderProgramFileName,
			const io::path &pixelShaderProgramFileName,
			const io::path &geometryShaderProgramFileName,
			const c8 *shaderName = nullptr,
			scene::E_PRIMITIVE_TYPE inType = scene::EPT_TRIANGLES,
			scene::E_PRIMITIVE_TYPE outType = scene::EPT_TRIANGLE_STRIP,
			u32 verticesOut = 0,
			IShaderConstantSetCallBack *callback = nullptr,
			E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
			s32 userData = 0) = 0;

	//! Delete a shader material and associated data.
	/**
	After you have deleted a material it is invalid to still use and doing
	so might result in a crash. The ID may be reused in the future when new
	materials are added.
	\param material Number of the material type. Must not be a built-in
	material. */
	virtual void deleteShaderMaterial(s32 material) = 0;
};

} // end namespace video
