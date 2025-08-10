// Copyright (C) 2023 Vitaliy Lobachevskiy
// Copyright (C) 2014 Patryk Nadrowski
// Copyright (C) 2009-2010 Amundis
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#include "SIrrCreationParameters.h"
#include "Common.h"
#include "VBO.h"
#include "CNullDriver.h"
#include "IMaterialRendererServices.h"
#include "EDriverFeatures.h"
#include "fast_atof.h"
#include "ExtensionHandler.h"
#include "IContextManager.h"

namespace video
{
struct VertexType;

class COpenGL3Renderer2D;

class COpenGL3DriverBase : public CNullDriver, public IMaterialRendererServices, public COpenGL3ExtensionHandler
{
	friend class COpenGLCoreTexture<COpenGL3DriverBase>;

protected:
	//! constructor (use createOpenGL3Driver instead)
	COpenGL3DriverBase(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager);

public:
	//! destructor
	virtual ~COpenGL3DriverBase();

	virtual bool beginScene(u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0), f32 clearDepth = 1.f, u8 clearStencil = 0,
			const SExposedVideoData &videoData = SExposedVideoData(), core::rect<s32> *sourceRect = 0) override;

	bool endScene() override;

	//! sets transformation
	void setTransform(E_TRANSFORMATION_STATE state, const core::matrix4 &mat) override;

	struct SHWBufferLink_opengl : public SHWBufferLink
	{
		SHWBufferLink_opengl(const scene::IVertexBuffer *vb) : SHWBufferLink(vb) {}
		SHWBufferLink_opengl(const scene::IIndexBuffer *ib) : SHWBufferLink(ib) {}

		OpenGLVBO Vbo;
	};

	bool updateVertexHardwareBuffer(SHWBufferLink_opengl *HWBuffer);
	bool updateIndexHardwareBuffer(SHWBufferLink_opengl *HWBuffer);

	//! updates hardware buffer if needed
	bool updateHardwareBuffer(SHWBufferLink *HWBuffer) override;

	//! Create hardware buffer from vertex buffer
	SHWBufferLink *createHardwareBuffer(const scene::IVertexBuffer *vb) override;

	//! Create hardware buffer from index buffer
	SHWBufferLink *createHardwareBuffer(const scene::IIndexBuffer *ib) override;

	//! Delete hardware buffer (only some drivers can)
	void deleteHardwareBuffer(SHWBufferLink *HWBuffer) override;

	void drawBuffers(const scene::IVertexBuffer *vb,
		const scene::IIndexBuffer *ib, u32 primCount,
		scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES) override;

	IRenderTarget *addRenderTarget() override;

	void blitRenderTarget(IRenderTarget *from, IRenderTarget *to) override;

	//! draws a vertex primitive list
	virtual void drawVertexPrimitiveList(const void *vertices, u32 vertexCount,
			const void *indexList, u32 primitiveCount,
			E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType) override;

	//! draws a vertex primitive list in 2d
	virtual void draw2DVertexPrimitiveList(const void *vertices, u32 vertexCount,
			const void *indexList, u32 primitiveCount,
			E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType) override;

	//! queries the features of the driver, returns true if feature is available
	bool queryFeature(E_VIDEO_DRIVER_FEATURE feature) const override
	{
		return FeatureEnabled[feature] && COpenGL3ExtensionHandler::queryFeature(feature);
	}

	//! Sets a material.
	void setMaterial(const SMaterial &material) override;

	virtual void draw2DImage(const video::ITexture *texture,
			const core::position2d<s32> &destPos,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			SColor color = SColor(255, 255, 255, 255), bool useAlphaChannelOfTexture = false) override;

	virtual void draw2DImage(const video::ITexture *texture, const core::rect<s32> &destRect,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			const video::SColor *const colors = 0, bool useAlphaChannelOfTexture = false) override;

	// internally used
	virtual void draw2DImage(const video::ITexture *texture, u32 layer, bool flip);

	using CNullDriver::draw2DImage;

	void draw2DImageBatch(const video::ITexture *texture,
			const core::array<core::position2d<s32>> &positions,
			const core::array<core::rect<s32>> &sourceRects,
			const core::rect<s32> *clipRect,
			SColor color,
			bool useAlphaChannelOfTexture) override;

	//! draw an 2d rectangle
	virtual void draw2DRectangle(SColor color, const core::rect<s32> &pos,
			const core::rect<s32> *clip = 0) override;

	//! Draws an 2d rectangle with a gradient.
	virtual void draw2DRectangle(const core::rect<s32> &pos,
			SColor colorLeftUp, SColor colorRightUp, SColor colorLeftDown, SColor colorRightDown,
			const core::rect<s32> *clip = 0) override;

	//! Draws a 2d line.
	virtual void draw2DLine(const core::position2d<s32> &start,
			const core::position2d<s32> &end,
			SColor color = SColor(255, 255, 255, 255)) override;

