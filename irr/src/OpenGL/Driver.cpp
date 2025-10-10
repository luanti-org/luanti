// Copyright (C) 2023 Vitaliy Lobachevskiy
// Copyright (C) 2014 Patryk Nadrowski
// Copyright (C) 2009-2010 Amundis
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "Driver.h"
#include <cassert>
#include "CNullDriver.h"
#include "IContextManager.h"

#include "COpenGLCoreTexture.h"
#include "COpenGLCoreRenderTarget.h"
#include "COpenGLCoreCacheHandler.h"

#include "MaterialRenderer.h"
#include "FixedPipelineRenderer.h"
#include "Renderer2D.h"

#include "EVertexAttributes.h"
#include "CImage.h"
#include "os.h"

#include "mt_opengl.h"

namespace video
{

struct VertexAttribute
{
	enum Mode : u8
	{
		Regular,
		Normalized,
		Integer,
	};
	u8 Index;
	u8 ComponentCount;
	GLenum ComponentType;
	Mode mode;
	u32 Offset;
};

struct VertexType
{
	u32 VertexSize;
	std::vector<VertexAttribute> Attributes;

	// allow ranged for loops
	inline auto begin() const { return Attributes.begin(); }
	inline auto end() const { return Attributes.end(); }
};

static const VertexType vtStandard = {
		sizeof(S3DVertex),
		{
				{EVA_POSITION, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex, Pos)},
				{EVA_NORMAL, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex, Normal)},
				{EVA_COLOR, 4, GL_UNSIGNED_BYTE, VertexAttribute::Mode::Normalized, offsetof(S3DVertex, Color)},
				{EVA_TCOORD0, 2, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex, TCoords)},
		},
};

// FIXME: this is actually UB because these vertex classes are not "standard-layout"
// they violate the following requirement:
// - only one class in the hierarchy has non-static data members
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"

static const VertexType vt2TCoords = {
		sizeof(S3DVertex2TCoords),
		{
				{EVA_POSITION, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex2TCoords, Pos)},
				{EVA_NORMAL, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex2TCoords, Normal)},
				{EVA_COLOR, 4, GL_UNSIGNED_BYTE, VertexAttribute::Mode::Normalized, offsetof(S3DVertex2TCoords, Color)},
				{EVA_TCOORD0, 2, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex2TCoords, TCoords)},
				{EVA_TCOORD1, 2, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex2TCoords, TCoords2)},
		},
};

static const VertexType vtTangents = {
		sizeof(S3DVertexTangents),
		{
				{EVA_POSITION, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertexTangents, Pos)},
				{EVA_NORMAL, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertexTangents, Normal)},
				{EVA_COLOR, 4, GL_UNSIGNED_BYTE, VertexAttribute::Mode::Normalized, offsetof(S3DVertexTangents, Color)},
				{EVA_TCOORD0, 2, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertexTangents, TCoords)},
				{EVA_TANGENT, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertexTangents, Tangent)},
				{EVA_BINORMAL, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertexTangents, Binormal)},
		},
};

#pragma GCC diagnostic pop

static const VertexType &getVertexTypeDescription(E_VERTEX_TYPE type)
{
	switch (type) {
	case EVT_STANDARD:
		return vtStandard;
	case EVT_2TCOORDS:
		return vt2TCoords;
	case EVT_TANGENTS:
		return vtTangents;
	default:
		IRR_CODE_UNREACHABLE();
	}
}

static const VertexType vt2DImage = {
		sizeof(S3DVertex),
		{
				{EVA_POSITION, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex, Pos)},
				{EVA_COLOR, 4, GL_UNSIGNED_BYTE, VertexAttribute::Mode::Normalized, offsetof(S3DVertex, Color)},
				{EVA_TCOORD0, 2, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex, TCoords)},
		},
};

static const VertexType vtPrimitive = {
		sizeof(S3DVertex),
		{
				{EVA_POSITION, 3, GL_FLOAT, VertexAttribute::Mode::Regular, offsetof(S3DVertex, Pos)},
				{EVA_COLOR, 4, GL_UNSIGNED_BYTE, VertexAttribute::Mode::Normalized, offsetof(S3DVertex, Color)},
		},
};

void APIENTRY COpenGL3DriverBase::debugCb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	((COpenGL3DriverBase *)userParam)->debugCb(source, type, id, severity, length, message);
}

