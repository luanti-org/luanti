// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#include "SIrrCreationParameters.h"


#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "IMaterialRendererServices.h"
#include "CNullDriver.h"

#include "COpenGLExtensionHandler.h"
#include "IContextManager.h"

namespace video
{
class IContextManager;

class COpenGLDriver : public CNullDriver, public IMaterialRendererServices, public COpenGLExtensionHandler
{
public:
	// Information about state of fixed pipeline activity.
	enum E_OPENGL_FIXED_PIPELINE_STATE
	{
		EOFPS_ENABLE = 0,        // fixed pipeline.
		EOFPS_DISABLE,           // programmable pipeline.
		EOFPS_ENABLE_TO_DISABLE, // switch from fixed to programmable pipeline.
		EOFPS_DISABLE_TO_ENABLE  // switch from programmable to fixed pipeline.
	};

	COpenGLDriver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager);

	bool initDriver();

	//! destructor
	virtual ~COpenGLDriver();

	virtual bool beginScene(u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0), f32 clearDepth = 1.f, u8 clearStencil = 0,
			const SExposedVideoData &videoData = SExposedVideoData(), core::rect<s32> *sourceRect = 0) override;

	bool endScene() override;

	//! sets transformation
	void setTransform(E_TRANSFORMATION_STATE state, const core::matrix4 &mat) override;

	struct SHWBufferLink_opengl : public SHWBufferLink
	{
		SHWBufferLink_opengl(const scene::IVertexBuffer *vb) : SHWBufferLink(vb) {}
		SHWBufferLink_opengl(const scene::IIndexBuffer *ib) : SHWBufferLink(ib) {}

		GLuint vbo_ID = 0;
		u32 vbo_Size = 0;
	};

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

	//! Create occlusion query.
	/** Use node for identification and mesh for occlusion test. */
	virtual void addOcclusionQuery(scene::ISceneNode *node,
			const scene::IMesh *mesh = 0) override;

	//! Remove occlusion query.
	void removeOcclusionQuery(scene::ISceneNode *node) override;

	//! Run occlusion query. Draws mesh stored in query.
	/** If the mesh shall not be rendered visible, use
	overrideMaterial to disable the color and depth buffer. */
	void runOcclusionQuery(scene::ISceneNode *node, bool visible = false) override;

	//! Update occlusion query. Retrieves results from GPU.
	/** If the query shall not block, set the flag to false.
	Update might not occur in this case, though */
	void updateOcclusionQuery(scene::ISceneNode *node, bool block = true) override;

	//! Return query result.
	/** Return value is the number of visible pixels/fragments.
	The value is a safe approximation, i.e. can be larger then the
	actual value of pixels. */
	u32 getOcclusionQueryResult(scene::ISceneNode *node) const override;

	//! Create render target.
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
		return FeatureEnabled[feature] && COpenGLExtensionHandler::queryFeature(feature);
	}

	//! Disable a feature of the driver.
	void disableFeature(E_VIDEO_DRIVER_FEATURE feature, bool flag = true) override;

	//! Sets a material. All 3d drawing functions draw geometry now
	//! using this material.
	//! \param material: Material to be used from now on.
	void setMaterial(const SMaterial &material) override;

	virtual void draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			SColor color = SColor(255, 255, 255, 255), bool useAlphaChannelOfTexture = false) override;

	virtual void draw2DImage(const video::ITexture *texture, const core::rect<s32> &destRect,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			const video::SColor *const colors = 0, bool useAlphaChannelOfTexture = false) override;

	// Explicitly bring in base class methods, otherwise
	// this overload would hide them.
	using CNullDriver::draw2DImage;
	virtual void draw2DImage(const video::ITexture *texture, u32 layer, bool flip);

	//! draws a set of 2d images, using a color and the alpha channel of the
	//! texture if desired.
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

	//! \return Returns the name of the video driver. Example: In case of the Direct3D8
	//! driver, it would return "Direct3D8.1".
	const char *getName() const override;

	//! sets a viewport
	void setViewPort(const core::rect<s32> &area) override;

	//! Sets the fog mode.
	virtual void setFog(SColor color, E_FOG_TYPE fogType, f32 start,
			f32 end, f32 density, bool pixelFog, bool rangeFog) override;

	//! Only used by the internal engine. Used to notify the driver that
	//! the window was resized.
	void OnResize(const core::dimension2d<u32> &size) override;

	//! Returns type of video driver
	E_DRIVER_TYPE getDriverType() const override;

	//! get color format of the current color buffer
	ECOLOR_FORMAT getColorFormat() const override;

	//! Returns the transformation set by setTransform
	const core::matrix4 &getTransform(E_TRANSFORMATION_STATE state) const override;

	//! Can be called by an IMaterialRenderer to make its work easier.
	virtual void setBasicRenderStates(const SMaterial &material, const SMaterial &lastmaterial,
			bool resetAllRenderstates) override;

	//! Compare in SMaterial doesn't check texture parameters, so we should call this on each OnRender call.
	virtual void setTextureRenderStates(const SMaterial &material, bool resetAllRenderstates);

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

	//! disables all textures beginning with the optional fromStage parameter. Otherwise all texture stages are disabled.
	//! Returns whether disabling was successful or not.
	bool disableTextures(u32 fromStage = 0);

	//! Adds a new material renderer to the VideoDriver, using GLSL to render geometry.
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
			s32 userData = 0) override;

	//! Returns a pointer to the IVideoDriver interface. (Implementation for
	//! IMaterialRendererServices)
	IVideoDriver *getVideoDriver() override;

	//! Returns the maximum amount of primitives (mostly vertices) which
	//! the device is able to render with one drawIndexedTriangleList
	//! call.
	u32 getMaximalPrimitiveCount() const override;

	virtual ITexture *addRenderTargetTexture(const core::dimension2d<u32> &size,
			const io::path &name, const ECOLOR_FORMAT format = ECF_UNKNOWN) override;

	virtual ITexture *addRenderTargetTextureMs(const core::dimension2d<u32> &size, u8 msaa,
			const io::path &name = "rt", const ECOLOR_FORMAT format = ECF_UNKNOWN) override;

	//! Creates a render target texture for a cubemap
	ITexture *addRenderTargetTextureCubemap(const u32 sideLen,
			const io::path &name, const ECOLOR_FORMAT format) override;

	virtual bool setRenderTargetEx(IRenderTarget *target, u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0),
			f32 clearDepth = 1.f, u8 clearStencil = 0) override;

	void clearBuffers(u16 flag, SColor color = SColor(255, 0, 0, 0), f32 depth = 1.f, u8 stencil = 0) override;

	//! Returns an image created from the last rendered frame.
	IImage *createScreenShot(video::ECOLOR_FORMAT format = video::ECF_UNKNOWN, video::E_RENDER_TARGET target = video::ERT_FRAME_BUFFER) override;

	//! checks if an OpenGL error has happened and prints it (+ some internal code which is usually the line number)
	//! for performance reasons only available in debug mode
	bool testGLError(int code = 0);

	//! Enable the 2d override material
	void enableMaterial2D(bool enable = true) override;

	//! Returns the graphics card vendor name.
	core::stringc getVendorInfo() override { return VendorName; }

	//! Returns the maximum texture size supported.
	core::dimension2du getMaxTextureSize() const override;

	//! Removes a texture from the texture cache and deletes it, freeing lot of memory.
	void removeTexture(ITexture *texture) override;

	//! Check if the driver supports creating textures with the given color format
	bool queryTextureFormat(ECOLOR_FORMAT format) const override;

	//! Used by some SceneNodes to check if a material should be rendered in the transparent render pass
	bool needsTransparentRenderPass(const video::SMaterial &material) const override;

	//! Convert E_PRIMITIVE_TYPE to OpenGL equivalent
	GLenum primitiveTypeToGL(scene::E_PRIMITIVE_TYPE type) const;

	//! Convert E_BLEND_FACTOR to OpenGL equivalent
	GLenum getGLBlend(E_BLEND_FACTOR factor) const;

	//! Get ZBuffer bits.
	GLenum getZBufferBits() const;

	bool getColorFormatParameters(ECOLOR_FORMAT format, GLint &internalFormat, GLenum &pixelFormat,
			GLenum &pixelType, void (**converter)(const void *, s32, void *)) const;

	//! Return info about fixed pipeline state.
	E_OPENGL_FIXED_PIPELINE_STATE getFixedPipelineState() const;

	//! Set info about fixed pipeline state.
	void setFixedPipelineState(E_OPENGL_FIXED_PIPELINE_STATE state);

	//! Get current material.
	const SMaterial &getCurrentMaterial() const;

	COpenGLCacheHandler *getCacheHandler() const;

