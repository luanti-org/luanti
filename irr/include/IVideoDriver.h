// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "rect.h"
#include "SColor.h"
#include "IImage.h"
#include "ITexture.h"
#include "irrArray.h"
#include "matrix4.h"
#include "dimension2d.h"
#include "position2d.h"
#include "EDriverTypes.h"
#include "EDriverFeatures.h"
#include "EPrimitiveTypes.h"
#include "EVideoTypes.h"
#include "SExposedVideoData.h"
#include "SOverrideMaterial.h"
#include "S3DVertex.h" // E_VERTEX_TYPE
#include "SVertexIndex.h" // E_INDEX_TYPE

namespace io
{
class IReadFile;
class IWriteFile;
} // end namespace io
namespace scene
{
class IMeshBuffer;
class IVertexBuffer;
class IIndexBuffer;
class IMesh;
class IMeshManipulator;
class ISceneNode;
} // end namespace scene

namespace video
{
class IImageLoader;
class IImageWriter;
class IMaterialRenderer;
class IGPUProgrammingServices;
class IRenderTarget;

const c8 *const FogTypeNames[] = {
		"FogExp",
		"FogLinear",
		"FogExp2",
		0,
	};

struct SFrameStats {
	//! Number of draw calls
	u32 Drawcalls = 0;
	//! Count of primitives drawn
	u32 PrimitivesDrawn = 0;
	//! Number of hardware buffers uploaded (new or updated)
	u32 HWBuffersUploaded = 0;
	//! Number of active hardware buffers
	u32 HWBuffersActive = 0;
};

//! Interface to driver which is able to perform 2d and 3d graphics functions.
/** This interface is one of the most important interfaces of
the Irrlicht Engine: All rendering and texture manipulation is done with
this interface. You are able to use the Irrlicht Engine by only
invoking methods of this interface if you like to, although the
scene::ISceneManager interface provides a lot of powerful classes
and methods to make the programmer's life easier.
*/
class IVideoDriver : public virtual IReferenceCounted
{
public:
	//! Applications must call this method before performing any rendering.
	/** This method can clear the back- and the z-buffer.
	\param clearFlag A combination of the E_CLEAR_BUFFER_FLAG bit-flags.
	\param clearColor The clear color for the color buffer.
	\param clearDepth The clear value for the depth buffer.
	\param clearStencil The clear value for the stencil buffer.
	\param videoData Handle of another window, if you want the
	bitmap to be displayed on another window. If this is an empty
	element, everything will be displayed in the default window.
	Note: This feature is not fully implemented for all devices.
	\param sourceRect Pointer to a rectangle defining the source
	rectangle of the area to be presented. Set to null to present
	everything. Note: not implemented in all devices.
	\return False if failed. */
	virtual bool beginScene(u16 clearFlag = (u16)(ECBF_COLOR | ECBF_DEPTH), SColor clearColor = SColor(255, 0, 0, 0), f32 clearDepth = 1.f, u8 clearStencil = 0,
			const SExposedVideoData &videoData = SExposedVideoData(), core::rect<s32> *sourceRect = 0) = 0;

	//! Alternative beginScene implementation. Can't clear stencil buffer, but otherwise identical to other beginScene
	bool beginScene(bool backBuffer, bool zBuffer, SColor color = SColor(255, 0, 0, 0),
			const SExposedVideoData &videoData = SExposedVideoData(), core::rect<s32> *sourceRect = 0)
	{
		u16 flag = 0;

		if (backBuffer)
			flag |= ECBF_COLOR;

		if (zBuffer)
			flag |= ECBF_DEPTH;

		return beginScene(flag, color, 1.f, 0, videoData, sourceRect);
	}

	//! Presents the rendered image to the screen.
	/** Applications must call this method after performing any
	rendering.
	\return False if failed and true if succeeded. */
	virtual bool endScene() = 0;

	//! Queries the features of the driver.
	/** Returns true if a feature is available
	\param feature Feature to query.
	\return True if the feature is available, false if not. */
	virtual bool queryFeature(E_VIDEO_DRIVER_FEATURE feature) const = 0;

	//! Disable a feature of the driver.
	/** Can also be used to enable the features again. It is not
	possible to enable unsupported features this way, though.
	\param feature Feature to disable.
	\param flag When true the feature is disabled, otherwise it is enabled. */
	virtual void disableFeature(E_VIDEO_DRIVER_FEATURE feature, bool flag = true) = 0;

	//! Sets transformation matrices.
	/** \param state Transformation type to be set, e.g. view,
	world, or projection.
	\param mat Matrix describing the transformation. */
	virtual void setTransform(E_TRANSFORMATION_STATE state, const core::matrix4 &mat) = 0;

	//! Returns the transformation set by setTransform
	/** \param state Transformation type to query
	\return Matrix describing the transformation. */
	virtual const core::matrix4 &getTransform(E_TRANSFORMATION_STATE state) const = 0;

	//! Retrieve the number of image loaders
	/** \return Number of image loaders */
	virtual u32 getImageLoaderCount() const = 0;

	//! Retrieve the given image loader
	/** \param n The index of the loader to retrieve. This parameter is an 0-based
	array index.
	\return A pointer to the specified loader, 0 if the index is incorrect. */
	virtual IImageLoader *getImageLoader(u32 n) = 0;

	//! Retrieve the number of image writers
	/** \return Number of image writers */
	virtual u32 getImageWriterCount() const = 0;

	//! Retrieve the given image writer
	/** \param n The index of the writer to retrieve. This parameter is an 0-based
	array index.
	\return A pointer to the specified writer, 0 if the index is incorrect. */
	virtual IImageWriter *getImageWriter(u32 n) = 0;

	//! Sets a material.
	/** All 3d drawing functions will draw geometry using this material thereafter.
	\param material: Material to be used from now on. */
	virtual void setMaterial(const SMaterial &material) = 0;

	//! Get access to a named texture.
	/** Loads the texture from disk if it is not
	already loaded and generates mipmap levels if desired.
	Texture loading can be influenced using the
	setTextureCreationFlag() method. The texture can be in several
	imageformats, such as BMP, JPG, TGA, PCX, PNG, and PSD.
	\param filename Filename of the texture to be loaded.
	\return Pointer to the texture, or 0 if the texture
	could not be loaded. This pointer should not be dropped. See
	IReferenceCounted::drop() for more information. */
	[[deprecated]] virtual ITexture *getTexture(const io::path &filename) = 0;