void COpenGL3DriverBase::debugCb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message)
{
	// shader compiler can be very noisy
	if (source == GL_DEBUG_SOURCE_SHADER_COMPILER && severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		return;

	ELOG_LEVEL ll = ELL_INFORMATION;
	if (severity == GL_DEBUG_SEVERITY_HIGH)
		ll = ELL_ERROR;
	else if (severity == GL_DEBUG_SEVERITY_MEDIUM)
		ll = ELL_WARNING;
	char buf[512];
	snprintf_irr(buf, sizeof(buf), "%04x %04x %.*s", source, type, length, message);
	os::Printer::log("GL", buf, ll);
}

COpenGL3DriverBase::COpenGL3DriverBase(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager) :
		CNullDriver(io, params.WindowSize), COpenGL3ExtensionHandler(), CacheHandler(0),
		Params(params), ResetRenderStates(true), LockRenderStateMode(false), AntiAlias(params.AntiAlias),
		MaterialRenderer2DActive(0), MaterialRenderer2DTexture(0), MaterialRenderer2DNoTexture(0),
		CurrentRenderMode(ERM_NONE), Transformation3DChanged(true),
		OGLES2ShaderPath(params.OGLES2ShaderPath),
		ColorFormat(ECF_R8G8B8), ContextManager(contextManager), EnableErrorTest(params.DriverDebug)
{
	if (!ContextManager)
		return;

	ContextManager->grab();
	ContextManager->generateSurface();
	ContextManager->generateContext();
	ExposedData = ContextManager->getContext();
	ContextManager->activateContext(ExposedData, false);
	GL.LoadAllProcedures(ContextManager);

	TEST_GL_ERROR(this);
}

COpenGL3DriverBase::~COpenGL3DriverBase()
{
	QuadIndexVBO.destroy();

	deleteMaterialRenders();

	CacheHandler->getTextureCache().clear();

	removeAllRenderTargets();
	deleteAllTextures();
	removeAllOcclusionQueries();
	removeAllHardwareBuffers();

	delete MaterialRenderer2DTexture;
	delete MaterialRenderer2DNoTexture;
	delete CacheHandler;

	if (ContextManager) {
		ContextManager->destroyContext();
		ContextManager->destroySurface();
		ContextManager->terminate();
		ContextManager->drop();
	}
}

void COpenGL3DriverBase::initQuadsIndices(u32 max_vertex_count)
{
	u32 max_quad_count = max_vertex_count / 4;
	u32 indices_size = 6 * max_quad_count;
	if (indices_size == QuadIndexVBO.getSize() * sizeof(u16))
		return;
	// initialize buffer contents
	std::vector<u16> QuadsIndices;
	QuadsIndices.reserve(indices_size);
	for (u32 k = 0; k < max_quad_count; k++) {
		QuadsIndices.push_back(4 * k + 0);
		QuadsIndices.push_back(4 * k + 1);
		QuadsIndices.push_back(4 * k + 2);
		QuadsIndices.push_back(4 * k + 0);
		QuadsIndices.push_back(4 * k + 2);
		QuadsIndices.push_back(4 * k + 3);
	}
	QuadIndexVBO.upload(QuadsIndices.data(), QuadsIndices.size() * sizeof(u16),
		0, GL_STATIC_DRAW, true);
	assert(QuadIndexVBO.exists());
}

void COpenGL3DriverBase::initVersion()
{
	Name = GL.GetString(GL_VERSION);
	printVersion();

	// print renderer information
	VendorName = GL.GetString(GL_RENDERER);
	os::Printer::log("Renderer", VendorName.c_str(), ELL_INFORMATION);

	Version = getVersionFromOpenGL();
}

bool COpenGL3DriverBase::isVersionAtLeast(int major, int minor) const noexcept
{
	if (Version.Major < major)
		return false;
	if (Version.Major > major)
		return true;
	return Version.Minor >= minor;
}

bool COpenGL3DriverBase::genericDriverInit(const core::dimension2d<u32> &screenSize, bool stencilBuffer)
{
	initVersion();
	initFeatures();
	printTextureFormats();

	if (EnableErrorTest) {
		if (KHRDebugSupported) {
			GL.Enable(GL_DEBUG_OUTPUT);
			GL.DebugMessageCallback(debugCb, this);
		} else {
			os::Printer::log("GL debug extension not available");
		}
	} else {
		// don't do debug things if they are not wanted (even if supported)
		KHRDebugSupported = false;
	}

	initQuadsIndices();

	// reset cache handler
	delete CacheHandler;
	CacheHandler = new COpenGL3CacheHandler(this);

	StencilBuffer = stencilBuffer;

	GL.PixelStorei(GL_PACK_ALIGNMENT, 1);

	for (s32 i = 0; i < ETS_COUNT; ++i)
		setTransform(static_cast<E_TRANSFORMATION_STATE>(i), core::IdentityMatrix);

	GL.ClearDepthf(1.0f);

	GL.FrontFace(GL_CW);

	// create material renderers
	createMaterialRenderers();

	// set the renderstates
	setRenderStates3DMode();

	// set fog mode
	setFog(FogColor, FogType, FogStart, FogEnd, FogDensity, PixelFog, RangeFog);

	// We need to reset once more at the beginning of the first rendering.
	// This fixes problems with intermediate changes to the material during texture load.
	ResetRenderStates = true;

	TEST_GL_ERROR(this);

	return true;
}

void COpenGL3DriverBase::printTextureFormats()
{
	char buf[128];
	for (u32 i = 0; i < static_cast<u32>(ECF_UNKNOWN); i++) {
		auto &info = TextureFormats[i];
		if (!info.InternalFormat) {
			snprintf_irr(buf, sizeof(buf), "%s -> unsupported", ColorFormatNames[i]);
		} else {
			snprintf_irr(buf, sizeof(buf), "%s -> %#06x %#06x %#06x%s",
					ColorFormatNames[i], info.InternalFormat, info.PixelFormat,
					info.PixelType, info.Converter ? " (c)" : "");
		}
		os::Printer::log(buf, ELL_DEBUG);
	}
}

void COpenGL3DriverBase::loadShaderData(const io::path &vertexShaderName, const io::path &fragmentShaderName, c8 **vertexShaderData, c8 **fragmentShaderData)
{
	io::path vsPath(OGLES2ShaderPath);
	vsPath += vertexShaderName;

	io::path fsPath(OGLES2ShaderPath);
	fsPath += fragmentShaderName;

	*vertexShaderData = 0;
	*fragmentShaderData = 0;

	io::IReadFile *vsFile = FileSystem->createAndOpenFile(vsPath);
	if (!vsFile) {
		std::string warning("Warning: Missing shader files needed to simulate fixed function materials:\n");
		warning.append(vsPath.c_str()).append("\n");
		warning += "Shaderpath can be changed in SIrrCreationParamters::OGLES2ShaderPath";
		os::Printer::log(warning.c_str(), ELL_WARNING);
		return;
	}

	io::IReadFile *fsFile = FileSystem->createAndOpenFile(fsPath);
	if (!fsFile) {
		std::string warning("Warning: Missing shader files needed to simulate fixed function materials:\n");
		warning.append(fsPath.c_str()).append("\n");
		warning += "Shaderpath can be changed in SIrrCreationParamters::OGLES2ShaderPath";
		os::Printer::log(warning.c_str(), ELL_WARNING);
		return;
	}

	long size = vsFile->getSize();
	if (size) {
		*vertexShaderData = new c8[size + 1];
		vsFile->read(*vertexShaderData, size);
		(*vertexShaderData)[size] = 0;
	}
	{
		auto tmp = std::string("Loaded ") + std::to_string(size) + " bytes for vertex shader " + vertexShaderName.c_str();
		os::Printer::log(tmp.c_str(), ELL_INFORMATION);
	}

	size = fsFile->getSize();
	if (size) {
		// if both handles are the same we must reset the file
		if (fsFile == vsFile)
			fsFile->seek(0);

		*fragmentShaderData = new c8[size + 1];
		fsFile->read(*fragmentShaderData, size);
		(*fragmentShaderData)[size] = 0;
	}
	{
		auto tmp = std::string("Loaded ") + std::to_string(size) + " bytes for fragment shader " + fragmentShaderName.c_str();
		os::Printer::log(tmp.c_str(), ELL_INFORMATION);
	}

	vsFile->drop();
	fsFile->drop();
}

void COpenGL3DriverBase::createMaterialRenderers()
{
	// Create callbacks.

	COpenGL3MaterialSolidCB *SolidCB = new COpenGL3MaterialSolidCB();
	COpenGL3MaterialSolidCB *TransparentAlphaChannelCB = new COpenGL3MaterialSolidCB();
	COpenGL3MaterialSolidCB *TransparentAlphaChannelRefCB = new COpenGL3MaterialSolidCB();
	COpenGL3MaterialSolidCB *TransparentVertexAlphaCB = new COpenGL3MaterialSolidCB();
	COpenGL3MaterialOneTextureBlendCB *OneTextureBlendCB = new COpenGL3MaterialOneTextureBlendCB();

	// Create built-in materials.
	// The addition order must be the same as in the E_MATERIAL_TYPE enumeration. Thus the

	const core::stringc VertexShader = OGLES2ShaderPath + "Solid.vsh";

	// EMT_SOLID
	core::stringc FragmentShader = OGLES2ShaderPath + "Solid.fsh";
	addHighLevelShaderMaterialFromFiles(VertexShader, FragmentShader, "", "Solid",
			scene::EPT_TRIANGLES, scene::EPT_TRIANGLE_STRIP, 0, SolidCB, EMT_SOLID, 0);

	// EMT_TRANSPARENT_ALPHA_CHANNEL
	FragmentShader = OGLES2ShaderPath + "TransparentAlphaChannel.fsh";
	addHighLevelShaderMaterialFromFiles(VertexShader, FragmentShader, "", "TransparentAlphaChannel",
			scene::EPT_TRIANGLES, scene::EPT_TRIANGLE_STRIP, 0, TransparentAlphaChannelCB, EMT_TRANSPARENT_ALPHA_CHANNEL, 0);

	// EMT_TRANSPARENT_ALPHA_CHANNEL_REF
	FragmentShader = OGLES2ShaderPath + "TransparentAlphaChannelRef.fsh";
	addHighLevelShaderMaterialFromFiles(VertexShader, FragmentShader, "", "TransparentAlphaChannelRef",
			scene::EPT_TRIANGLES, scene::EPT_TRIANGLE_STRIP, 0, TransparentAlphaChannelRefCB, EMT_SOLID, 0);

	// EMT_TRANSPARENT_VERTEX_ALPHA
	FragmentShader = OGLES2ShaderPath + "TransparentVertexAlpha.fsh";
	addHighLevelShaderMaterialFromFiles(VertexShader, FragmentShader, "", "TransparentVertexAlpha",
			scene::EPT_TRIANGLES, scene::EPT_TRIANGLE_STRIP, 0, TransparentVertexAlphaCB, EMT_TRANSPARENT_ALPHA_CHANNEL, 0);

	// EMT_ONETEXTURE_BLEND
	FragmentShader = OGLES2ShaderPath + "OneTextureBlend.fsh";
	addHighLevelShaderMaterialFromFiles(VertexShader, FragmentShader, "", "OneTextureBlend",
			scene::EPT_TRIANGLES, scene::EPT_TRIANGLE_STRIP, 0, OneTextureBlendCB, EMT_ONETEXTURE_BLEND, 0);

	// Drop callbacks.

	SolidCB->drop();
	TransparentAlphaChannelCB->drop();
	TransparentAlphaChannelRefCB->drop();
	TransparentVertexAlphaCB->drop();
	OneTextureBlendCB->drop();

	// Create 2D material renderers

	c8 *vs2DData = 0;
	c8 *fs2DData = 0;
	loadShaderData(io::path("Renderer2D.vsh"), io::path("Renderer2D.fsh"), &vs2DData, &fs2DData);
	MaterialRenderer2DTexture = new COpenGL3Renderer2D(vs2DData, fs2DData, this, true);
	delete[] vs2DData;
	delete[] fs2DData;
	vs2DData = 0;
	fs2DData = 0;

	loadShaderData(io::path("Renderer2D.vsh"), io::path("Renderer2D_noTex.fsh"), &vs2DData, &fs2DData);
	MaterialRenderer2DNoTexture = new COpenGL3Renderer2D(vs2DData, fs2DData, this, false);
	delete[] vs2DData;
	delete[] fs2DData;
}

bool COpenGL3DriverBase::setMaterialTexture(u32 layerIdx, const video::ITexture *texture)
{
	Material.TextureLayers[layerIdx].Texture = const_cast<ITexture *>(texture); // function uses const-pointer for texture because all draw functions use const-pointers already
	return CacheHandler->getTextureCache().set(0, texture);
}

bool COpenGL3DriverBase::beginScene(u16 clearFlag, SColor clearColor, f32 clearDepth, u8 clearStencil, const SExposedVideoData &videoData, core::rect<s32> *sourceRect)
{
	CNullDriver::beginScene(clearFlag, clearColor, clearDepth, clearStencil, videoData, sourceRect);

	if (ContextManager)
		ContextManager->activateContext(videoData, true);

	clearBuffers(clearFlag, clearColor, clearDepth, clearStencil);

	return true;
}

bool COpenGL3DriverBase::endScene()
{
	CNullDriver::endScene();

	GL.Flush();

	if (ContextManager)
		return ContextManager->swapBuffers();

	return false;
}

//! Returns the transformation set by setTransform
const core::matrix4 &COpenGL3DriverBase::getTransform(E_TRANSFORMATION_STATE state) const
{
	return Matrices[state];
}

//! sets transformation
void COpenGL3DriverBase::setTransform(E_TRANSFORMATION_STATE state, const core::matrix4 &mat)
{
	Matrices[state] = mat;
	Transformation3DChanged = true;
}

bool COpenGL3DriverBase::uploadHardwareBuffer(OpenGLVBO &vbo,
	const void *buffer, size_t bufferSize, scene::E_HARDWARE_MAPPING hint)
{
	accountHWBufferUpload(bufferSize);

	GLenum usage = GL_STATIC_DRAW;
	if (hint == scene::EHM_STREAM)
		usage = GL_STREAM_DRAW;
	else if (hint == scene::EHM_DYNAMIC)
		usage = GL_DYNAMIC_DRAW;

	vbo.upload(buffer, bufferSize, 0, usage);

	return (!TEST_GL_ERROR(this));
}

bool COpenGL3DriverBase::updateVertexHardwareBuffer(SHWBufferLink_opengl *HWBuffer)
{
	if (!HWBuffer)
		return false;

	assert(HWBuffer->IsVertex);
	const auto *vb = HWBuffer->VertexBuffer;
	assert(vb);

	const u32 vertexSize = getVertexPitchFromType(vb->getType());
	const size_t bufferSize = vertexSize * vb->getCount();

	return uploadHardwareBuffer(HWBuffer->Vbo, vb->getData(),
		bufferSize, vb->getHardwareMappingHint());
}

bool COpenGL3DriverBase::updateIndexHardwareBuffer(SHWBufferLink_opengl *HWBuffer)
{
	if (!HWBuffer)
		return false;

	assert(!HWBuffer->IsVertex);
	const auto *ib = HWBuffer->IndexBuffer;
	assert(ib);

	u32 indexSize;
	switch (ib->getType()) {
	case EIT_16BIT:
		indexSize = sizeof(u16);
		break;
	case EIT_32BIT:
		indexSize = sizeof(u32);
		break;
	default:
		return false;
	}

	const size_t bufferSize = ib->getCount() * indexSize;

	return uploadHardwareBuffer(HWBuffer->Vbo, ib->getData(),
		bufferSize, ib->getHardwareMappingHint());
}

bool COpenGL3DriverBase::updateHardwareBuffer(SHWBufferLink *HWBuffer)
{
	if (!HWBuffer)
		return false;

	auto *b = static_cast<SHWBufferLink_opengl *>(HWBuffer);

	if (b->IsVertex) {
		assert(b->VertexBuffer);
		if (b->ChangedID != b->VertexBuffer->getChangedID() || !b->Vbo.exists()) {
			if (!updateVertexHardwareBuffer(b))
				return false;
			b->ChangedID = b->VertexBuffer->getChangedID();
		}
	} else {
		assert(b->IndexBuffer);
		if (b->ChangedID != b->IndexBuffer->getChangedID() || !b->Vbo.exists()) {
			if (!updateIndexHardwareBuffer(b))
				return false;
			b->ChangedID = b->IndexBuffer->getChangedID();
		}
	}
	return true;
}

COpenGL3DriverBase::SHWBufferLink *COpenGL3DriverBase::createHardwareBuffer(const scene::IVertexBuffer *vb)
{
	if (!vb || vb->getHardwareMappingHint() == scene::EHM_NEVER)
		return 0;

	auto *HWBuffer = new SHWBufferLink_opengl(vb);
	registerHardwareBuffer(HWBuffer);

	if (!updateVertexHardwareBuffer(HWBuffer)) {
		deleteHardwareBuffer(HWBuffer);
		return 0;
	}

	return HWBuffer;
}

COpenGL3DriverBase::SHWBufferLink *COpenGL3DriverBase::createHardwareBuffer(const scene::IIndexBuffer *ib)
{
	if (!ib || ib->getHardwareMappingHint() == scene::EHM_NEVER)
		return 0;

	auto *HWBuffer = new SHWBufferLink_opengl(ib);
	registerHardwareBuffer(HWBuffer);

	if (!updateIndexHardwareBuffer(HWBuffer)) {
		deleteHardwareBuffer(HWBuffer);
		return 0;
	}

	return HWBuffer;
}

void COpenGL3DriverBase::deleteHardwareBuffer(SHWBufferLink *HWBuffer)
{
	if (!HWBuffer)
		return;

	auto *b = static_cast<SHWBufferLink_opengl *>(HWBuffer);
	b->Vbo.destroy();

	CNullDriver::deleteHardwareBuffer(HWBuffer);
}

void COpenGL3DriverBase::drawBuffers(const scene::IVertexBuffer *vb,
	const scene::IIndexBuffer *ib, u32 PrimitiveCount,
	scene::E_PRIMITIVE_TYPE PrimitiveType)
{
	if (!vb || !ib)
		return;

	auto *hwvert = static_cast<SHWBufferLink_opengl *>(getBufferLink(vb));
	auto *hwidx = static_cast<SHWBufferLink_opengl *>(getBufferLink(ib));
	updateHardwareBuffer(hwvert);
	updateHardwareBuffer(hwidx);

	const void *vertices = vb->getData();
	if (hwvert) {
		assert(hwvert->IsVertex);
		assert(hwvert->Vbo.exists());
		GL.BindBuffer(GL_ARRAY_BUFFER, hwvert->Vbo.getName());
		vertices = nullptr;
	}

	const void *indexList = ib->getData();
	if (hwidx) {
		assert(!hwidx->IsVertex);
		assert(hwidx->Vbo.exists());
		GL.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, hwidx->Vbo.getName());
		indexList = nullptr;
	}

	drawVertexPrimitiveList(vertices, vb->getCount(), indexList,
		PrimitiveCount, vb->getType(), PrimitiveType, ib->getType());

	if (hwvert)
		GL.BindBuffer(GL_ARRAY_BUFFER, 0);
	if (hwidx)
		GL.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

IRenderTarget *COpenGL3DriverBase::addRenderTarget()
{
	COpenGL3RenderTarget *renderTarget = new COpenGL3RenderTarget(this);
	RenderTargets.push_back(renderTarget);

	return renderTarget;
}

void COpenGL3DriverBase::blitRenderTarget(IRenderTarget *from, IRenderTarget *to)
{
	if (Version.Spec == OpenGLSpec::ES && Version.Major < 3) {
		os::Printer::log("glBlitFramebuffer not supported by OpenGL ES < 3.0", ELL_ERROR);
		return;
	}

	GLuint prev_fbo_id;
	CacheHandler->getFBO(prev_fbo_id);

	COpenGL3RenderTarget *src = static_cast<COpenGL3RenderTarget *>(from);
	COpenGL3RenderTarget *dst = static_cast<COpenGL3RenderTarget *>(to);
	GL.BindFramebuffer(GL.READ_FRAMEBUFFER, src->getBufferID());
	GL.BindFramebuffer(GL.DRAW_FRAMEBUFFER, dst->getBufferID());
	GL.BlitFramebuffer(
			0, 0, src->getSize().Width, src->getSize().Height,
			0, 0, dst->getSize().Width, dst->getSize().Height,
			GL.COLOR_BUFFER_BIT | GL.DEPTH_BUFFER_BIT | GL.STENCIL_BUFFER_BIT, GL.NEAREST);

	// This resets both read and draw framebuffer. Note that we bypass CacheHandler here.
	GL.BindFramebuffer(GL.FRAMEBUFFER, prev_fbo_id);
}

//! draws a vertex primitive list
void COpenGL3DriverBase::drawVertexPrimitiveList(const void *vertices, u32 vertexCount,
		const void *indexList, u32 primitiveCount,
		E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
{
	if (!primitiveCount || !vertexCount)
		return;

	if (!checkPrimitiveCount(primitiveCount))
		return;

	CNullDriver::drawVertexPrimitiveList(vertices, vertexCount, indexList, primitiveCount, vType, pType, iType);

	setRenderStates3DMode();

	drawGeneric(vertices, indexList, primitiveCount, vType, pType, iType);
}

//! draws a vertex primitive list in 2d
void COpenGL3DriverBase::draw2DVertexPrimitiveList(const void *vertices, u32 vertexCount,
		const void *indexList, u32 primitiveCount,
		E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
{
	if (!primitiveCount || !vertexCount)
		return;

	if (!vertices)
		return;

	if (!checkPrimitiveCount(primitiveCount))
		return;

	CNullDriver::draw2DVertexPrimitiveList(vertices, vertexCount, indexList, primitiveCount, vType, pType, iType);

	setRenderStates2DMode(
		Material.MaterialType == EMT_TRANSPARENT_VERTEX_ALPHA,
		Material.getTexture(0),
		Material.MaterialType == EMT_TRANSPARENT_ALPHA_CHANNEL
	);

	drawGeneric(vertices, indexList, primitiveCount, vType, pType, iType);
}

void COpenGL3DriverBase::draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos,
		const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect, SColor color,
		bool useAlphaChannelOfTexture)
{
	if (!texture)
		return;

	if (!sourceRect.isValid())
		return;

	SColor colors[4] = {color, color, color, color};
	draw2DImage(texture, {destPos, sourceRect.getSize()}, sourceRect, clipRect, colors, useAlphaChannelOfTexture);
}

void COpenGL3DriverBase::draw2DImage(const video::ITexture *texture, const core::rect<s32> &destRect,
		const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect,
		const video::SColor *const colors, bool useAlphaChannelOfTexture)
{
	if (!texture)
		return;

	// texcoords need to be flipped horizontally for RTTs
	const bool isRTT = texture->isRenderTarget();
	const core::dimension2du &ss = texture->getOriginalSize();
	const f32 invW = 1.f / static_cast<f32>(ss.Width);
	const f32 invH = 1.f / static_cast<f32>(ss.Height);
	const core::rect<f32> tcoords(
			sourceRect.UpperLeftCorner.X * invW,
			(isRTT ? sourceRect.LowerRightCorner.Y : sourceRect.UpperLeftCorner.Y) * invH,
			sourceRect.LowerRightCorner.X * invW,
			(isRTT ? sourceRect.UpperLeftCorner.Y : sourceRect.LowerRightCorner.Y) * invH);

	const video::SColor temp[4] = {
			0xFFFFFFFF,
			0xFFFFFFFF,
			0xFFFFFFFF,
			0xFFFFFFFF,
		};

	const video::SColor *const useColor = colors ? colors : temp;

	chooseMaterial2D();
	if (!setMaterialTexture(0, texture))
		return;

	setRenderStates2DMode(useColor[0].getAlpha() < 255 || useColor[1].getAlpha() < 255 ||
								  useColor[2].getAlpha() < 255 || useColor[3].getAlpha() < 255,
			true, useAlphaChannelOfTexture);

	const core::dimension2d<u32> &renderTargetSize = getCurrentRenderTargetSize();

	if (clipRect) {
		if (!clipRect->isValid())
			return;

		GL.Enable(GL_SCISSOR_TEST);
		GL.Scissor(clipRect->UpperLeftCorner.X, renderTargetSize.Height - clipRect->LowerRightCorner.Y,
				clipRect->getWidth(), clipRect->getHeight());
	}

	f32 left  = (f32)destRect.UpperLeftCorner.X;
	f32 right = (f32)destRect.LowerRightCorner.X;
	f32 down  = (f32)destRect.LowerRightCorner.Y;
	f32 top   = (f32)destRect.UpperLeftCorner.Y;

	S3DVertex vertices[4];
	vertices[0] = S3DVertex(left, top, 0, 0, 0, 1, useColor[0], tcoords.UpperLeftCorner.X, tcoords.UpperLeftCorner.Y);
	vertices[1] = S3DVertex(right, top, 0, 0, 0, 1, useColor[3], tcoords.LowerRightCorner.X, tcoords.UpperLeftCorner.Y);
	vertices[2] = S3DVertex(right, down, 0, 0, 0, 1, useColor[2], tcoords.LowerRightCorner.X, tcoords.LowerRightCorner.Y);
	vertices[3] = S3DVertex(left, down, 0, 0, 0, 1, useColor[1], tcoords.UpperLeftCorner.X, tcoords.LowerRightCorner.Y);

	drawQuad(vt2DImage, vertices);

	if (clipRect)
		GL.Disable(GL_SCISSOR_TEST);

	TEST_GL_ERROR(this);
}

void COpenGL3DriverBase::draw2DImage(const video::ITexture *texture, u32 layer, bool flip)
{
	if (!texture)
		return;

	chooseMaterial2D();
	if (!setMaterialTexture(0, texture))
		return;

	setRenderStates2DMode(false, true, true);

	S3DVertex quad2DVertices[4];

	quad2DVertices[0].Pos = core::vector3df(-1.f, 1.f, 0.f);
	quad2DVertices[1].Pos = core::vector3df(1.f, 1.f, 0.f);
	quad2DVertices[2].Pos = core::vector3df(1.f, -1.f, 0.f);
	quad2DVertices[3].Pos = core::vector3df(-1.f, -1.f, 0.f);

	f32 modificator = (flip) ? 1.f : 0.f;

	quad2DVertices[0].TCoords = core::vector2df(0.f, 0.f + modificator);
	quad2DVertices[1].TCoords = core::vector2df(1.f, 0.f + modificator);
	quad2DVertices[2].TCoords = core::vector2df(1.f, 1.f - modificator);
	quad2DVertices[3].TCoords = core::vector2df(0.f, 1.f - modificator);

	quad2DVertices[0].Color = SColor(0xFFFFFFFF);
	quad2DVertices[1].Color = SColor(0xFFFFFFFF);
	quad2DVertices[2].Color = SColor(0xFFFFFFFF);
	quad2DVertices[3].Color = SColor(0xFFFFFFFF);

	drawQuad(vt2DImage, quad2DVertices);
}

void COpenGL3DriverBase::draw2DImageBatch(const video::ITexture *texture,
		const core::array<core::position2d<s32>> &positions,
		const core::array<core::rect<s32>> &sourceRects,
		const core::rect<s32> *clipRect,
		SColor color, bool useAlphaChannelOfTexture)
{
	if (!texture)
		return;

	chooseMaterial2D();
	if (!setMaterialTexture(0, texture))
		return;

	setRenderStates2DMode(color.getAlpha() < 255, true, useAlphaChannelOfTexture);

	const core::dimension2d<u32> &renderTargetSize = getCurrentRenderTargetSize();

	if (clipRect) {
		if (!clipRect->isValid())
			return;

		GL.Enable(GL_SCISSOR_TEST);
		GL.Scissor(clipRect->UpperLeftCorner.X, renderTargetSize.Height - clipRect->LowerRightCorner.Y,
				clipRect->getWidth(), clipRect->getHeight());
	}

	const u32 drawCount = core::min_<u32>(positions.size(), sourceRects.size());
	assert(6 * drawCount * sizeof(u16) <= QuadIndexVBO.getSize()); // FIXME split the batch? or let it crash?

	std::vector<S3DVertex> vtx;
	vtx.reserve(drawCount * 4);

	// texcoords need to be flipped horizontally for RTTs
	const bool isRTT = texture->isRenderTarget();
	const core::dimension2du ss = texture->getOriginalSize();
	const f32 invW = 1.f / static_cast<f32>(ss.Width);
	const f32 invH = 1.f / static_cast<f32>(ss.Height);

	for (u32 i = 0; i < drawCount; i++) {
		const core::position2d<s32> targetPos = positions[i];
		const core::rect<s32> sourceRect = sourceRects[i];

		// now draw it.

		const core::rect<f32> tcoords(
			sourceRect.UpperLeftCorner.X * invW,
			(isRTT ? sourceRect.LowerRightCorner.Y : sourceRect.UpperLeftCorner.Y) * invH,
			sourceRect.LowerRightCorner.X * invW,
			(isRTT ? sourceRect.UpperLeftCorner.Y : sourceRect.LowerRightCorner.Y) * invH);

		const core::rect<s32> poss(targetPos, sourceRect.getSize());

		f32 left  = (f32)poss.UpperLeftCorner.X;
		f32 right = (f32)poss.LowerRightCorner.X;
		f32 down  = (f32)poss.LowerRightCorner.Y;
		f32 top   = (f32)poss.UpperLeftCorner.Y;

		vtx.emplace_back(left, top, 0.0f,
				0.0f, 0.0f, 0.0f, color,
				tcoords.UpperLeftCorner.X, tcoords.UpperLeftCorner.Y);
		vtx.emplace_back(right, top, 0.0f,
				0.0f, 0.0f, 0.0f, color,
				tcoords.LowerRightCorner.X, tcoords.UpperLeftCorner.Y);
		vtx.emplace_back(right, down, 0.0f,
				0.0f, 0.0f, 0.0f, color,
				tcoords.LowerRightCorner.X, tcoords.LowerRightCorner.Y);
		vtx.emplace_back(left, down, 0.0f,
				0.0f, 0.0f, 0.0f, color,
				tcoords.UpperLeftCorner.X, tcoords.LowerRightCorner.Y);
	}

	GL.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, QuadIndexVBO.getName());
	drawElements(GL_TRIANGLES, vt2DImage, vtx.data(), vtx.size(), 0, 6 * drawCount);
	GL.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	if (clipRect)
		GL.Disable(GL_SCISSOR_TEST);
}

//! draw a 2d rectangle
void COpenGL3DriverBase::draw2DRectangle(SColor color,
		const core::rect<s32> &position,
		const core::rect<s32> *clip)
{
	draw2DRectangle(position, color, color, color, color, clip);
}

//! draw an 2d rectangle
void COpenGL3DriverBase::draw2DRectangle(const core::rect<s32> &position,
		SColor colorLeftUp, SColor colorRightUp,
		SColor colorLeftDown, SColor colorRightDown,
		const core::rect<s32> *clip)
{
	core::rect<s32> pos = position;

	if (clip)
		pos.clipAgainst(*clip);

	if (!pos.isValid())
		return;

	chooseMaterial2D();
	setMaterialTexture(0, 0);

	setRenderStates2DMode(colorLeftUp.getAlpha() < 255 ||
								  colorRightUp.getAlpha() < 255 ||
								  colorLeftDown.getAlpha() < 255 ||
								  colorRightDown.getAlpha() < 255,
			false, false);

	f32 left  = (f32)pos.UpperLeftCorner.X;
	f32 right = (f32)pos.LowerRightCorner.X;
	f32 down  = (f32)pos.LowerRightCorner.Y;
	f32 top   = (f32)pos.UpperLeftCorner.Y;

	S3DVertex vertices[4];
	vertices[0] = S3DVertex(left,   top, 0, 0, 0, 1, colorLeftUp, 0, 0);
	vertices[1] = S3DVertex(right,  top, 0, 0, 0, 1, colorRightUp, 0, 0);
	vertices[2] = S3DVertex(right, down, 0, 0, 0, 1, colorRightDown, 0, 0);
	vertices[3] = S3DVertex(left,  down, 0, 0, 0, 1, colorLeftDown, 0, 0);

	drawQuad(vtPrimitive, vertices);
}

//! Draws a 2d line.
void COpenGL3DriverBase::draw2DLine(const core::position2d<s32> &start,
		const core::position2d<s32> &end, SColor color)
{
	{
		chooseMaterial2D();
		setMaterialTexture(0, 0);

		setRenderStates2DMode(color.getAlpha() < 255, false, false);

		f32 startX = (f32)start.X;
		f32 endX   = (f32)end.X;
		f32 startY = (f32)start.Y;
		f32 endY   = (f32)end.Y;

		S3DVertex vertices[2];
		vertices[0] = S3DVertex(startX, startY, 0, 0, 0, 1, color, 0, 0);
		vertices[1] = S3DVertex(endX, endY, 0, 0, 0, 1, color, 1, 1);

		drawArrays(GL_LINES, vtPrimitive, vertices, 2);
	}
}

void COpenGL3DriverBase::drawQuad(const VertexType &vertexType, const S3DVertex (&vertices)[4])
{
	drawArrays(GL_TRIANGLE_FAN, vertexType, vertices, 4);
}

void COpenGL3DriverBase::drawArrays(GLenum primitiveType, const VertexType &vertexType, const void *vertices, int vertexCount)
{
	beginDraw(vertexType, reinterpret_cast<uintptr_t>(vertices));
	GL.DrawArrays(primitiveType, 0, vertexCount);
	endDraw(vertexType);
}

void COpenGL3DriverBase::drawElements(GLenum primitiveType, const VertexType &vertexType, const void *vertices, int vertexCount, const u16 *indices, int indexCount)
{
	beginDraw(vertexType, reinterpret_cast<uintptr_t>(vertices));
	GL.DrawRangeElements(primitiveType, 0, vertexCount - 1, indexCount, GL_UNSIGNED_SHORT, indices);
	endDraw(vertexType);
}

void COpenGL3DriverBase::drawGeneric(const void *vertices, const void *indexList,
		u32 primitiveCount,
		E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
{
	auto &vTypeDesc = getVertexTypeDescription(vType);
	beginDraw(vTypeDesc, reinterpret_cast<uintptr_t>(vertices));
	GLenum indexSize = 0;

	switch (iType) {
	case EIT_16BIT:
		indexSize = GL_UNSIGNED_SHORT;
		break;
	case EIT_32BIT:
		indexSize = GL_UNSIGNED_INT;
		break;
	}

	switch (pType) {
	case scene::EPT_POINTS:
	case scene::EPT_POINT_SPRITES:
		GL.DrawArrays(GL_POINTS, 0, primitiveCount);
		break;
	case scene::EPT_LINE_STRIP:
		GL.DrawElements(GL_LINE_STRIP, primitiveCount + 1, indexSize, indexList);
		break;
	case scene::EPT_LINE_LOOP:
		GL.DrawElements(GL_LINE_LOOP, primitiveCount, indexSize, indexList);
		break;
	case scene::EPT_LINES:
		GL.DrawElements(GL_LINES, primitiveCount * 2, indexSize, indexList);
		break;
	case scene::EPT_TRIANGLE_STRIP:
		GL.DrawElements(GL_TRIANGLE_STRIP, primitiveCount + 2, indexSize, indexList);
		break;
	case scene::EPT_TRIANGLE_FAN:
		GL.DrawElements(GL_TRIANGLE_FAN, primitiveCount + 2, indexSize, indexList);
		break;
	case scene::EPT_TRIANGLES:
		GL.DrawElements(GL_TRIANGLES, primitiveCount * 3, indexSize, indexList);
		break;
	default:
		break;
	}

	endDraw(vTypeDesc);
}

void COpenGL3DriverBase::beginDraw(const VertexType &vertexType, uintptr_t verticesBase)
{
	for (auto &attr : vertexType) {
		GL.EnableVertexAttribArray(attr.Index);
		switch (attr.mode) {
		case VertexAttribute::Mode::Regular:
			GL.VertexAttribPointer(attr.Index, attr.ComponentCount, attr.ComponentType, GL_FALSE, vertexType.VertexSize, reinterpret_cast<void *>(verticesBase + attr.Offset));
			break;
		case VertexAttribute::Mode::Normalized:
			GL.VertexAttribPointer(attr.Index, attr.ComponentCount, attr.ComponentType, GL_TRUE, vertexType.VertexSize, reinterpret_cast<void *>(verticesBase + attr.Offset));
			break;
		case VertexAttribute::Mode::Integer:
			GL.VertexAttribIPointer(attr.Index, attr.ComponentCount, attr.ComponentType, vertexType.VertexSize, reinterpret_cast<void *>(verticesBase + attr.Offset));
			break;
		}
	}
}

void COpenGL3DriverBase::endDraw(const VertexType &vertexType)
{
	for (auto &attr : vertexType)
		GL.DisableVertexAttribArray(attr.Index);
}

ITexture *COpenGL3DriverBase::createDeviceDependentTexture(const io::path &name, E_TEXTURE_TYPE type, const std::vector<IImage*> &images)
{
	return new COpenGL3Texture(name, images, type, this);
}

// Same as COpenGLDriver::TextureFlipMatrix
static const core::matrix4 s_texture_flip_matrix = {
	1,  0, 0, 0,
	0, -1, 0, 0,
	0,  1, 1, 0,
	0,  0, 0, 1
};

//! Sets a material.
void COpenGL3DriverBase::setMaterial(const SMaterial &material)
{
	Material = material;
	OverrideMaterial.apply(Material);

	for (u32 i = 0; i < Feature.MaxTextureUnits; ++i) {
		auto *texture = material.getTexture(i);
		CacheHandler->getTextureCache().set(i, texture);
		if (texture) {
			setTransform((E_TRANSFORMATION_STATE)(ETS_TEXTURE_0 + i),
				texture->isRenderTarget()
					? material.getTextureMatrix(i) * s_texture_flip_matrix
					: material.getTextureMatrix(i)
			);
		}
	}
}

//! prints error if an error happened.
bool COpenGL3DriverBase::testGLError(const char *file, int line)
{
	if (!EnableErrorTest)
		return false;

	GLenum g = GL.GetError();
	const char *err = nullptr;
	switch (g) {
	case GL_NO_ERROR:
		return false;
	case GL_INVALID_ENUM:
		err = "GL_INVALID_ENUM";
		break;
	case GL_INVALID_VALUE:
		err = "GL_INVALID_VALUE";
		break;
	case GL_INVALID_OPERATION:
		err = "GL_INVALID_OPERATION";
		break;
	case GL_STACK_OVERFLOW:
		err = "GL_STACK_OVERFLOW";
		break;
	case GL_STACK_UNDERFLOW:
		err = "GL_STACK_UNDERFLOW";
		break;
	case GL_OUT_OF_MEMORY:
		err = "GL_OUT_OF_MEMORY";
		break;
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		err = "GL_INVALID_FRAMEBUFFER_OPERATION";
		break;
#ifdef GL_VERSION_4_5
	case GL_CONTEXT_LOST:
		err = "GL_CONTEXT_LOST";
		break;
#endif
	};

	// Empty the error queue, see <https://www.khronos.org/opengl/wiki/OpenGL_Error>
	bool multiple = false;
	while (GL.GetError() != GL_NO_ERROR)
		multiple = true;

	// basename
	for (char sep : {'/', '\\'}) {
		const char *tmp = strrchr(file, sep);
		if (tmp)
			file = tmp+1;
	}

	char buf[80];
	snprintf_irr(buf, sizeof(buf), "%s %s:%d%s",
		err, file, line, multiple ? " (older errors exist)" : "");
	os::Printer::log(buf, ELL_ERROR);
	return true;
}

void COpenGL3DriverBase::setRenderStates3DMode()
{
	if (LockRenderStateMode)
		return;

	if (CurrentRenderMode != ERM_3D) {
		// Reset Texture Stages
		CacheHandler->setBlend(false);
		CacheHandler->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		ResetRenderStates = true;
	}

	if (ResetRenderStates || LastMaterial != Material) {
		// unset old material

		// unset last 3d material
		if (CurrentRenderMode == ERM_2D && MaterialRenderer2DActive) {
			MaterialRenderer2DActive->OnUnsetMaterial();
			MaterialRenderer2DActive = 0;
		} else if (LastMaterial.MaterialType != Material.MaterialType &&
				   static_cast<u32>(LastMaterial.MaterialType) < MaterialRenderers.size())
			MaterialRenderers[LastMaterial.MaterialType].Renderer->OnUnsetMaterial();

		// set new material.
		if (static_cast<u32>(Material.MaterialType) < MaterialRenderers.size())
			MaterialRenderers[Material.MaterialType].Renderer->OnSetMaterial(
					Material, LastMaterial, ResetRenderStates, this);

		LastMaterial = Material;
		CacheHandler->correctCacheMaterial(LastMaterial);
		ResetRenderStates = false;
	}

	if (static_cast<u32>(Material.MaterialType) < MaterialRenderers.size())
		MaterialRenderers[Material.MaterialType].Renderer->OnRender(this, video::EVT_STANDARD);

	CurrentRenderMode = ERM_3D;
}

//! Can be called by an IMaterialRenderer to make its work easier.
void COpenGL3DriverBase::setBasicRenderStates(const SMaterial &material, const SMaterial &lastmaterial, bool resetAllRenderStates)
{
	// ZBuffer
	switch (material.ZBuffer) {
	case ECFN_DISABLED:
		CacheHandler->setDepthTest(false);
		break;
	case ECFN_LESSEQUAL:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_LEQUAL);
		break;
	case ECFN_EQUAL:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_EQUAL);
		break;
	case ECFN_LESS:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_LESS);
		break;
	case ECFN_NOTEQUAL:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_NOTEQUAL);
		break;
	case ECFN_GREATEREQUAL:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_GEQUAL);
		break;
	case ECFN_GREATER:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_GREATER);
		break;
	case ECFN_ALWAYS:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_ALWAYS);
		break;
	case ECFN_NEVER:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_NEVER);
		break;
	default:
		break;
	}

	// ZWrite
	if (getWriteZBuffer(material)) {
		CacheHandler->setDepthMask(true);
	} else {
		CacheHandler->setDepthMask(false);
	}

	// Back face culling
	if ((material.FrontfaceCulling) && (material.BackfaceCulling)) {
		CacheHandler->setCullFaceFunc(GL_FRONT_AND_BACK);
		CacheHandler->setCullFace(true);
	} else if (material.BackfaceCulling) {
		CacheHandler->setCullFaceFunc(GL_BACK);
		CacheHandler->setCullFace(true);
	} else if (material.FrontfaceCulling) {
		CacheHandler->setCullFaceFunc(GL_FRONT);
		CacheHandler->setCullFace(true);
	} else {
		CacheHandler->setCullFace(false);
	}

	// Color Mask
	CacheHandler->setColorMask(material.ColorMask);

	// Blend Equation
	if (material.BlendOperation == EBO_NONE)
		CacheHandler->setBlend(false);
	else {
		CacheHandler->setBlend(true);

		switch (material.BlendOperation) {
		case EBO_ADD:
			CacheHandler->setBlendEquation(GL_FUNC_ADD);
			break;
		case EBO_SUBTRACT:
			CacheHandler->setBlendEquation(GL_FUNC_SUBTRACT);
			break;
		case EBO_REVSUBTRACT:
			CacheHandler->setBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
			break;
		case EBO_MIN:
			if (BlendMinMaxSupported)
				CacheHandler->setBlendEquation(GL_MIN);
			else
				os::Printer::log("Attempt to use EBO_MIN without driver support", ELL_WARNING);
			break;
		case EBO_MAX:
			if (BlendMinMaxSupported)
				CacheHandler->setBlendEquation(GL_MAX);
			else
				os::Printer::log("Attempt to use EBO_MAX without driver support", ELL_WARNING);
			break;
		default:
			break;
		}
	}

	// Blend Factor
	if (IR(material.BlendFactor) & 0xFFFFFFFF // TODO: why the & 0xFFFFFFFF?
			&& material.MaterialType != EMT_ONETEXTURE_BLEND) {
		E_BLEND_FACTOR srcRGBFact = EBF_ZERO;
		E_BLEND_FACTOR dstRGBFact = EBF_ZERO;
		E_BLEND_FACTOR srcAlphaFact = EBF_ZERO;
		E_BLEND_FACTOR dstAlphaFact = EBF_ZERO;
		E_MODULATE_FUNC modulo = EMFN_MODULATE_1X;
		u32 alphaSource = 0;

		unpack_textureBlendFuncSeparate(srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulo, alphaSource, material.BlendFactor);

		CacheHandler->setBlendFuncSeparate(getGLBlend(srcRGBFact), getGLBlend(dstRGBFact),
				getGLBlend(srcAlphaFact), getGLBlend(dstAlphaFact));
	}

	// fillmode
	if (Version.Spec != OpenGLSpec::ES && // not supported in gles
			(resetAllRenderStates ||
			lastmaterial.Wireframe != material.Wireframe ||
			lastmaterial.PointCloud != material.PointCloud)) {
		GL.PolygonMode(GL_FRONT_AND_BACK,
				material.Wireframe ? GL_LINE :
				material.PointCloud ? GL_POINT :
				GL_FILL);
	}

	// Polygon Offset
	if (resetAllRenderStates ||
			lastmaterial.PolygonOffsetDepthBias != material.PolygonOffsetDepthBias ||
			lastmaterial.PolygonOffsetSlopeScale != material.PolygonOffsetSlopeScale) {
		if (material.PolygonOffsetDepthBias || material.PolygonOffsetSlopeScale) {
			GL.Enable(GL.POLYGON_OFFSET_FILL);
			GL.PolygonOffset(material.PolygonOffsetSlopeScale, material.PolygonOffsetDepthBias);
		} else {
			GL.Disable(GL.POLYGON_OFFSET_FILL);
		}
	}

	if (resetAllRenderStates || lastmaterial.Thickness != material.Thickness)
		GL.LineWidth(core::clamp(static_cast<GLfloat>(material.Thickness), DimAliasedLine[0], DimAliasedLine[1]));

	// Anti aliasing
	// Deal with MSAA even if it's not enabled in the OpenGL context, we might be
	// rendering to an FBO with multisampling.
	if (resetAllRenderStates || lastmaterial.AntiAliasing != material.AntiAliasing) {
		if (material.AntiAliasing & EAAM_ALPHA_TO_COVERAGE)
			GL.Enable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		else if (lastmaterial.AntiAliasing & EAAM_ALPHA_TO_COVERAGE)
			GL.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	}

	// Texture parameters
	setTextureRenderStates(material, resetAllRenderStates);
}

