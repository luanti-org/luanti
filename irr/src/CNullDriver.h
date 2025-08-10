// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IVideoDriver.h"
#include "IFileSystem.h"
#include "IGPUProgrammingServices.h"
#include "irrArray.h"
#include "irrString.h"
#include "IMesh.h"
#include "IMeshBuffer.h"
#include "IMeshSceneNode.h"
#include "CFPSCounter.h"
#include "S3DVertex.h"
#include "SVertexIndex.h"
#include "SExposedVideoData.h"

namespace io
{
class IWriteFile;
class IReadFile;
} // end namespace io
namespace video
{
class IImageLoader;
class IImageWriter;

class CNullDriver : public IVideoDriver, public IGPUProgrammingServices
{
public:
	//! constructor
	CNullDriver(io::IFileSystem *io, const core::dimension2d<u32> &screenSize);

	//! destructor
	virtual ~CNullDriver();

	virtual bool beginScene(u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0), f32 clearDepth = 1.f, u8 clearStencil = 0,
			const SExposedVideoData &videoData = SExposedVideoData(), core::rect<s32> *sourceRect = 0) override;

	bool endScene() override;

	//! Disable a feature of the driver.
	void disableFeature(E_VIDEO_DRIVER_FEATURE feature, bool flag = true) override;

	//! queries the features of the driver, returns true if feature is available
	bool queryFeature(E_VIDEO_DRIVER_FEATURE feature) const override;

	//! sets transformation
	void setTransform(E_TRANSFORMATION_STATE state, const core::matrix4 &mat) override;

	//! Retrieve the number of image loaders
	u32 getImageLoaderCount() const override;

	//! Retrieve the given image loader
	IImageLoader *getImageLoader(u32 n) override;

	//! Retrieve the number of image writers
	u32 getImageWriterCount() const override;

	//! Retrieve the given image writer
	IImageWriter *getImageWriter(u32 n) override;

	//! sets a material
	void setMaterial(const SMaterial &material) override;

	//! loads a Texture
	ITexture *getTexture(const io::path &filename) override;

	//! loads a Texture
	ITexture *getTexture(io::IReadFile *file) override;

	//! Returns amount of textures currently loaded
	u32 getTextureCount() const override;

	ITexture *addTexture(const core::dimension2d<u32> &size, const io::path &name, ECOLOR_FORMAT format = ECF_A8R8G8B8) override;

	ITexture *addTexture(const io::path &name, IImage *image) override;

	ITexture *addArrayTexture(const io::path &name, IImage **images, u32 count) override;

	virtual ITexture *addTextureCubemap(const io::path &name, IImage *imagePosX, IImage *imageNegX, IImage *imagePosY,
			IImage *imageNegY, IImage *imagePosZ, IImage *imageNegZ) override;

	ITexture *addTextureCubemap(const u32 sideLen, const io::path &name, ECOLOR_FORMAT format = ECF_A8R8G8B8) override;