	//! Get access to a named texture.
	/** Loads the texture from disk if it is not
	already loaded and generates mipmap levels if desired.
	Texture loading can be influenced using the
	setTextureCreationFlag() method. The texture can be in several
	imageformats, such as BMP, JPG, TGA, PCX, PNG, and PSD.
	\param file Pointer to an already opened file.
	\return Pointer to the texture, or 0 if the texture
	could not be loaded. This pointer should not be dropped. See
	IReferenceCounted::drop() for more information. */
	[[deprecated]] virtual ITexture *getTexture(io::IReadFile *file) = 0;

	//! Returns amount of textures currently loaded
	/** \return Amount of textures currently loaded */
	virtual u32 getTextureCount() const = 0;

	//! Creates an empty texture of specified size.
	/** \param size: Size of the texture.
	\param name A name for the texture. Later calls to
	getTexture() with this name will return this texture.
	The name can _not_ be empty.
	\param format Desired color format of the texture. Please note
	that the driver may choose to create the texture in another
	color format.
	\return Pointer to the newly created texture. This pointer
	should not be dropped. See IReferenceCounted::drop() for more
	information. */
	virtual ITexture *addTexture(const core::dimension2d<u32> &size,
			const io::path &name, ECOLOR_FORMAT format = ECF_A8R8G8B8) = 0;

	//! Creates a texture from an IImage.
	/** \param name A name for the texture. Later calls of
	getTexture() with this name will return this texture.
	The name can _not_ be empty.
	\param image Image the texture is created from.
	\return Pointer to the newly created texture. This pointer
	should not be dropped. See IReferenceCounted::drop() for more
	information. */
	virtual ITexture *addTexture(const io::path &name, IImage *image) = 0;

	/**
	 * Creates an array texture from IImages.
	 * @param name A name for the texture.
	 * @param images Pointer to array of images
	 * @param count Number of images (must be at least 1)
	 * @return Pointer to the newly created texture
	 */
	virtual ITexture *addArrayTexture(const io::path &name, IImage **images, u32 count) = 0;

	//! Creates a cubemap texture from loaded IImages.
	/** \param name A name for the texture. Later calls of getTexture() with this name will return this texture.
	The name can _not_ be empty.
	\param imagePosX Image (positive X) the texture is created from.
	\param imageNegX Image (negative X) the texture is created from.
	\param imagePosY Image (positive Y) the texture is created from.
	\param imageNegY Image (negative Y) the texture is created from.
	\param imagePosZ Image (positive Z) the texture is created from.
	\param imageNegZ Image (negative Z) the texture is created from.
	\return Pointer to the newly created texture. This pointer should not be dropped. See IReferenceCounted::drop() for more information. */
	virtual ITexture *addTextureCubemap(const io::path &name, IImage *imagePosX, IImage *imageNegX, IImage *imagePosY,
			IImage *imageNegY, IImage *imagePosZ, IImage *imageNegZ) = 0;

	//! Creates an empty cubemap texture of specified size.
	/** \param sideLen diameter of one side of the cube
	\param name A name for the texture. Later calls of
	getTexture() with this name will return this texture.
	The name can _not_ be empty.
	\param format Desired color format of the texture. Please note
	that the driver may choose to create the texture in another
	color format.
	\return Pointer to the newly created texture. 	*/
	virtual ITexture *addTextureCubemap(const u32 sideLen, const io::path &name, ECOLOR_FORMAT format = ECF_A8R8G8B8) = 0;

	//! Adds a new render target texture to the texture cache.
	/** \param size Size of the texture, in pixels. Width and
	height should be a power of two (e.g. 64, 128, 256, 512, ...)
	and it should not be bigger than the backbuffer, because it
	shares the zbuffer with the screen buffer.
	\param name A name for the texture. Later calls of getTexture() with this name will return this texture.
	The name can _not_ be empty.
	\param format The color format of the render target. Floating point formats are supported.
	\return Pointer to the created texture or 0 if the texture
	could not be created. This pointer should not be dropped. See
	IReferenceCounted::drop() for more information.
	You may want to remove it from driver texture cache with removeTexture if you no longer need it.
	*/
	virtual ITexture *addRenderTargetTexture(const core::dimension2d<u32> &size,
			const io::path &name = "rt", const ECOLOR_FORMAT format = ECF_UNKNOWN) = 0;

	//! Adds a multisampled render target texture to the texture cache.
	/** \param msaa The number of samples to use, values that make sense are > 1.
	Only works if the driver supports the EVDF_TEXTURE_MULTISAMPLE feature,
	check via queryFeature.
	\see addRenderTargetTexture */
	virtual ITexture *addRenderTargetTextureMs(const core::dimension2d<u32> &size, u8 msaa,
			const io::path &name = "rt", const ECOLOR_FORMAT format = ECF_UNKNOWN) = 0;

	//! Adds a new render target texture with 6 sides for a cubemap map to the texture cache.
	/** \param sideLen Length of one cubemap side.
	\param name A name for the texture. Later calls of getTexture() with this name will return this texture.
	The name can _not_ be empty.
	\param format The color format of the render target. Floating point formats are supported.
	\return Pointer to the created texture or 0 if the texture
	could not be created. This pointer should not be dropped. See
	IReferenceCounted::drop() for more information. */
	virtual ITexture *addRenderTargetTextureCubemap(const u32 sideLen,
			const io::path &name = "rt", const ECOLOR_FORMAT format = ECF_UNKNOWN) = 0;

	//! Removes a texture from the texture cache and deletes it.
	/** This method can free a lot of memory!
	Please note that after calling this, the pointer to the
	ITexture may no longer be valid, if it was not grabbed before
	by other parts of the engine for storing it longer. So it is a
	good idea to set all materials which are using this texture to
	0 or another texture first.
	\param texture Texture to delete from the engine cache. */
	virtual void removeTexture(ITexture *texture) = 0;