//! Compare in SMaterial doesn't check texture parameters, so we should call this on each OnRender call.
void COpenGL3DriverBase::setTextureRenderStates(const SMaterial &material, bool resetAllRenderstates)
{
	// Set textures to TU/TIU and apply filters to them

	for (s32 i = Feature.MaxTextureUnits - 1; i >= 0; --i) {
		const COpenGL3Texture *tmpTexture = CacheHandler->getTextureCache()[i];

		if (!tmpTexture)
			continue;

		GLenum tmpTextureType = tmpTexture->getOpenGLTextureType();

		CacheHandler->setActiveTexture(GL_TEXTURE0 + i);

		const auto &layer = material.TextureLayers[i];
		auto &states = tmpTexture->getStatesCache();

		if (resetAllRenderstates)
			states.IsCached = false;

		if (!states.IsCached || layer.MagFilter != states.MagFilter) {
			E_TEXTURE_MAG_FILTER magFilter = layer.MagFilter;
			GL.TexParameteri(tmpTextureType, GL_TEXTURE_MAG_FILTER,
					magFilter == ETMAGF_NEAREST ? GL_NEAREST : (assert(magFilter == ETMAGF_LINEAR), GL_LINEAR));

			states.MagFilter = magFilter;
		}

		if (material.UseMipMaps && tmpTexture->hasMipMaps()) {
			if (!states.IsCached || layer.MinFilter != states.MinFilter ||
					!states.MipMapStatus) {
				E_TEXTURE_MIN_FILTER minFilter = layer.MinFilter;
				GL.TexParameteri(tmpTextureType, GL_TEXTURE_MIN_FILTER,
						minFilter == ETMINF_NEAREST_MIPMAP_NEAREST ? GL_NEAREST_MIPMAP_NEAREST : minFilter == ETMINF_LINEAR_MIPMAP_NEAREST ? GL_LINEAR_MIPMAP_NEAREST
																						 : minFilter == ETMINF_NEAREST_MIPMAP_LINEAR       ? GL_NEAREST_MIPMAP_LINEAR
																																		   : (assert(minFilter == ETMINF_LINEAR_MIPMAP_LINEAR), GL_LINEAR_MIPMAP_LINEAR));

				states.MinFilter = minFilter;
				states.MipMapStatus = true;
			}
		} else {
			if (!states.IsCached || layer.MinFilter != states.MinFilter ||
					states.MipMapStatus) {
				E_TEXTURE_MIN_FILTER minFilter = layer.MinFilter;
				GL.TexParameteri(tmpTextureType, GL_TEXTURE_MIN_FILTER,
						(minFilter == ETMINF_NEAREST_MIPMAP_NEAREST || minFilter == ETMINF_NEAREST_MIPMAP_LINEAR) ? GL_NEAREST : (assert(minFilter == ETMINF_LINEAR_MIPMAP_NEAREST || minFilter == ETMINF_LINEAR_MIPMAP_LINEAR), GL_LINEAR));

				states.MinFilter = minFilter;
				states.MipMapStatus = false;
			}
		}

		if (LODBiasSupported &&
			(!states.IsCached || layer.LODBias != states.LODBias)) {
			if (layer.LODBias) {
				const float tmp = core::clamp(layer.LODBias * 0.125f, -MaxTextureLODBias, MaxTextureLODBias);
				GL.TexParameterf(tmpTextureType, GL.TEXTURE_LOD_BIAS, tmp);
			} else
				GL.TexParameterf(tmpTextureType, GL.TEXTURE_LOD_BIAS, 0.f);

			states.LODBias = layer.LODBias;
		}

		if (AnisotropicFilterSupported &&
				(!states.IsCached || layer.AnisotropicFilter != states.AnisotropicFilter)) {
			GL.TexParameteri(tmpTextureType, GL.TEXTURE_MAX_ANISOTROPY,
					layer.AnisotropicFilter > 1 ? core::min_(MaxAnisotropy, layer.AnisotropicFilter) : 1);

			states.AnisotropicFilter = layer.AnisotropicFilter;
		}

		if (!states.IsCached || layer.TextureWrapU != states.WrapU) {
			GL.TexParameteri(tmpTextureType, GL_TEXTURE_WRAP_S, getTextureWrapMode(layer.TextureWrapU));
			states.WrapU = layer.TextureWrapU;
		}

		if (!states.IsCached || layer.TextureWrapV != states.WrapV) {
			GL.TexParameteri(tmpTextureType, GL_TEXTURE_WRAP_T, getTextureWrapMode(layer.TextureWrapV));
			states.WrapV = layer.TextureWrapV;
		}

		states.IsCached = true;
	}
}