	virtual bool setRenderTargetEx(IRenderTarget *target, u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0),
			f32 clearDepth = 1.f, u8 clearStencil = 0) override;

	virtual bool setRenderTarget(ITexture *texture, u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0),
			f32 clearDepth = 1.f, u8 clearStencil = 0) override;

	//! sets a viewport
	void setViewPort(const core::rect<s32> &area) override;

	//! gets the area of the current viewport
	const core::rect<s32> &getViewPort() const override;

	//! draws a vertex primitive list
	virtual void drawVertexPrimitiveList(const void *vertices, u32 vertexCount,
			const void *indexList, u32 primitiveCount,
			E_VERTEX_TYPE vType = EVT_STANDARD, scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES,
			E_INDEX_TYPE iType = EIT_16BIT) override;

	//! draws a vertex primitive list in 2d
	virtual void draw2DVertexPrimitiveList(const void *vertices, u32 vertexCount,
			const void *indexList, u32 primitiveCount,
			E_VERTEX_TYPE vType = EVT_STANDARD, scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES,
			E_INDEX_TYPE iType = EIT_16BIT) override;

	//! Draws a 3d line.
	virtual void draw3DLine(const core::vector3df &start,
			const core::vector3df &end, SColor color = SColor(255, 255, 255, 255)) override;

	//! Draws a 3d axis aligned box.
	virtual void draw3DBox(const core::aabbox3d<f32> &box,
			SColor color = SColor(255, 255, 255, 255)) override;

	//! draws an 2d image
	void draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos, bool useAlphaChannelOfTexture) override;

	//! Draws a set of 2d images, using a color and the alpha channel of the texture.
	/** All drawings are clipped against clipRect (if != 0).
	The subtextures are defined by the array of sourceRects and are
	positioned using the array of positions.
	\param texture Texture to be drawn.
	\param pos Array of upper left 2d destinations where the images
	will be drawn.
	\param sourceRects Source rectangles of the image.
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
			bool useAlphaChannelOfTexture = false) override;

	//! Draws a 2d image, using a color (if color is other then Color(255,255,255,255)) and the alpha channel of the texture if wanted.
	virtual void draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			SColor color = SColor(255, 255, 255, 255), bool useAlphaChannelOfTexture = false) override;

	//! Draws a part of the texture into the rectangle.
	virtual void draw2DImage(const video::ITexture *texture, const core::rect<s32> &destRect,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			const video::SColor *const colors = 0, bool useAlphaChannelOfTexture = false) override;

	//! Draws a 2d rectangle
	void draw2DRectangle(SColor color, const core::rect<s32> &pos, const core::rect<s32> *clip = 0) override;

	//! Draws a 2d rectangle with a gradient.
	virtual void draw2DRectangle(const core::rect<s32> &pos,
			SColor colorLeftUp, SColor colorRightUp, SColor colorLeftDown, SColor colorRightDown,
			const core::rect<s32> *clip = 0) override;

	//! Draws a 2d line.
	virtual void draw2DLine(const core::position2d<s32> &start,
			const core::position2d<s32> &end,
			SColor color = SColor(255, 255, 255, 255)) override;

	virtual void setFog(SColor color = SColor(0, 255, 255, 255),
			E_FOG_TYPE fogType = EFT_FOG_LINEAR,
			f32 start = 50.0f, f32 end = 100.0f, f32 density = 0.01f,
			bool pixelFog = false, bool rangeFog = false) override;

	virtual void getFog(SColor &color, E_FOG_TYPE &fogType,
			f32 &start, f32 &end, f32 &density,
			bool &pixelFog, bool &rangeFog) override;

	//! get color format of the current color buffer
	ECOLOR_FORMAT getColorFormat() const override;

	//! get screen size
	const core::dimension2d<u32> &getScreenSize() const override;

	//! get current render target
	IRenderTarget *getCurrentRenderTarget() const;

	//! get render target size
	const core::dimension2d<u32> &getCurrentRenderTargetSize() const override;

	// get current frames per second value
	s32 getFPS() const override;

	SFrameStats getFrameStats() const override;

	//! \return Returns the name of the video driver. Example: In case of the DIRECT3D8
	//! driver, it would return "Direct3D8.1".
	const char *getName() const override;

	//! Adds an external image loader to the engine.
	void addExternalImageLoader(IImageLoader *loader) override;

	//! Adds an external image writer to the engine.
	void addExternalImageWriter(IImageWriter *writer) override;

	//! Removes a texture from the texture cache and deletes it, freeing lot of
	//! memory.
	void removeTexture(ITexture *texture) override;

	//! Removes all texture from the texture cache and deletes them, freeing lot of
	//! memory.
	void removeAllTextures() override;

	//! Creates a render target texture.
	virtual ITexture *addRenderTargetTexture(const core::dimension2d<u32> &size,
			const io::path &name, const ECOLOR_FORMAT format = ECF_UNKNOWN) override;

	//! Creates a multisampled render target texture.
	virtual ITexture *addRenderTargetTextureMs(const core::dimension2d<u32> &size, u8 msaa,
			const io::path &name, const ECOLOR_FORMAT format = ECF_UNKNOWN) override;

	//! Creates a render target texture for a cubemap
	ITexture *addRenderTargetTextureCubemap(const u32 sideLen,
			const io::path &name, const ECOLOR_FORMAT format) override;

	//! Creates an 1bit alpha channel of the texture based of an color key.
	void makeColorKeyTexture(video::ITexture *texture, video::SColor color) const override;

	//! Creates an 1bit alpha channel of the texture based of an color key position.
	virtual void makeColorKeyTexture(video::ITexture *texture,
			core::position2d<s32> colorKeyPixelPos) const override;

	//! Returns the maximum amount of primitives (mostly vertices) which
	//! the device is able to render with one drawIndexedTriangleList
	//! call.
	u32 getMaximalPrimitiveCount() const override;

	//! Enables or disables a texture creation flag.
	void setTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag, bool enabled) override;

	//! Returns if a texture creation flag is enabled or disabled.
	bool getTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag) const override;

	IImage *createImageFromFile(const io::path &filename) override;

	IImage *createImageFromFile(io::IReadFile *file) override;

	//! Creates a software image from a byte array.
	/** \param useForeignMemory: If true, the image will use the data pointer
	directly and own it from now on, which means it will also try to delete [] the
	data when the image will be destructed. If false, the memory will by copied. */
	virtual IImage *createImageFromData(ECOLOR_FORMAT format,
			const core::dimension2d<u32> &size, void *data, bool ownForeignMemory = false,
			bool deleteMemory = true) override;

	//! Creates an empty software image.
	IImage *createImage(ECOLOR_FORMAT format, const core::dimension2d<u32> &size) override;

	//! Creates a software image from part of a texture.
	virtual IImage *createImage(ITexture *texture,
			const core::position2d<s32> &pos,
			const core::dimension2d<u32> &size) override;

	void drawMeshBuffer(const scene::IMeshBuffer *mb) override
	{
		if (!mb)
			return;
		drawBuffers(mb->getVertexBuffer(), mb->getIndexBuffer(),
			mb->getPrimitiveCount(), mb->getPrimitiveType());
	}

	// Note: this should handle hw buffers
	virtual void drawBuffers(const scene::IVertexBuffer *vb,
		const scene::IIndexBuffer *ib, u32 primCount,
		scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES) override;

	//! Draws the normals of a mesh buffer
	virtual void drawMeshBufferNormals(const scene::IMeshBuffer *mb, f32 length = 10.f,
			SColor color = 0xffffffff) override;

	//! Check if the driver supports creating textures with the given color format
	bool queryTextureFormat(ECOLOR_FORMAT format) const override
	{
		return false;
	}