	//! Removes all textures from the texture cache and deletes them.
	/** This method can free a lot of memory!
	Please note that after calling this, the pointer to the
	ITexture may no longer be valid, if it was not grabbed before
	by other parts of the engine for storing it longer. So it is a
	good idea to set all materials which are using this texture to
	0 or another texture first. */
	virtual void removeAllTextures() = 0;

	//! Eagerly upload buffer to hardware
	/** This can be a good idea if you have a newly created or modified buffer,
	which you know you will draw in the near future (e.g. end of same frame,
	or next frame), because it gives the GPU driver to copy the contents. */
	virtual void updateHardwareBuffer(const scene::IVertexBuffer *vb) = 0;

	//! Eagerly upload buffer to hardware
	/** This can be a good idea if you have a newly created or modified buffer,
	which you know you will draw in the near future (e.g. end of same frame,
	or next frame), because it gives the GPU driver to copy the contents. */
	virtual void updateHardwareBuffer(const scene::IIndexBuffer *ib) = 0;

	//! Remove hardware buffer
	virtual void removeHardwareBuffer(const scene::IVertexBuffer *vb) = 0;

	//! Remove hardware buffer
	virtual void removeHardwareBuffer(const scene::IIndexBuffer *ib) = 0;

	//! Remove all hardware buffers
	virtual void removeAllHardwareBuffers() = 0;

	//! Create occlusion query.
	/** Use node for identification and mesh for occlusion test. */
	virtual void addOcclusionQuery(scene::ISceneNode *node,
			const scene::IMesh *mesh = 0) = 0;

	//! Remove occlusion query.
	virtual void removeOcclusionQuery(scene::ISceneNode *node) = 0;

	//! Remove all occlusion queries.
	virtual void removeAllOcclusionQueries() = 0;

	//! Run occlusion query. Draws mesh stored in query.
	/** If the mesh shall not be rendered visible, use
	overrideMaterial to disable the color and depth buffer. */
	virtual void runOcclusionQuery(scene::ISceneNode *node, bool visible = false) = 0;

	//! Run all occlusion queries. Draws all meshes stored in queries.
	/** If the meshes shall not be rendered visible, use
	overrideMaterial to disable the color and depth buffer. */
	virtual void runAllOcclusionQueries(bool visible = false) = 0;

	//! Update occlusion query. Retrieves results from GPU.
	/** If the query shall not block, set the flag to false.
	Update might not occur in this case, though */
	virtual void updateOcclusionQuery(scene::ISceneNode *node, bool block = true) = 0;

	//! Update all occlusion queries. Retrieves results from GPU.
	/** If the query shall not block, set the flag to false.
	Update might not occur in this case, though */
	virtual void updateAllOcclusionQueries(bool block = true) = 0;

	//! Return query result.
	/** Return value is the number of visible pixels/fragments.
	The value is a safe approximation, i.e. can be larger than the
	actual value of pixels. */
	virtual u32 getOcclusionQueryResult(scene::ISceneNode *node) const = 0;

	//! Create render target.
	virtual IRenderTarget *addRenderTarget() = 0;

	//! Remove render target.
	virtual void removeRenderTarget(IRenderTarget *renderTarget) = 0;

	//! Remove all render targets.
	virtual void removeAllRenderTargets() = 0;

	//! Blit contents of one render target to another one.
	/** This is glBlitFramebuffer in OpenGL. */
	virtual void blitRenderTarget(IRenderTarget *from, IRenderTarget *to) = 0;

	//! Sets a boolean alpha channel on the texture based on a color key.
	/** This makes the texture fully transparent at the texels where
	this color key can be found when using for example draw2DImage
	with useAlphachannel==true.  The alpha of other texels is not modified.
	\param texture Texture whose alpha channel is modified.
	\param color Color key color. Every texel with this color will
	become fully transparent as described above. Please note that the
	colors of a texture may be converted when loading it, so the
	color values may not be exactly the same in the engine and for
	example in picture edit programs. To avoid this problem, you
	could use the makeColorKeyTexture method, which takes the
	position of a pixel instead a color value. */
	virtual void makeColorKeyTexture(video::ITexture *texture,
			video::SColor color) const = 0;

	//! Sets a boolean alpha channel on the texture based on the color at a position.
	/** This makes the texture fully transparent at the texels where
	the color key can be found when using for example draw2DImage
	with useAlphachannel==true.  The alpha of other texels is not modified.
	\param texture Texture whose alpha channel is modified.
	\param colorKeyPixelPos Position of a pixel with the color key
	color. Every texel with this color will become fully transparent as
	described above. */
	virtual void makeColorKeyTexture(video::ITexture *texture,
			core::position2d<s32> colorKeyPixelPos) const = 0;

	//! Set a render target.
	/** This will only work if the driver supports the
	EVDF_RENDER_TO_TARGET feature, which can be queried with
	queryFeature(). Please note that you cannot render 3D or 2D
	geometry with a render target as texture on it when you are rendering
	the scene into this render target at the same time. It is usually only
	possible to render into a texture between the
	IVideoDriver::beginScene() and endScene() method calls. If you need the
	best performance use this method instead of setRenderTarget.
	\param target Render target object. If set to nullptr, it makes the
	window the current render target.
	\param clearFlag A combination of the E_CLEAR_BUFFER_FLAG bit-flags.
	\param clearColor The clear color for the color buffer.
	\param clearDepth The clear value for the depth buffer.
	\param clearStencil The clear value for the stencil buffer.
	\return True if successful and false if not. */
	virtual bool setRenderTargetEx(IRenderTarget *target, u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0),
			f32 clearDepth = 1.f, u8 clearStencil = 0) = 0;