	//! Draws a 3d line.
	virtual void draw3DLine(const core::vector3df &start,
			const core::vector3df &end,
			SColor color = SColor(255, 255, 255, 255)) override;

	//! Returns the name of the video driver.
	const char *getName() const override;

	//! Returns the maximum texture size supported.
	core::dimension2du getMaxTextureSize() const override;

	//! sets a viewport
	void setViewPort(const core::rect<s32> &area) override;

	//! Only used internally by the engine
	void OnResize(const core::dimension2d<u32> &size) override;

	//! Returns type of video driver
	E_DRIVER_TYPE getDriverType() const override;

	//! get color format of the current color buffer
	ECOLOR_FORMAT getColorFormat() const override;

	//! Returns the transformation set by setTransform
	const core::matrix4 &getTransform(E_TRANSFORMATION_STATE state) const override;

	//! Can be called by an IMaterialRenderer to make its work easier.
	void setBasicRenderStates(const SMaterial &material, const SMaterial &lastmaterial, bool resetAllRenderstates) override;

	//! Compare in SMaterial doesn't check texture parameters, so we should call this on each OnRender call.
	void setTextureRenderStates(const SMaterial &material, bool resetAllRenderstates);

	//! Get a vertex shader constant index.
	s32 getVertexShaderConstantID(const c8 *name) override;

	//! Get a pixel shader constant index.
	s32 getPixelShaderConstantID(const c8 *name) override;

	//! Sets a constant for the vertex shader based on an index.
	bool setVertexShaderConstant(s32 index, const f32 *floats, int count) override;

	//! Int interface for the above.
	bool setVertexShaderConstant(s32 index, const s32 *ints, int count) override;

	//! Uint interface for the above.
	bool setVertexShaderConstant(s32 index, const u32 *ints, int count) override;

	//! Sets a constant for the pixel shader based on an index.
	bool setPixelShaderConstant(s32 index, const f32 *floats, int count) override;

	//! Int interface for the above.
	bool setPixelShaderConstant(s32 index, const s32 *ints, int count) override;

	//! Uint interface for the above.
	bool setPixelShaderConstant(s32 index, const u32 *ints, int count) override;

	//! Adds a new material renderer to the VideoDriver
	virtual s32 addHighLevelShaderMaterial(
			const c8 *vertexShaderProgram,
			const c8 *pixelShaderProgram,
			const c8 *geometryShaderProgram = nullptr,
			const c8 *shaderName = nullptr,
			scene::E_PRIMITIVE_TYPE inType = scene::EPT_TRIANGLES,
			scene::E_PRIMITIVE_TYPE outType = scene::EPT_TRIANGLE_STRIP,
			u32 verticesOut = 0,
			IShaderConstantSetCallBack *callback = nullptr,
			E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
			s32 userData = 0) override;

	//! Returns pointer to the IGPUProgrammingServices interface.
	IGPUProgrammingServices *getGPUProgrammingServices() override;

	//! Returns a pointer to the IVideoDriver interface.
	IVideoDriver *getVideoDriver() override;

	//! Returns the maximum amount of primitives
	u32 getMaximalPrimitiveCount() const override;

	virtual ITexture *addRenderTargetTexture(const core::dimension2d<u32> &size,
			const io::path &name, const ECOLOR_FORMAT format = ECF_UNKNOWN) override;

	virtual ITexture *addRenderTargetTextureMs(const core::dimension2d<u32> &size, u8 msaa,
			const io::path &name, const ECOLOR_FORMAT format = ECF_UNKNOWN) override;

	//! Creates a render target texture for a cubemap
	ITexture *addRenderTargetTextureCubemap(const u32 sideLen,
			const io::path &name, const ECOLOR_FORMAT format) override;

	virtual bool setRenderTargetEx(IRenderTarget *target, u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0),
			f32 clearDepth = 1.f, u8 clearStencil = 0) override;

	void clearBuffers(u16 flag, SColor color = SColor(255, 0, 0, 0), f32 depth = 1.f, u8 stencil = 0) override;

	//! Returns an image created from the last rendered frame.
	IImage *createScreenShot(video::ECOLOR_FORMAT format = video::ECF_UNKNOWN, video::E_RENDER_TARGET target = video::ERT_FRAME_BUFFER) override;

	//! checks if an OpenGL error has happened and prints it, use via TEST_GL_ERROR().
	// Does *nothing* unless in debug mode.
	bool testGLError(const char *file, int line);

	//! Returns the graphics card vendor name.
	core::stringc getVendorInfo() override
	{
		return VendorName;
	};

	void removeTexture(ITexture *texture) override;

	//! Check if the driver supports creating textures with the given color format
	bool queryTextureFormat(ECOLOR_FORMAT format) const override;

	//! Used by some SceneNodes to check if a material should be rendered in the transparent render pass
	bool needsTransparentRenderPass(const video::SMaterial &material) const override;

	//! Convert E_BLEND_FACTOR to OpenGL equivalent
	GLenum getGLBlend(E_BLEND_FACTOR factor) const;

	virtual bool getColorFormatParameters(ECOLOR_FORMAT format, GLint &internalFormat, GLenum &pixelFormat,
			GLenum &pixelType, void (**converter)(const void *, s32, void *)) const;

	//! Get current material.
	const SMaterial &getCurrentMaterial() const;

	COpenGL3CacheHandler *getCacheHandler() const;