protected:
	/// Links a hardware buffer to either a vertex or index buffer
	struct SHWBufferLink
	{
		SHWBufferLink(const scene::IVertexBuffer *vb) :
				VertexBuffer(vb), IsVertex(true)
		{
			if (VertexBuffer) {
				VertexBuffer->grab();
				VertexBuffer->setHWBuffer(this);
			}
		}
		SHWBufferLink(const scene::IIndexBuffer *ib) :
				IndexBuffer(ib), IsVertex(false)
		{
			if (IndexBuffer) {
				IndexBuffer->grab();
				IndexBuffer->setHWBuffer(this);
			}
		}

		virtual ~SHWBufferLink()
		{
			if (IsVertex && VertexBuffer) {
				VertexBuffer->setHWBuffer(nullptr);
				VertexBuffer->drop();
			} else if (!IsVertex && IndexBuffer) {
				IndexBuffer->setHWBuffer(nullptr);
				IndexBuffer->drop();
			}
		}

		union {
			const scene::IVertexBuffer *VertexBuffer;
			const scene::IIndexBuffer *IndexBuffer;
		};
		size_t ListPosition = static_cast<size_t>(-1);
		u32 ChangedID = 0;
		bool IsVertex;
	};

	//! Gets hardware buffer link from a vertex buffer (may create or update buffer)
	virtual SHWBufferLink *getBufferLink(const scene::IVertexBuffer *mb);

	//! Gets hardware buffer link from a index buffer (may create or update buffer)
	virtual SHWBufferLink *getBufferLink(const scene::IIndexBuffer *mb);

	//! updates hardware buffer if needed  (only some drivers can)
	virtual bool updateHardwareBuffer(SHWBufferLink *HWBuffer) { return false; }

	//! Delete hardware buffer
	virtual void deleteHardwareBuffer(SHWBufferLink *HWBuffer);

	//! Create hardware buffer from vertex buffer
	virtual SHWBufferLink *createHardwareBuffer(const scene::IVertexBuffer *vb) { return 0; }

	//! Create hardware buffer from index buffer
	virtual SHWBufferLink *createHardwareBuffer(const scene::IIndexBuffer *ib) { return 0; }