	//! Sets a new render target.
	/** This will only work if the driver supports the
	EVDF_RENDER_TO_TARGET feature, which can be queried with
	queryFeature(). Usually, rendering to textures is done in this
	way:
	\code
	// create render target
	ITexture* target = driver->addRenderTargetTexture(core::dimension2d<u32>(128,128), "rtt1");

	// ...

	driver->setRenderTarget(target); // set render target
	// .. draw stuff here
	driver->setRenderTarget(0); // set previous render target
	\endcode
	Please note that you cannot render 3D or 2D geometry with a
	render target as texture on it when you are rendering the scene
	into this render target at the same time. It is usually only
	possible to render into a texture between the
	IVideoDriver::beginScene() and endScene() method calls.
	\param texture New render target. Must be a texture created with
	IVideoDriver::addRenderTargetTexture(). If set to nullptr, it makes
	the window the current render target.
	\param clearFlag A combination of the E_CLEAR_BUFFER_FLAG bit-flags.
	\param clearColor The clear color for the color buffer.
	\param clearDepth The clear value for the depth buffer.
	\param clearStencil The clear value for the stencil buffer.
	\return True if successful and false if not. */
	virtual bool setRenderTarget(ITexture *texture, u16 clearFlag = ECBF_COLOR | ECBF_DEPTH, SColor clearColor = SColor(255, 0, 0, 0),
			f32 clearDepth = 1.f, u8 clearStencil = 0) = 0;

	//! Sets a new render target.
	//! Prefer to use the setRenderTarget function taking flags as parameter as this one can't clear the stencil buffer.
	//! It's still offered for backward compatibility.
	bool setRenderTarget(ITexture *texture, bool clearBackBuffer, bool clearZBuffer, SColor color = SColor(255, 0, 0, 0))
	{
		u16 flag = 0;

		if (clearBackBuffer)
			flag |= ECBF_COLOR;

		if (clearZBuffer)
			flag |= ECBF_DEPTH;

		return setRenderTarget(texture, flag, color);
	}

	//! Sets a new viewport.
	/** Every rendering operation is done into this new area.
	\param area: Rectangle defining the new area of rendering
	operations. */
	virtual void setViewPort(const core::rect<s32> &area) = 0;

	//! Gets the area of the current viewport.
	/** \return Rectangle of the current viewport. */
	virtual const core::rect<s32> &getViewPort() const = 0;

	//! Draws a vertex primitive list
	/** Note that, depending on the index type, some vertices might be not
	accessible through the index list. The limit is at 65535 vertices for 16bit
	indices. Please note that currently not all primitives are available for
	all drivers, and some might be emulated via triangle renders.
	\param vertices Pointer to array of vertices.
	\param vertexCount Amount of vertices in the array.
	\param indexList Pointer to array of indices. These define the vertices used
	for each primitive. Depending on the pType, indices are interpreted as single
	objects (for point like primitives), pairs (for lines), triplets (for
	triangles), or quads.
	\param primCount Amount of Primitives
	\param vType Vertex type, e.g. video::EVT_STANDARD for S3DVertex.
	\param pType Primitive type, e.g. scene::EPT_TRIANGLE_FAN for a triangle fan.
	\param iType Index type, e.g. video::EIT_16BIT for 16bit indices. */
	virtual void drawVertexPrimitiveList(const void *vertices, u32 vertexCount,
			const void *indexList, u32 primCount,
			E_VERTEX_TYPE vType = EVT_STANDARD,
			scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES,
			E_INDEX_TYPE iType = EIT_16BIT) = 0;

	//! Draws a vertex primitive list in 2d
	/** Compared to the general (3d) version of this method, this
	one sets up a 2d render mode, and uses only x and y of vectors.
	Note that, depending on the index type, some vertices might be
	not accessible through the index list. The limit is at 65535
	vertices for 16bit indices. Please note that currently not all
	primitives are available for all drivers, and some might be
	emulated via triangle renders. This function is not available
	for the sw drivers.
	\param vertices Pointer to array of vertices.
	\param vertexCount Amount of vertices in the array.
	\param indexList Pointer to array of indices. These define the
	vertices used for each primitive. Depending on the pType,
	indices are interpreted as single objects (for point like
	primitives), pairs (for lines), triplets (for triangles), or
	quads.
	\param primCount Amount of Primitives
	\param vType Vertex type, e.g. video::EVT_STANDARD for S3DVertex.
	\param pType Primitive type, e.g. scene::EPT_TRIANGLE_FAN for a triangle fan.
	\param iType Index type, e.g. video::EIT_16BIT for 16bit indices. */
	virtual void draw2DVertexPrimitiveList(const void *vertices, u32 vertexCount,
			const void *indexList, u32 primCount,
			E_VERTEX_TYPE vType = EVT_STANDARD,
			scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES,
			E_INDEX_TYPE iType = EIT_16BIT) = 0;

	//! Draws an indexed triangle list.
	/** Note that there may be at maximum 65536 vertices, because
	the index list is an array of 16 bit values each with a maximum
	value of 65536. If there are more than 65536 vertices in the
	list, results of this operation are not defined.
	\param vertices Pointer to array of vertices.
	\param vertexCount Amount of vertices in the array.
	\param indexList Pointer to array of indices.
	\param triangleCount Amount of Triangles. Usually amount of indices / 3. */
	void drawIndexedTriangleList(const S3DVertex *vertices,
			u32 vertexCount, const u16 *indexList, u32 triangleCount)
	{
		drawVertexPrimitiveList(vertices, vertexCount, indexList, triangleCount, EVT_STANDARD, scene::EPT_TRIANGLES, EIT_16BIT);
	}

	//! Draws an indexed triangle list.
	/** Note that there may be at maximum 65536 vertices, because
	the index list is an array of 16 bit values each with a maximum
	value of 65536. If there are more than 65536 vertices in the
	list, results of this operation are not defined.
	\param vertices Pointer to array of vertices.
	\param vertexCount Amount of vertices in the array.
	\param indexList Pointer to array of indices.
	\param triangleCount Amount of Triangles. Usually amount of indices / 3. */
	void drawIndexedTriangleList(const S3DVertex2TCoords *vertices,
			u32 vertexCount, const u16 *indexList, u32 triangleCount)
	{
		drawVertexPrimitiveList(vertices, vertexCount, indexList, triangleCount, EVT_2TCOORDS, scene::EPT_TRIANGLES, EIT_16BIT);
	}

	//! Draws an indexed triangle list.
	/** Note that there may be at maximum 65536 vertices, because
	the index list is an array of 16 bit values each with a maximum
	value of 65536. If there are more than 65536 vertices in the
	list, results of this operation are not defined.
	\param vertices Pointer to array of vertices.
	\param vertexCount Amount of vertices in the array.
	\param indexList Pointer to array of indices.
	\param triangleCount Amount of Triangles. Usually amount of indices / 3. */
	void drawIndexedTriangleList(const S3DVertexTangents *vertices,
			u32 vertexCount, const u16 *indexList, u32 triangleCount)
	{
		drawVertexPrimitiveList(vertices, vertexCount, indexList, triangleCount, EVT_TANGENTS, scene::EPT_TRIANGLES, EIT_16BIT);
	}