// Get OpenGL ES2.0 texture wrap mode from Irrlicht wrap mode.
GLint COpenGL3DriverBase::getTextureWrapMode(u8 clamp) const
{
	switch (clamp) {
	case ETC_CLAMP:
	case ETC_CLAMP_TO_EDGE:
	case ETC_CLAMP_TO_BORDER:
		return GL_CLAMP_TO_EDGE;
	case ETC_MIRROR:
		return GL_REPEAT;
	default:
		return GL_REPEAT;
	}
}

//! sets the needed renderstates
void COpenGL3DriverBase::setRenderStates2DMode(bool alpha, bool texture, bool alphaChannel)
{
	if (LockRenderStateMode)
		return;

	COpenGL3Renderer2D *nextActiveRenderer = texture ? MaterialRenderer2DTexture : MaterialRenderer2DNoTexture;

	if (CurrentRenderMode != ERM_2D) {
		// unset last 3d material
		if (CurrentRenderMode == ERM_3D) {
			if (static_cast<u32>(LastMaterial.MaterialType) < MaterialRenderers.size())
				MaterialRenderers[LastMaterial.MaterialType].Renderer->OnUnsetMaterial();
		}

		CurrentRenderMode = ERM_2D;
	} else if (MaterialRenderer2DActive && MaterialRenderer2DActive != nextActiveRenderer) {
		MaterialRenderer2DActive->OnUnsetMaterial();
	}

	MaterialRenderer2DActive = nextActiveRenderer;

	MaterialRenderer2DActive->OnSetMaterial(Material, LastMaterial, true, 0);
	LastMaterial = Material;
	CacheHandler->correctCacheMaterial(LastMaterial);

	// no alphaChannel without texture
	alphaChannel &= texture;

	if (alphaChannel || alpha) {
		CacheHandler->setBlend(true);
		CacheHandler->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		CacheHandler->setBlendEquation(GL_FUNC_ADD);
	} else
		CacheHandler->setBlend(false);

	Material.setTexture(0, const_cast<COpenGL3Texture *>(CacheHandler->getTextureCache().get(0)));
	setTransform(ETS_TEXTURE_0, core::IdentityMatrix);

	if (texture) {
		if (OverrideMaterial2DEnabled)
			setTextureRenderStates(OverrideMaterial2D, false);
		else
			setTextureRenderStates(InitMaterial2D, false);
	}

	MaterialRenderer2DActive->OnRender(this, video::EVT_STANDARD);
}