public:
	virtual void updateHardwareBuffer(const scene::IVertexBuffer *vb) override;

	virtual void updateHardwareBuffer(const scene::IIndexBuffer *ib) override;

	//! Remove hardware buffer
	void removeHardwareBuffer(const scene::IVertexBuffer *vb) override;

	//! Remove hardware buffer
	void removeHardwareBuffer(const scene::IIndexBuffer *ib) override;

	//! Remove all hardware buffers
	void removeAllHardwareBuffers() override;

	//! Run garbage-collection on all HW buffers
	void expireHardwareBuffers();

	//! is vbo recommended?
	virtual bool isHardwareBufferRecommend(const scene::IVertexBuffer *mb);

	//! is vbo recommended?
	virtual bool isHardwareBufferRecommend(const scene::IIndexBuffer *mb);

	//! Create occlusion query.
	/** Use node for identification and mesh for occlusion test. */
	virtual void addOcclusionQuery(scene::ISceneNode *node,
			const scene::IMesh *mesh = 0) override;

	//! Remove occlusion query.
	void removeOcclusionQuery(scene::ISceneNode *node) override;

	//! Remove all occlusion queries.
	void removeAllOcclusionQueries() override;

	//! Run occlusion query. Draws mesh stored in query.
	/** If the mesh shall not be rendered visible, use
	overrideMaterial to disable the color and depth buffer. */
	void runOcclusionQuery(scene::ISceneNode *node, bool visible = false) override;

	//! Run all occlusion queries. Draws all meshes stored in queries.
	/** If the meshes shall not be rendered visible, use
	overrideMaterial to disable the color and depth buffer. */
	void runAllOcclusionQueries(bool visible = false) override;

	//! Update occlusion query. Retrieves results from GPU.
	/** If the query shall not block, set the flag to false.
	Update might not occur in this case, though */
	void updateOcclusionQuery(scene::ISceneNode *node, bool block = true) override;

	//! Update all occlusion queries. Retrieves results from GPU.
	/** If the query shall not block, set the flag to false.
	Update might not occur in this case, though */
	void updateAllOcclusionQueries(bool block = true) override;

	//! Return query result.
	/** Return value is the number of visible pixels/fragments.
	The value is a safe approximation, i.e. can be larger than the
	actual value of pixels. */
	u32 getOcclusionQueryResult(scene::ISceneNode *node) const override;

	//! Create render target.
	IRenderTarget *addRenderTarget() override;

	void blitRenderTarget(IRenderTarget *from, IRenderTarget *to) override {}

	//! Remove render target.
	void removeRenderTarget(IRenderTarget *renderTarget) override;

	//! Remove all render targets.
	void removeAllRenderTargets() override;

	//! Only used by the engine internally.
	/** Used to notify the driver that the window was resized. */
	void OnResize(const core::dimension2d<u32> &size) override;

	//! Adds a new material renderer to the video device.
	virtual s32 addMaterialRenderer(IMaterialRenderer *renderer,
			const char *name = 0) override;

	//! Returns driver and operating system specific data about the IVideoDriver.
	const SExposedVideoData &getExposedVideoData() override;

	//! Returns type of video driver
	E_DRIVER_TYPE getDriverType() const override;

	//! Returns the transformation set by setTransform
	const core::matrix4 &getTransform(E_TRANSFORMATION_STATE state) const override;

	//! Returns pointer to the IGPUProgrammingServices interface.
	IGPUProgrammingServices *getGPUProgrammingServices() override;

	//! Returns pointer to material renderer or null
	IMaterialRenderer *getMaterialRenderer(u32 idx) const override;

	//! Returns amount of currently available material renderers.
	u32 getMaterialRendererCount() const override;

	//! Returns name of the material renderer
	const char *getMaterialRendererName(u32 idx) const override;

	//! Adds a new material renderer to the VideoDriver, based on a high level shading language.
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
			s32 userData = 0)override;

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
			s32 userData = 0) override;

	s32 addHighLevelShaderMaterialFromFiles(
			io::IReadFile *vertexShaderProgram,
			io::IReadFile *pixelShaderProgram = 0,
			io::IReadFile *geometryShaderProgram = 0,
			const c8 *shaderName = nullptr,
			scene::E_PRIMITIVE_TYPE inType = scene::EPT_TRIANGLES,
			scene::E_PRIMITIVE_TYPE outType = scene::EPT_TRIANGLE_STRIP,
			u32 verticesOut = 0,
			IShaderConstantSetCallBack *callback = nullptr,
			E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
			s32 userData = 0);

	virtual void deleteShaderMaterial(s32 material) override;

	//! Returns a pointer to the mesh manipulator.
	scene::IMeshManipulator *getMeshManipulator() override;

	void clearBuffers(u16 flag, SColor color = SColor(255, 0, 0, 0), f32 depth = 1.f, u8 stencil = 0) override;

	//! Returns an image created from the last rendered frame.
	IImage *createScreenShot(video::ECOLOR_FORMAT format = video::ECF_UNKNOWN, video::E_RENDER_TARGET target = video::ERT_FRAME_BUFFER) override;

	//! Writes the provided image to disk file
	bool writeImageToFile(IImage *image, const io::path &filename, u32 param = 0) override;

	//! Writes the provided image to a file.
	bool writeImageToFile(IImage *image, io::IWriteFile *file, u32 param = 0) override;

	//! Sets the name of a material renderer.
	void setMaterialRendererName(u32 idx, const char *name) override;

	//! Swap the material renderers used for certain id's
	void swapMaterialRenderers(u32 idx1, u32 idx2, bool swapNames) override;

	//! looks if the image is already loaded
	video::ITexture *findTexture(const io::path &filename) override;

	//! Returns the graphics card vendor name.
	core::stringc getVendorInfo() override { return "Not available on this driver."; }

	//! Set the minimum number of vertices for which a hw buffer will be created
	/** \param count Number of vertices to set as minimum. */
	void setMinHardwareBufferVertexCount(u32 count) override;

	//! Get the global Material, which might override local materials.
	/** Depending on the enable flags, values from this Material
	are used to override those of local materials of some
	meshbuffer being rendered. */
	SOverrideMaterial &getOverrideMaterial() override;

	//! Get the 2d override material for altering its values
	SMaterial &getMaterial2D() override;

	//! Enable the 2d override material
	void enableMaterial2D(bool enable = true) override;

	//! Only used by the engine internally.
	void setAllowZWriteOnTransparent(bool flag) override
	{
		AllowZWriteOnTransparent = flag;
	}

	//! Returns the maximum texture size supported.
	core::dimension2du getMaxTextureSize() const override;

	//! Used by some SceneNodes to check if a material should be rendered in the transparent render pass
	bool needsTransparentRenderPass(const video::SMaterial &material) const override;