	//! Draws an indexed triangle fan.
	/** Note that there may be at maximum 65536 vertices, because
	the index list is an array of 16 bit values each with a maximum
	value of 65536. If there are more than 65536 vertices in the
	list, results of this operation are not defined.
	\param vertices Pointer to array of vertices.
	\param vertexCount Amount of vertices in the array.
	\param indexList Pointer to array of indices.
	\param triangleCount Amount of Triangles. Usually amount of indices - 2. */
	void drawIndexedTriangleFan(const S3DVertex *vertices,
			u32 vertexCount, const u16 *indexList, u32 triangleCount)
	{
		drawVertexPrimitiveList(vertices, vertexCount, indexList, triangleCount, EVT_STANDARD, scene::EPT_TRIANGLE_FAN, EIT_16BIT);
	}

	//! Draws an indexed triangle fan.
	/** Note that there may be at maximum 65536 vertices, because
	the index list is an array of 16 bit values each with a maximum
	value of 65536. If there are more than 65536 vertices in the
	list, results of this operation are not defined.
	\param vertices Pointer to array of vertices.
	\param vertexCount Amount of vertices in the array.
	\param indexList Pointer to array of indices.
	\param triangleCount Amount of Triangles. Usually amount of indices - 2. */
	void drawIndexedTriangleFan(const S3DVertex2TCoords *vertices,
			u32 vertexCount, const u16 *indexList, u32 triangleCount)
	{
		drawVertexPrimitiveList(vertices, vertexCount, indexList, triangleCount, EVT_2TCOORDS, scene::EPT_TRIANGLE_FAN, EIT_16BIT);
	}

	//! Draws an indexed triangle fan.
	/** Note that there may be at maximum 65536 vertices, because
	the index list is an array of 16 bit values each with a maximum
	value of 65536. If there are more than 65536 vertices in the
	list, results of this operation are not defined.
	\param vertices Pointer to array of vertices.
	\param vertexCount Amount of vertices in the array.
	\param indexList Pointer to array of indices.
	\param triangleCount Amount of Triangles. Usually amount of indices - 2. */
	void drawIndexedTriangleFan(const S3DVertexTangents *vertices,
			u32 vertexCount, const u16 *indexList, u32 triangleCount)
	{
		drawVertexPrimitiveList(vertices, vertexCount, indexList, triangleCount, EVT_TANGENTS, scene::EPT_TRIANGLE_FAN, EIT_16BIT);
	}

	//! Draws a 3d line.
	/** For some implementations, this method simply calls
	drawVertexPrimitiveList for some triangles.
	Note that the line is drawn using the current transformation
	matrix and material. So if you need to draw the 3D line
	independently of the current transformation, use
	\code
	driver->setMaterial(someMaterial);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	\endcode
	for some properly set up material before drawing the line.
	Some drivers support line thickness set in the material.
	\param start Start of the 3d line.
	\param end End of the 3d line.
	\param color Color of the line. */
	virtual void draw3DLine(const core::vector3df &start,
			const core::vector3df &end, SColor color = SColor(255, 255, 255, 255)) = 0;

	//! Draws a 3d axis aligned box.
	/** This method simply calls draw3DLine for the edges of the
	box. Note that the box is drawn using the current transformation
	matrix and material. So if you need to draw it independently of
	the current transformation, use
	\code
	driver->setMaterial(someMaterial);
	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	\endcode
	for some properly set up material before drawing the box.
	\param box The axis aligned box to draw
	\param color Color to use while drawing the box. */
	virtual void draw3DBox(const core::aabbox3d<f32> &box,
			SColor color = SColor(255, 255, 255, 255)) = 0;

	//! Draws a 2d image without any special effects
	/** \param texture Pointer to texture to use.
	\param destPos Upper left 2d destination position where the
	image will be drawn.
	\param useAlphaChannelOfTexture: If true, the alpha channel of
	the texture is used to draw the image.*/
	virtual void draw2DImage(const video::ITexture *texture,
			const core::position2d<s32> &destPos, bool useAlphaChannelOfTexture = false) = 0;