protected:
	virtual bool genericDriverInit(const core::dimension2d<u32> &screenSize, bool stencilBuffer);

	void initVersion();
	virtual OpenGLVersion getVersionFromOpenGL() const = 0;

	virtual void initFeatures() = 0;

	bool isVersionAtLeast(int major, int minor = 0) const noexcept;

	void chooseMaterial2D();

	ITexture *createDeviceDependentTexture(const io::path &name, E_TEXTURE_TYPE type,
		const std::vector<IImage*> &images) override;

	//! Map Irrlicht wrap mode to OpenGL enum
	GLint getTextureWrapMode(u8 clamp) const;

	//! sets the needed renderstates
	void setRenderStates3DMode();

	//! sets the needed renderstates
	void setRenderStates2DMode(bool alpha, bool texture, bool alphaChannel);

	//! Prevent setRenderStateMode calls to do anything.
	// hack to allow drawing meshbuffers in 2D mode.
	// Better solution would be passing this flag through meshbuffers,
	// but the way this is currently implemented in Irrlicht makes this tricky to implement
	void lockRenderStateMode()
	{
		LockRenderStateMode = true;
	}

	//! Allow setRenderStateMode calls to work again
	void unlockRenderStateMode()
	{
		LockRenderStateMode = false;
	}

	bool uploadHardwareBuffer(OpenGLVBO &vbo, const void *buffer, size_t bufferSize, scene::E_HARDWARE_MAPPING hint);

	void createMaterialRenderers();

	void loadShaderData(const io::path &vertexShaderName, const io::path &fragmentShaderName, c8 **vertexShaderData, c8 **fragmentShaderData);

	bool setMaterialTexture(u32 layerIdx, const video::ITexture *texture);

	//! Same as `CacheHandler->setViewport`, but also sets `ViewPort`
	virtual void setViewPortRaw(u32 width, u32 height);

	void drawQuad(const VertexType &vertexType, const S3DVertex (&vertices)[4]);
	void drawArrays(GLenum primitiveType, const VertexType &vertexType, const void *vertices, int vertexCount);
	void drawElements(GLenum primitiveType, const VertexType &vertexType, const void *vertices, int vertexCount, const u16 *indices, int indexCount);
	void drawElements(GLenum primitiveType, const VertexType &vertexType, uintptr_t vertices, uintptr_t indices, int indexCount);

	void drawGeneric(const void *vertices, const void *indexList, u32 primitiveCount,
		E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType);

	void beginDraw(const VertexType &vertexType, uintptr_t verticesBase);
	void endDraw(const VertexType &vertexType);

	COpenGL3CacheHandler *CacheHandler;
	core::stringc Name;
	core::stringc VendorName;
	SIrrlichtCreationParameters Params;
	OpenGLVersion Version;

	//! bool to make all renderstates reset if set to true.
	bool ResetRenderStates;
	bool LockRenderStateMode;
	u8 AntiAlias;

	using FColorConverter = void (*)(const void *source, s32 count, void *dest);
	struct STextureFormatInfo
	{
		GLenum InternalFormat = 0;
		GLenum PixelFormat = 0;
		GLenum PixelType = 0;
		FColorConverter Converter = nullptr;
	};
	STextureFormatInfo TextureFormats[ECF_UNKNOWN] = {};

private:
	COpenGL3Renderer2D *MaterialRenderer2DActive;
	COpenGL3Renderer2D *MaterialRenderer2DTexture;
	COpenGL3Renderer2D *MaterialRenderer2DNoTexture;

	core::matrix4 Matrices[ETS_COUNT];

	//! enumeration for rendering modes such as 2d and 3d for minimizing the switching of renderStates.
	enum E_RENDER_MODE
	{
		ERM_NONE = 0, // no render state has been set yet.
		ERM_2D,       // 2d drawing rendermode
		ERM_3D        // 3d rendering mode
	};

	E_RENDER_MODE CurrentRenderMode;
	bool Transformation3DChanged;
	io::path OGLES2ShaderPath;

	SMaterial Material, LastMaterial;

	//! Color buffer format
	ECOLOR_FORMAT ColorFormat;

	IContextManager *ContextManager;

	void printTextureFormats();

	bool EnableErrorTest;

	OpenGLVBO QuadIndexVBO;
	void initQuadsIndices(u32 max_vertex_count = 65536);

	void debugCb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message);
	static void APIENTRY debugCb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);
};

} // end namespace video