protected:
	//! deletes all textures
	void deleteAllTextures();

	//! opens the file and loads it into the surface
	ITexture *loadTextureFromFile(io::IReadFile *file, const io::path &hashName = "");

	//! adds a surface, not loaded or created by the Irrlicht Engine
	void addTexture(ITexture *surface);

	virtual ITexture *createDeviceDependentTexture(const io::path &name, E_TEXTURE_TYPE type,
		const std::vector<IImage*> &images);

	//! checks triangle count and print warning if wrong
	bool checkPrimitiveCount(u32 prmcnt) const;

	bool checkImage(IImage *image) const;

	bool checkImage(const std::vector<IImage*> &image) const;

	// adds a material renderer and drops it afterwards. To be used for internal creation
	s32 addAndDropMaterialRenderer(IMaterialRenderer *m);

	//! deletes all material renderers
	void deleteMaterialRenders();

	// adds a created hardware buffer to the relevant data structure
	void registerHardwareBuffer(SHWBufferLink *HWBuffer);

	// prints renderer version
	void printVersion();

	inline void accountHWBufferUpload(u32 size)
	{
		FrameStats.HWBuffersUploaded++;
		(void)size;
	}

	inline bool getWriteZBuffer(const SMaterial &material) const
	{
		switch (material.ZWriteEnable) {
		case video::EZW_OFF:
			return false;
		case video::EZW_AUTO:
			return AllowZWriteOnTransparent || !needsTransparentRenderPass(material);
		case video::EZW_ON:
			return true;
		}
		return true; // never should get here, but some compilers don't know and complain
	}

	struct SSurface
	{
		video::ITexture *Surface;

		bool operator<(const SSurface &other) const
		{
			return Surface->getName() < other.Surface->getName();
		}
	};

	struct SMaterialRenderer
	{
		core::stringc Name;
		IMaterialRenderer *Renderer;
	};

	struct SDummyTexture : public ITexture
	{
		SDummyTexture(const io::path &name, E_TEXTURE_TYPE type) :
				ITexture(name, type){};

		void setSize(const core::dimension2d<u32> &size) { Size = OriginalSize = size; }

		void *lock(E_TEXTURE_LOCK_MODE mode = ETLM_READ_WRITE, u32 mipmapLevel = 0, u32 layer = 0, E_TEXTURE_LOCK_FLAGS lockFlags = ETLF_FLIP_Y_UP_RTT) override { return 0; }
		void unlock() override {}
		void regenerateMipMapLevels(u32 layer = 0) override {}
	};
	core::array<SSurface> Textures;

	struct SOccQuery
	{
		SOccQuery(scene::ISceneNode *node, const scene::IMesh *mesh = 0) :
				Node(node), Mesh(mesh), PID(0), Result(0xffffffff), Run(0xffffffff)
		{
			if (Node)
				Node->grab();
			if (Mesh)
				Mesh->grab();
		}

		SOccQuery(const SOccQuery &other) :
				Node(other.Node), Mesh(other.Mesh), PID(other.PID), Result(other.Result), Run(other.Run)
		{
			if (Node)
				Node->grab();
			if (Mesh)
				Mesh->grab();
		}

		~SOccQuery()
		{
			if (Node)
				Node->drop();
			if (Mesh)
				Mesh->drop();
		}

		SOccQuery &operator=(const SOccQuery &other)
		{
			Node = other.Node;
			Mesh = other.Mesh;
			PID = other.PID;
			Result = other.Result;
			Run = other.Run;
			if (Node)
				Node->grab();
			if (Mesh)
				Mesh->grab();
			return *this;
		}

		bool operator==(const SOccQuery &other) const
		{
			return other.Node == Node;
		}

		scene::ISceneNode *Node;
		const scene::IMesh *Mesh;
		union
		{
			void *PID;
			unsigned int UID;
		};
		u32 Result;
		u32 Run;
	};
	core::array<SOccQuery> OcclusionQueries;

	core::array<IRenderTarget *> RenderTargets;

	// Shared objects used with simplified IVideoDriver::setRenderTarget method with ITexture* param.
	IRenderTarget *SharedRenderTarget;
	core::array<ITexture *> SharedDepthTextures;

	IRenderTarget *CurrentRenderTarget;
	core::dimension2d<u32> CurrentRenderTargetSize;

	core::array<video::IImageLoader *> SurfaceLoader;
	core::array<video::IImageWriter *> SurfaceWriter;
	core::array<SMaterialRenderer> MaterialRenderers;

	std::vector<SHWBufferLink *> HWBufferList;

	io::IFileSystem *FileSystem;

	//! mesh manipulator
	scene::IMeshManipulator *MeshManipulator;

	core::rect<s32> ViewPort;
	core::dimension2d<u32> ScreenSize;
	core::matrix4 TransformationMatrix;

	CFPSCounter FPSCounter;
	SFrameStats FrameStats;

	u32 MinVertexCountForVBO;

	u32 TextureCreationFlags;

	f32 FogStart;
	f32 FogEnd;
	f32 FogDensity;
	SColor FogColor;
	SExposedVideoData ExposedData;

	SOverrideMaterial OverrideMaterial;
	SMaterial OverrideMaterial2D;
	SMaterial InitMaterial2D;
	bool OverrideMaterial2DEnabled;

	E_FOG_TYPE FogType;
	bool PixelFog;
	bool RangeFog;
	bool AllowZWriteOnTransparent;

	bool FeatureEnabled[video::EVDF_COUNT];
};

} // end namespace video