void COpenGL3DriverBase::chooseMaterial2D()
{
	if (!OverrideMaterial2DEnabled)
		Material = InitMaterial2D;

	if (OverrideMaterial2DEnabled) {
		OverrideMaterial2D.ZWriteEnable = EZW_OFF;
		OverrideMaterial2D.ZBuffer = ECFN_DISABLED; // it will be ECFN_DISABLED after merge

		Material = OverrideMaterial2D;
	}
}

//! \return Returns the name of the video driver.
const char *COpenGL3DriverBase::getName() const
{
	return Name.c_str();
}

void COpenGL3DriverBase::setViewPort(const core::rect<s32> &area)
{
	core::rect<s32> vp = area;
	core::rect<s32> rendert(0, 0, getCurrentRenderTargetSize().Width, getCurrentRenderTargetSize().Height);
	vp.clipAgainst(rendert);

	if (vp.getHeight() > 0 && vp.getWidth() > 0)
		CacheHandler->setViewport(vp.UpperLeftCorner.X, getCurrentRenderTargetSize().Height - vp.UpperLeftCorner.Y - vp.getHeight(), vp.getWidth(), vp.getHeight());

	ViewPort = vp;
}

void COpenGL3DriverBase::setViewPortRaw(u32 width, u32 height)
{
	CacheHandler->setViewport(0, 0, width, height);
	ViewPort = core::recti(0, 0, width, height);
}