	//! Draws a 2d image using a color
	/** (if color is other than
	Color(255,255,255,255)) and the alpha channel of the texture.
	\param texture Texture to be drawn.
	\param destPos Upper left 2d destination position where the
	image will be drawn.
	\param sourceRect Source rectangle in the texture (based on it's OriginalSize)
	\param clipRect Pointer to rectangle on the screen where the
	image is clipped to.
	If this pointer is NULL the image is not clipped.
	\param color Color with which the image is drawn. If the color
	equals Color(255,255,255,255) it is ignored. Note that the
	alpha component is used: If alpha is other than 255, the image
	will be transparent.
	\param useAlphaChannelOfTexture: If true, the alpha channel of
	the texture is used to draw the image.*/
	virtual void draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			SColor color = SColor(255, 255, 255, 255), bool useAlphaChannelOfTexture = false) = 0;

	//! Draws a set of 2d images, using a color and the alpha channel of the texture.
	/** All drawings are clipped against clipRect (if != 0).
	The subtextures are defined by the array of sourceRects and are
	positioned using the array of positions.
	\param texture Texture to be drawn.
	\param positions Array of upper left 2d destinations where the
	images will be drawn.
	\param sourceRects Source rectangles of the texture (based on it's OriginalSize)
	\param clipRect Pointer to rectangle on the screen where the
	images are clipped to.
	If this pointer is 0 then the image is not clipped.
	\param color Color with which the image is drawn.
	Note that the alpha component is used. If alpha is other than
	255, the image will be transparent.
	\param useAlphaChannelOfTexture: If true, the alpha channel of
	the texture is used to draw the image. */
	virtual void draw2DImageBatch(const video::ITexture *texture,
			const core::array<core::position2d<s32>> &positions,
			const core::array<core::rect<s32>> &sourceRects,
			const core::rect<s32> *clipRect = 0,
			SColor color = SColor(255, 255, 255, 255),
			bool useAlphaChannelOfTexture = false) = 0;

	//! Draws a part of the texture into the rectangle. Note that colors must be an array of 4 colors if used.
	/** Suggested and first implemented by zola.
	\param texture The texture to draw from
	\param destRect The rectangle to draw into
	\param sourceRect The rectangle denoting a part of the texture (based on it's OriginalSize)
	\param clipRect Clips the destination rectangle (may be 0)
	\param colors Array of 4 colors denoting the color values of
	the corners of the destRect
	\param useAlphaChannelOfTexture True if alpha channel will be
	blended. */
	virtual void draw2DImage(const video::ITexture *texture, const core::rect<s32> &destRect,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			const video::SColor *const colors = 0, bool useAlphaChannelOfTexture = false) = 0;

	//! Draws a 2d rectangle.
	/** \param color Color of the rectangle to draw. The alpha
	component will not be ignored and specifies how transparent the
	rectangle will be.
	\param pos Position of the rectangle.
	\param clip Pointer to rectangle against which the rectangle
	will be clipped. If the pointer is null, no clipping will be
	performed. */
	virtual void draw2DRectangle(SColor color, const core::rect<s32> &pos,
			const core::rect<s32> *clip = 0) = 0;

	//! Draws a 2d rectangle with a gradient.
	/** \param colorLeftUp Color of the upper left corner to draw.
	The alpha component will not be ignored and specifies how
	transparent the rectangle will be.
	\param colorRightUp Color of the upper right corner to draw.
	The alpha component will not be ignored and specifies how
	transparent the rectangle will be.
	\param colorLeftDown Color of the lower left corner to draw.
	The alpha component will not be ignored and specifies how
	transparent the rectangle will be.
	\param colorRightDown Color of the lower right corner to draw.
	The alpha component will not be ignored and specifies how
	transparent the rectangle will be.
	\param pos Position of the rectangle.
	\param clip Pointer to rectangle against which the rectangle
	will be clipped. If the pointer is null, no clipping will be
	performed. */
	virtual void draw2DRectangle(const core::rect<s32> &pos,
			SColor colorLeftUp, SColor colorRightUp,
			SColor colorLeftDown, SColor colorRightDown,
			const core::rect<s32> *clip = 0) = 0;

	//! Draws a 2d line.
	/** In theory both start and end will be included in coloring.
	BUG: Currently d3d ignores the last pixel
	(it uses the so called "diamond exit rule" for drawing lines).
	\param start Screen coordinates of the start of the line
	in pixels.
	\param end Screen coordinates of the start of the line in
	pixels.
	\param color Color of the line to draw. */
	virtual void draw2DLine(const core::position2d<s32> &start,
			const core::position2d<s32> &end,
			SColor color = SColor(255, 255, 255, 255)) = 0;

	//! Draws a mesh buffer
	/** \param mb Buffer to draw */
	virtual void drawMeshBuffer(const scene::IMeshBuffer *mb) = 0;

	/**
	 * Draws a mesh from individual vertex and index buffers.
	 * @param vb vertices to use
	 * @param ib indices to use
	 * @param primCount amount of primitives
	 * @param pType primitive type
	 */
	virtual void drawBuffers(const scene::IVertexBuffer *vb,
		const scene::IIndexBuffer *ib, u32 primCount,
		scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES) = 0;

	//! Draws normals of a mesh buffer
	/** \param mb Buffer to draw the normals of
	\param length length scale factor of the normals
	\param color Color the normals are rendered with
	*/
	virtual void drawMeshBufferNormals(const scene::IMeshBuffer *mb, f32 length = 10.f, SColor color = 0xffffffff) = 0;

	//! Sets the fog mode.
	/** These are global values attached to each 3d object rendered,
	which has the fog flag enabled in its material.
	\param color Color of the fog
	\param fogType Type of fog used
	\param start Only used in linear fog mode (linearFog=true).
	Specifies where fog starts.
	\param end Only used in linear fog mode (linearFog=true).
	Specifies where fog ends.
	\param density Only used in exponential fog mode
	(linearFog=false). Must be a value between 0 and 1.
	\param pixelFog Set this to false for vertex fog, and true if
	you want per-pixel fog.
	\param rangeFog Set this to true to enable range-based vertex
	fog. The distance from the viewer is used to compute the fog,
	not the z-coordinate. This is better, but slower. This might not
	be available with all drivers and fog settings. */
	virtual void setFog(SColor color = SColor(0, 255, 255, 255),
			E_FOG_TYPE fogType = EFT_FOG_LINEAR,
			f32 start = 50.0f, f32 end = 100.0f, f32 density = 0.01f,
			bool pixelFog = false, bool rangeFog = false) = 0;

	//! Gets the fog mode.
	virtual void getFog(SColor &color, E_FOG_TYPE &fogType,
			f32 &start, f32 &end, f32 &density,
			bool &pixelFog, bool &rangeFog) = 0;

	//! Get the current color format of the color buffer
	/** \return Color format of the color buffer. */
	virtual ECOLOR_FORMAT getColorFormat() const = 0;

	//! Get the size of the screen or render window.
	/** \return Size of screen or render window. */
	virtual const core::dimension2d<u32> &getScreenSize() const = 0;

	//! Get the size of the current render target
	/** This method will return the screen size if the driver
	doesn't support render to texture, or if the current render
	target is the screen.
	\return Size of render target or screen/window */
	virtual const core::dimension2d<u32> &getCurrentRenderTargetSize() const = 0;

	//! Returns current frames per second value.
	/** This value is updated approximately every 1.5 seconds and
	is only intended to provide a rough guide to the average frame
	rate. It is not suitable for use in performing timing
	calculations or framerate independent movement.
	\return Approximate amount of frames per second drawn. */
	virtual s32 getFPS() const = 0;

	//! Return some statistics about the last frame
	virtual SFrameStats getFrameStats() const = 0;

	//! Gets name of this video driver.
	/** \return Returns the name of the video driver, e.g. in case
	of the Direct3D8 driver, it would return "Direct3D 8.1". */
	virtual const char *getName() const = 0;

	//! Adds an external image loader to the engine.
	/** This is useful if the Irrlicht Engine should be able to load
	textures of currently unsupported file formats (e.g. gif). The
	IImageLoader only needs to be implemented for loading this file
	format. A pointer to the implementation can be passed to the
	engine using this method.
	\param loader Pointer to the external loader created. */
	virtual void addExternalImageLoader(IImageLoader *loader) = 0;

	//! Adds an external image writer to the engine.
	/** This is useful if the Irrlicht Engine should be able to
	write textures of currently unsupported file formats (e.g
	.gif). The IImageWriter only needs to be implemented for
	writing this file format. A pointer to the implementation can
	be passed to the engine using this method.
	\param writer: Pointer to the external writer created. */
	virtual void addExternalImageWriter(IImageWriter *writer) = 0;

	//! Returns the maximum amount of primitives
	/** (mostly vertices) which the device is able to render with
	one drawVertexPrimitiveList call.
	\return Maximum amount of primitives. */
	virtual u32 getMaximalPrimitiveCount() const = 0;

	//! Enables or disables a texture creation flag.
	/** These flags define how textures should be created. By
	changing this value, you can influence for example the speed of
	rendering a lot. But please note that the video drivers take
	this value only as recommendation. It could happen that you
	enable the ETCF_ALWAYS_16_BIT mode, but the driver still creates
	32 bit textures.
	\param flag Texture creation flag.
	\param enabled Specifies if the given flag should be enabled or
	disabled. */
	virtual void setTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag, bool enabled = true) = 0;

	//! Returns if a texture creation flag is enabled or disabled.
	/** You can change this value using setTextureCreationFlag().
	\param flag Texture creation flag.
	\return The current texture creation flag enabled mode. */
	virtual bool getTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag) const = 0;

	//! Creates a software image from a file.
	/** No hardware texture will be created for this image. This
	method is useful for example if you want to read a heightmap
	for a terrain renderer.
	\param filename Name of the file from which the image is
	created.
	\return The created image.
	If you no longer need the image, you should call IImage::drop().
	See IReferenceCounted::drop() for more information. */
	virtual IImage *createImageFromFile(const io::path &filename) = 0;

	//! Creates a software image from a file.
	/** No hardware texture will be created for this image. This
	method is useful for example if you want to read a heightmap
	for a terrain renderer.
	\param file File from which the image is created.
	\return The created image.
	If you no longer need the image, you should call IImage::drop().
	See IReferenceCounted::drop() for more information. */
	virtual IImage *createImageFromFile(io::IReadFile *file) = 0;

	//! Writes the provided image to a file.
	/** Requires that there is a suitable image writer registered
	for writing the image.
	\param image Image to write.
	\param filename Name of the file to write.
	\param param Control parameter for the backend (e.g. compression
	level).
	\return True on successful write. */
	virtual bool writeImageToFile(IImage *image, const io::path &filename, u32 param = 0) = 0;

	//! Writes the provided image to a file.
	/** Requires that there is a suitable image writer registered
	for writing the image.
	\param image Image to write.
	\param file  An already open io::IWriteFile object. The name
	will be used to determine the appropriate image writer to use.
	\param param Control parameter for the backend (e.g. compression
	level).
	\return True on successful write. */
	virtual bool writeImageToFile(IImage *image, io::IWriteFile *file, u32 param = 0) = 0;

	//! Creates a software image from a byte array.
	/** No hardware texture will be created for this image. This
	method is useful for example if you want to read a heightmap
	for a terrain renderer.
	\param format Desired color format of the texture
	\param size Desired size of the image
	\param data A byte array with pixel color information
	\param ownForeignMemory If true, the image will use the data
	pointer directly and own it afterward. If false, the memory
	will by copied internally.
	WARNING: Setting this to 'true' will not work across dll boundaries.
	So unless you link Irrlicht statically you should keep this to 'false'.
	The parameter is mainly for internal usage.
	\param deleteMemory Whether the memory is deallocated upon
	destruction.
	\return The created image.
	If you no longer need the image, you should call IImage::drop().
	See IReferenceCounted::drop() for more information. */
	virtual IImage *createImageFromData(ECOLOR_FORMAT format,
			const core::dimension2d<u32> &size, void *data, bool ownForeignMemory = false,
			bool deleteMemory = true) = 0;

	//! Creates an empty software image.
	/**
	\param format Desired color format of the image.
	\param size Size of the image to create.
	\return The created image.
	If you no longer need the image, you should call IImage::drop().
	See IReferenceCounted::drop() for more information. */
	virtual IImage *createImage(ECOLOR_FORMAT format, const core::dimension2d<u32> &size) = 0;

	//! Creates a software image from a part of a texture.
	/**
	\param texture Texture to copy to the new image in part.
	\param pos Position of rectangle to copy.
	\param size Extents of rectangle to copy.
	\return The created image.
	If you no longer need the image, you should call IImage::drop().
	See IReferenceCounted::drop() for more information. */
	virtual IImage *createImage(ITexture *texture,
			const core::position2d<s32> &pos,
			const core::dimension2d<u32> &size) = 0;

	//! Event handler for resize events. Only used by the engine internally.
	/** Used to notify the driver that the window was resized.
	Usually, there is no need to call this method. */
	virtual void OnResize(const core::dimension2d<u32> &size) = 0;

	//! Adds a new material renderer to the video device.
	/** Use this method to extend the VideoDriver with new material
	types. To extend the engine using this method do the following:
	Derive a class from IMaterialRenderer and override the methods
	you need. For setting the right renderstates, you can try to
	get a pointer to the real rendering device using
	IVideoDriver::getExposedVideoData(). Add your class with
	IVideoDriver::addMaterialRenderer(). To use an object being
	displayed with your new material, set the MaterialType member of
	the SMaterial struct to the value returned by this method.
	If you simply want to create a new material using vertex and/or
	pixel shaders it would be easier to use the
	video::IGPUProgrammingServices interface which you can get
	using the getGPUProgrammingServices() method.
	\param renderer A pointer to the new renderer.
	\param name Optional name for the material renderer entry.
	\return The number of the material type which can be set in
	SMaterial::MaterialType to use the renderer. -1 is returned if
	an error occurred. For example if you tried to add an material
	renderer to the software renderer or the null device, which do
	not accept material renderers. */
	virtual s32 addMaterialRenderer(IMaterialRenderer *renderer, const c8 *name = 0) = 0;

	//! Get access to a material renderer by index.
	/** \param idx Id of the material renderer. Can be a value of
	the E_MATERIAL_TYPE enum or a value which was returned by
	addMaterialRenderer().
	\return Pointer to material renderer or null if not existing. */
	virtual IMaterialRenderer *getMaterialRenderer(u32 idx) const = 0;

	//! Get amount of currently available material renderers.
	/** \return Amount of currently available material renderers. */
	virtual u32 getMaterialRendererCount() const = 0;

	//! Get name of a material renderer
	/** This string can, e.g., be used to test if a specific
	renderer already has been registered/created, or use this
	string to store data about materials: This returned name will
	be also used when serializing materials.
	\param idx Id of the material renderer. Can be a value of the
	E_MATERIAL_TYPE enum or a value which was returned by
	addMaterialRenderer().
	\return String with the name of the renderer, or 0 if not
	existing */
	virtual const c8 *getMaterialRendererName(u32 idx) const = 0;

	//! Sets the name of a material renderer.
	/** Will have no effect on built-in material renderers.
	\param idx: Id of the material renderer. Can be a value of the
	E_MATERIAL_TYPE enum or a value which was returned by
	addMaterialRenderer().
	\param name: New name of the material renderer. */
	virtual void setMaterialRendererName(u32 idx, const c8 *name) = 0;

	//! Swap the material renderers used for certain id's
	/** Swap the IMaterialRenderers responsible for rendering specific
	 material-id's. This means every SMaterial using a MaterialType
	 with one of the indices involved here will now render differently.
	 \param idx1 First material index to swap. It must already exist or nothing happens.
	 \param idx2 Second material index to swap. It must already exist or nothing happens.
	 \param swapNames When true the renderer names also swap
					  When false the names will stay at the original index */
	virtual void swapMaterialRenderers(u32 idx1, u32 idx2, bool swapNames = true) = 0;

	//! Returns driver and operating system specific data about the IVideoDriver.
	/** This method should only be used if the engine should be
	extended without having to modify the source of the engine.
	\return Collection of device dependent pointers. */
	virtual const SExposedVideoData &getExposedVideoData() = 0;

	//! Get type of video driver
	/** \return Type of driver. */
	virtual E_DRIVER_TYPE getDriverType() const = 0;

	//! Gets the IGPUProgrammingServices interface.
	/** \return Pointer to the IGPUProgrammingServices. Returns 0
	if the video driver does not support this. For example the
	Software driver and the Null driver will always return 0. */
	virtual IGPUProgrammingServices *getGPUProgrammingServices() = 0;

	//! Returns a pointer to the mesh manipulator.
	virtual scene::IMeshManipulator *getMeshManipulator() = 0;

	//! Clear the color, depth and/or stencil buffers.
	virtual void clearBuffers(u16 flag, SColor color = SColor(255, 0, 0, 0), f32 depth = 1.f, u8 stencil = 0) = 0;

	//! Clears the ZBuffer.
	/** Note that you usually need not to call this method, as it
	is automatically done in IVideoDriver::beginScene() or
	IVideoDriver::setRenderTarget() if you enable zBuffer. But if
	you have to render some special things, you can clear the
	zbuffer during the rendering process with this method any time.
	*/
	void clearZBuffer()
	{
		clearBuffers(ECBF_DEPTH, SColor(255, 0, 0, 0), 1.f, 0);
	}

	//! Make a screenshot of the last rendered frame.
	/** \return An image created from the last rendered frame. */
	virtual IImage *createScreenShot(video::ECOLOR_FORMAT format = video::ECF_UNKNOWN, video::E_RENDER_TARGET target = video::ERT_FRAME_BUFFER) = 0;

	//! Check if the image is already loaded.
	/** Works similar to getTexture(), but does not load the texture
	if it is not currently loaded.
	\param filename Name of the texture.
	\return Pointer to loaded texture, or 0 if not found. */
	virtual video::ITexture *findTexture(const io::path &filename) = 0;

	//! Set the minimum number of vertices for which a hw buffer will be created
	/** \param count Number of vertices to set as minimum. */
	virtual void setMinHardwareBufferVertexCount(u32 count) = 0;

	//! Get the global Material, which might override local materials.
	/** Depending on the enable flags, values from this Material
	are used to override those of local materials of some
	meshbuffer being rendered.
	\return Reference to the Override Material. */
	virtual SOverrideMaterial &getOverrideMaterial() = 0;

	//! Get the 2d override material for altering its values
	/** The 2d override material allows to alter certain render
	states of the 2d methods. Not all members of SMaterial are
	honored, especially not MaterialType and Textures. Moreover,
	the zbuffer is always ignored, and lighting is always off. All
	other flags can be changed, though some might have to effect
	in most cases.
	Please note that you have to enable/disable this effect with
	enableMaterial2D(). This effect is costly, as it increases
	the number of state changes considerably. Always reset the
	values when done.
	\return Material reference which should be altered to reflect
	the new settings.
	*/
	virtual SMaterial &getMaterial2D() = 0;

	//! Enable the 2d override material
	/** \param enable Flag which tells whether the material shall be
	enabled or disabled. */
	virtual void enableMaterial2D(bool enable = true) = 0;

	//! Get the graphics card vendor name.
	virtual core::stringc getVendorInfo() = 0;

	//! Only used by the engine internally.
	/** Passes the global material flag AllowZWriteOnTransparent.
	\param flag Default behavior is to disable ZWrite, i.e. false. */
	virtual void setAllowZWriteOnTransparent(bool flag) = 0;

	//! Get the maximum texture size supported.
	virtual core::dimension2du getMaxTextureSize() const = 0;

	//! Check if the driver supports creating textures with the given color format
	/**	\return True if the format is available, false if not. */
	virtual bool queryTextureFormat(ECOLOR_FORMAT format) const = 0;

	//! Used by some SceneNodes to check if a material should be rendered in the transparent render pass
	virtual bool needsTransparentRenderPass(const video::SMaterial &material) const = 0;
};

} // end namespace video