private:
	bool updateVertexHardwareBuffer(SHWBufferLink_opengl *HWBuffer);
	bool updateIndexHardwareBuffer(SHWBufferLink_opengl *HWBuffer);

	//! inits the parts of the open gl driver used on all platforms
	bool genericDriverInit();

	ITexture *createDeviceDependentTexture(const io::path &name, E_TEXTURE_TYPE type, const std::vector<IImage*> &images) override;

	//! creates a transposed matrix in supplied GLfloat array to pass to OpenGL
	inline void getGLMatrix(GLfloat gl_matrix[16], const core::matrix4 &m);
	inline void getGLTextureMatrix(GLfloat gl_matrix[16], const core::matrix4 &m);

	//! get native wrap mode value
	GLint getTextureWrapMode(const u8 clamp);

	//! sets the needed renderstates
	void setRenderStates3DMode();

	//! sets the needed renderstates
	void setRenderStates2DMode(bool alpha, bool texture, bool alphaChannel);

	void createMaterialRenderers();

	//! Assign a hardware light to the specified requested light, if any
	//! free hardware lights exist.
	//! \param[in] lightIndex: the index of the requesting light
	void assignHardwareLight(u32 lightIndex);

	//! helper function for render setup.
	void getColorBuffer(const void *vertices, u32 vertexCount, E_VERTEX_TYPE vType);

	//! helper function doing the actual rendering.
	void renderArray(const void *indexList, u32 primitiveCount,
			scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType);

	//! Same as `CacheHandler->setViewport`, but also sets `ViewPort`
	virtual void setViewPortRaw(u32 width, u32 height);

	COpenGLCacheHandler *CacheHandler;

	core::stringc Name;
	core::matrix4 Matrices[ETS_COUNT];
	core::array<u8> ColorBuffer;

	//! enumeration for rendering modes such as 2d and 3d for minimizing the switching of renderStates.
	enum E_RENDER_MODE
	{
		ERM_NONE = 0, // no render state has been set yet.
		ERM_2D,       // 2d drawing rendermode
		ERM_3D        // 3d rendering mode
	};

	E_RENDER_MODE CurrentRenderMode;
	//! bool to make all renderstates reset if set to true.
	bool ResetRenderStates;
	bool Transformation3DChanged;
	u8 AntiAlias;

	SMaterial Material, LastMaterial;

	core::stringc VendorName;

	core::matrix4 TextureFlipMatrix;

	//! Color buffer format
	ECOLOR_FORMAT ColorFormat;

	E_OPENGL_FIXED_PIPELINE_STATE FixedPipelineState;

	SIrrlichtCreationParameters Params;

	//! Built-in 2D quad for 2D rendering.
	S3DVertex Quad2DVertices[4];
	static const u16 Quad2DIndices[4];

	IContextManager *ContextManager;
};

} // end namespace video

#endif // _IRR_COMPILE_WITH_OPENGL_