//! Draws a 3d line.
void COpenGL3DriverBase::draw3DLine(const core::vector3df &start,
		const core::vector3df &end, SColor color)
{
	setRenderStates3DMode();

	S3DVertex vertices[2];
	vertices[0] = S3DVertex(start.X, start.Y, start.Z, 0, 0, 1, color, 0, 0);
	vertices[1] = S3DVertex(end.X, end.Y, end.Z, 0, 0, 1, color, 0, 0);

	drawArrays(GL_LINES, vtPrimitive, vertices, 2);
}

//! Only used by the internal engine. Used to notify the driver that
//! the window was resized.
void COpenGL3DriverBase::OnResize(const core::dimension2d<u32> &size)
{
	CNullDriver::OnResize(size);
	CacheHandler->setViewport(0, 0, size.Width, size.Height);
	Transformation3DChanged = true;
}

//! Returns type of video driver
E_DRIVER_TYPE COpenGL3DriverBase::getDriverType() const
{
	return EDT_OPENGL3;
}

//! returns color format
ECOLOR_FORMAT COpenGL3DriverBase::getColorFormat() const
{
	return ColorFormat;
}

//! Get a vertex shader constant index.
s32 COpenGL3DriverBase::getVertexShaderConstantID(const c8 *name)
{
	return getPixelShaderConstantID(name);
}

