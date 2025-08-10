// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "EDriverTypes.h"
#include "irrArray.h"

namespace video
{
class ITexture;

//! Enumeration of cube texture surfaces
enum E_CUBE_SURFACE
{
	ECS_POSX = 0,
	ECS_NEGX,
	ECS_POSY,
	ECS_NEGY,
	ECS_POSZ,
	ECS_NEGZ
};

//! Interface of a Render Target.
/** This is a framebuffer object (FBO) in OpenGL. */
class IRenderTarget : public virtual IReferenceCounted
{
public:
	//! constructor
	IRenderTarget() :
			DepthStencil(0), DriverType(EDT_NULL)
	{
	}

	//! Returns an array of previously set textures.
	const core::array<ITexture *> &getTexture() const
	{
		return Textures;
	}

	//! Returns a of previously set depth / depth-stencil texture.
	ITexture *getDepthStencil() const
	{
		return DepthStencil;
	}

	//! Returns an array of active surface for cube textures
	const core::array<E_CUBE_SURFACE> &getCubeSurfaces() const
	{
		return CubeSurfaces;
	}

	//! Set multiple textures.
	/** Set multiple textures for the render target.
	\param texture Array of texture objects. These textures are used for a color outputs.
	\param depthStencil Depth or packed depth-stencil texture. This texture is used as depth
	or depth-stencil buffer. You can pass getDepthStencil() if you don't want to change it.
	\param cubeSurfaces When rendering to cube textures, set the surface to be used for each texture. Can be empty otherwise.
	*/
	void setTexture(const core::array<ITexture *> &texture, ITexture *depthStencil, const core::array<E_CUBE_SURFACE> &cubeSurfaces = core::array<E_CUBE_SURFACE>())
	{
		setTextures(texture.const_pointer(), texture.size(), depthStencil, cubeSurfaces.const_pointer(), cubeSurfaces.size());
	}

	//! Sets one texture + depthStencil
	//! You can pass getDepthStencil() for depthStencil if you don't want to change that one
	void setTexture(ITexture *texture, ITexture *depthStencil)
	{
		if (texture) {
			setTextures(&texture, 1, depthStencil);
		} else {
			setTextures(0, 0, depthStencil);
		}
	}

	//! Set one cube surface texture.
	void setTexture(ITexture *texture, ITexture *depthStencil, E_CUBE_SURFACE cubeSurface)
	{
		if (texture) {
			setTextures(&texture, 1, depthStencil, &cubeSurface, 1);
		} else {
			setTextures(0, 0, depthStencil, &cubeSurface, 1);
		}
	}

	//! Get driver type of render target.
	E_DRIVER_TYPE getDriverType() const
	{
		return DriverType;
	}

protected:
	//! Set multiple textures.
	// NOTE: working with pointers instead of arrays to avoid unnecessary memory allocations for the single textures case
	virtual void setTextures(ITexture *const *textures, u32 numTextures, ITexture *depthStencil, const E_CUBE_SURFACE *cubeSurfaces = 0, u32 numCubeSurfaces = 0) = 0;

	//! Textures assigned to render target.
	core::array<ITexture *> Textures;

	//! Depth or packed depth-stencil texture assigned to render target.
	ITexture *DepthStencil;

	//! Active surface of cube textures
	core::array<E_CUBE_SURFACE> CubeSurfaces;

	//! Driver type of render target.
	E_DRIVER_TYPE DriverType;
};

}