//! Get a pixel shader constant index.
s32 COpenGL3DriverBase::getPixelShaderConstantID(const c8 *name)
{
	os::Printer::log("Error: Please call services->getPixelShaderConstantID(), not VideoDriver->getPixelShaderConstantID().");
	return -1;
}

//! Sets a constant for the vertex shader based on an index.
bool COpenGL3DriverBase::setVertexShaderConstant(s32 index, const f32 *floats, int count)
{
	os::Printer::log("Error: Please call services->setVertexShaderConstant(), not VideoDriver->setVertexShaderConstant().");
	return false;
}

//! Int interface for the above.
bool COpenGL3DriverBase::setVertexShaderConstant(s32 index, const s32 *ints, int count)
{
	os::Printer::log("Error: Please call services->setVertexShaderConstant(), not VideoDriver->setVertexShaderConstant().");
	return false;
}

bool COpenGL3DriverBase::setVertexShaderConstant(s32 index, const u32 *ints, int count)
{
	os::Printer::log("Error: Please call services->setVertexShaderConstant(), not VideoDriver->setVertexShaderConstant().");
	return false;
}

//! Sets a constant for the pixel shader based on an index.
bool COpenGL3DriverBase::setPixelShaderConstant(s32 index, const f32 *floats, int count)
{
	os::Printer::log("Error: Please call services->setPixelShaderConstant(), not VideoDriver->setPixelShaderConstant().");
	return false;
}

//! Int interface for the above.
bool COpenGL3DriverBase::setPixelShaderConstant(s32 index, const s32 *ints, int count)
{
	os::Printer::log("Error: Please call services->setPixelShaderConstant(), not VideoDriver->setPixelShaderConstant().");
	return false;
}

bool COpenGL3DriverBase::setPixelShaderConstant(s32 index, const u32 *ints, int count)
{
	os::Printer::log("Error: Please call services->setPixelShaderConstant(), not VideoDriver->setPixelShaderConstant().");
	return false;
}

//! Adds a new material renderer to the VideoDriver, using GLSL to render geometry.
s32 COpenGL3DriverBase::addHighLevelShaderMaterial(
		const c8 *vertexShaderProgram,
		const c8 *pixelShaderProgram,
		const c8 *geometryShaderProgram,
		const c8 *shaderName,
		scene::E_PRIMITIVE_TYPE inType,
		scene::E_PRIMITIVE_TYPE outType,
		u32 verticesOut,
		IShaderConstantSetCallBack *callback,
		E_MATERIAL_TYPE baseMaterial,
		s32 userData)
{
	s32 nr = -1;
	COpenGL3MaterialRenderer *r = new COpenGL3MaterialRenderer(
			this, nr, vertexShaderProgram,
			pixelShaderProgram, shaderName,
			callback, baseMaterial, userData);

	r->drop();
	return nr;
}

//! Returns a pointer to the IVideoDriver interface. (Implementation for
//! IMaterialRendererServices)
IVideoDriver *COpenGL3DriverBase::getVideoDriver()
{
	return this;
}

//! Returns pointer to the IGPUProgrammingServices interface.
IGPUProgrammingServices *COpenGL3DriverBase::getGPUProgrammingServices()
{
	return this;
}

ITexture *COpenGL3DriverBase::addRenderTargetTexture(const core::dimension2d<u32> &size,
		const io::path &name, const ECOLOR_FORMAT format)
{
	return addRenderTargetTextureMs(size, 0, name, format);
}

ITexture *COpenGL3DriverBase::addRenderTargetTextureMs(const core::dimension2d<u32> &size, u8 msaa,
		const io::path &name, const ECOLOR_FORMAT format)
{
	COpenGL3Texture *renderTargetTexture = new COpenGL3Texture(name, size, msaa > 0 ? ETT_2D_MS : ETT_2D, format, this, msaa);
	addTexture(renderTargetTexture);
	renderTargetTexture->drop();

	return renderTargetTexture;
}

ITexture *COpenGL3DriverBase::addRenderTargetTextureCubemap(const u32 sideLen, const io::path &name, const ECOLOR_FORMAT format)
{
	bool supportForFBO = (Feature.ColorAttachment > 0);

	const core::dimension2d<u32> size(sideLen, sideLen);
	core::dimension2du destSize(size);

	if (!supportForFBO) {
		destSize = core::dimension2d<u32>(core::min_(size.Width, ScreenSize.Width), core::min_(size.Height, ScreenSize.Height));
		destSize = destSize.getOptimalSize((size == size.getOptimalSize()), false, false);
	}

	COpenGL3Texture *renderTargetTexture = new COpenGL3Texture(name, destSize, ETT_CUBEMAP, format, this);
	addTexture(renderTargetTexture);
	renderTargetTexture->drop();

	return renderTargetTexture;
}

//! Returns the maximum amount of primitives
u32 COpenGL3DriverBase::getMaximalPrimitiveCount() const
{
	return Version.Spec == OpenGLSpec::ES ? 65535 : 0x7fffffff;
}

bool COpenGL3DriverBase::setRenderTargetEx(IRenderTarget *target, u16 clearFlag, SColor clearColor, f32 clearDepth, u8 clearStencil)
{
	if (target && target->getDriverType() != getDriverType()) {
		os::Printer::log("Fatal Error: Tried to set a render target not owned by OpenGL 3 driver.", ELL_ERROR);
		return false;
	}

	if (CurrentRenderTarget) {
		// Update mip-map of the generated texture, if enabled.
		auto textures = CurrentRenderTarget->getTexture();
		for (size_t i = 0; i < textures.size(); ++i)
			textures[i]->regenerateMipMapLevels();
	}

	core::dimension2d<u32> destRenderTargetSize(0, 0);

	if (target) {
		COpenGL3RenderTarget *renderTarget = static_cast<COpenGL3RenderTarget *>(target);

		CacheHandler->setFBO(renderTarget->getBufferID());
		renderTarget->update();

		destRenderTargetSize = renderTarget->getSize();

		setViewPortRaw(destRenderTargetSize.Width, destRenderTargetSize.Height);
	} else {
		CacheHandler->setFBO(0);

		destRenderTargetSize = core::dimension2d<u32>(0, 0);

		setViewPortRaw(ScreenSize.Width, ScreenSize.Height);
	}

	if (CurrentRenderTargetSize != destRenderTargetSize) {
		CurrentRenderTargetSize = destRenderTargetSize;

		Transformation3DChanged = true;
	}

	CurrentRenderTarget = target;

	clearBuffers(clearFlag, clearColor, clearDepth, clearStencil);

	return true;
}

void COpenGL3DriverBase::clearBuffers(u16 flag, SColor color, f32 depth, u8 stencil)
{
	GLbitfield mask = 0;
	u8 colorMask = 0;
	bool depthMask = false;

	CacheHandler->getColorMask(colorMask);
	CacheHandler->getDepthMask(depthMask);

	if (flag & ECBF_COLOR) {
		CacheHandler->setColorMask(ECP_ALL);

		const f32 inv = 1.0f / 255.0f;
		GL.ClearColor(color.getRed() * inv, color.getGreen() * inv,
				color.getBlue() * inv, color.getAlpha() * inv);

		mask |= GL_COLOR_BUFFER_BIT;
	}

	if (flag & ECBF_DEPTH) {
		CacheHandler->setDepthMask(true);
		GL.ClearDepthf(depth);
		mask |= GL_DEPTH_BUFFER_BIT;
	}

	if (flag & ECBF_STENCIL) {
		GL.ClearStencil(stencil);
		mask |= GL_STENCIL_BUFFER_BIT;
	}

	if (mask)
		GL.Clear(mask);

	CacheHandler->setColorMask(colorMask);
	CacheHandler->setDepthMask(depthMask);
}

//! Returns an image created from the last rendered frame.
// We want to read the front buffer to get the latest render finished.
// This is not possible under ogl-es, though, so one has to call this method
// outside of the render loop only.
IImage *COpenGL3DriverBase::createScreenShot(video::ECOLOR_FORMAT format, video::E_RENDER_TARGET target)
{
	if (target == video::ERT_MULTI_RENDER_TEXTURES || target == video::ERT_RENDER_TEXTURE || target == video::ERT_STEREO_BOTH_BUFFERS)
		return 0;

	GLint internalformat = GL_RGBA;
	GLint type = GL_UNSIGNED_BYTE;
	{
		//			GL.GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &internalformat);
		//			GL.GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
		// there's a format we don't support ATM
		if (GL_UNSIGNED_SHORT_4_4_4_4 == type) {
			internalformat = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
		}
	}

	IImage *newImage = 0;
	if (GL_RGBA == internalformat) {
		if (GL_UNSIGNED_BYTE == type)
			newImage = new CImage(ECF_A8R8G8B8, ScreenSize);
		else
			newImage = new CImage(ECF_A1R5G5B5, ScreenSize);
	} else {
		if (GL_UNSIGNED_BYTE == type)
			newImage = new CImage(ECF_R8G8B8, ScreenSize);
		else
			newImage = new CImage(ECF_R5G6B5, ScreenSize);
	}

	if (!newImage)
		return 0;

	u8 *pixels = static_cast<u8 *>(newImage->getData());
	if (!pixels) {
		newImage->drop();
		return 0;
	}

	GL.ReadPixels(0, 0, ScreenSize.Width, ScreenSize.Height, internalformat, type, pixels);
	TEST_GL_ERROR(this);

	// opengl images are horizontally flipped, so we have to fix that here.
	const s32 pitch = newImage->getPitch();
	u8 *p2 = pixels + (ScreenSize.Height - 1) * pitch;
	u8 *tmpBuffer = new u8[pitch];
	for (u32 i = 0; i < ScreenSize.Height; i += 2) {
		memcpy(tmpBuffer, pixels, pitch);
		memcpy(pixels, p2, pitch);
		memcpy(p2, tmpBuffer, pitch);
		pixels += pitch;
		p2 -= pitch;
	}
	delete[] tmpBuffer;

	// also GL_RGBA doesn't match the internal encoding of the image (which is BGRA)
	if (GL_RGBA == internalformat && GL_UNSIGNED_BYTE == type) {
		pixels = static_cast<u8 *>(newImage->getData());
		for (u32 i = 0; i < ScreenSize.Height; i++) {
			for (u32 j = 0; j < ScreenSize.Width; j++) {
				u32 c = *(u32 *)(pixels + 4 * j);
				*(u32 *)(pixels + 4 * j) = (c & 0xFF00FF00) |
										   ((c & 0x00FF0000) >> 16) | ((c & 0x000000FF) << 16);
			}
			pixels += pitch;
		}
	}

	if (TEST_GL_ERROR(this)) {
		newImage->drop();
		return 0;
	}
	return newImage;
}

void COpenGL3DriverBase::removeTexture(ITexture *texture)
{
	CacheHandler->getTextureCache().remove(texture);
	CNullDriver::removeTexture(texture);
}

core::dimension2du COpenGL3DriverBase::getMaxTextureSize() const
{
	return core::dimension2du(MaxTextureSize, MaxTextureSize);
}

GLenum COpenGL3DriverBase::getGLBlend(E_BLEND_FACTOR factor) const
{
	static GLenum const blendTable[] = {
			GL_ZERO,
			GL_ONE,
			GL_DST_COLOR,
			GL_ONE_MINUS_DST_COLOR,
			GL_SRC_COLOR,
			GL_ONE_MINUS_SRC_COLOR,
			GL_SRC_ALPHA,
			GL_ONE_MINUS_SRC_ALPHA,
			GL_DST_ALPHA,
			GL_ONE_MINUS_DST_ALPHA,
			GL_SRC_ALPHA_SATURATE,
		};

	return blendTable[factor];
}

bool COpenGL3DriverBase::getColorFormatParameters(ECOLOR_FORMAT format, GLint &internalFormat, GLenum &pixelFormat,
		GLenum &pixelType, void (**converter)(const void *, s32, void *)) const
{
	auto &info = TextureFormats[format];
	internalFormat = info.InternalFormat;
	pixelFormat = info.PixelFormat;
	pixelType = info.PixelType;
	*converter = info.Converter;
	return info.InternalFormat != 0;
}

bool COpenGL3DriverBase::queryTextureFormat(ECOLOR_FORMAT format) const
{
	return TextureFormats[format].InternalFormat != 0;
}

bool COpenGL3DriverBase::needsTransparentRenderPass(const video::SMaterial &material) const
{
	return CNullDriver::needsTransparentRenderPass(material) || material.isAlphaBlendOperation();
}

const SMaterial &COpenGL3DriverBase::getCurrentMaterial() const
{
	return Material;
}

COpenGL3CacheHandler *COpenGL3DriverBase::getCacheHandler() const
{
	return CacheHandler;
}

} // end namespace
