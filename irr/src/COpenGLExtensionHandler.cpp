// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "COpenGLExtensionHandler.h"

#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "irrString.h"
#include "SMaterial.h"
#include "fast_atof.h"
#include "IContextManager.h"

namespace video
{

bool COpenGLExtensionHandler::needsDSAFramebufferHack = true;

COpenGLExtensionHandler::COpenGLExtensionHandler() :
		StencilBuffer(false), TextureCompressionExtension(false),
		MaxAnisotropy(1), MaxAuxBuffers(0), MaxIndices(65535),
		MaxTextureSize(1), MaxGeometryVerticesOut(0),
		MaxTextureLODBias(0.f), Version(0), ShaderLanguageVersion(0),
		OcclusionQuerySupport(false), IsAtiRadeonX(false), pGlActiveTexture(0), pGlActiveTextureARB(0), pGlClientActiveTextureARB(0),
		pGlGenProgramsARB(0), pGlGenProgramsNV(0),
		pGlBindProgramARB(0), pGlBindProgramNV(0),
		pGlDeleteProgramsARB(0), pGlDeleteProgramsNV(0),
		pGlProgramStringARB(0), pGlLoadProgramNV(0),
		pGlProgramLocalParameter4fvARB(0),
		pGlCreateShaderObjectARB(0), pGlShaderSourceARB(0),
		pGlCompileShaderARB(0), pGlCreateProgramObjectARB(0), pGlAttachObjectARB(0),
		pGlLinkProgramARB(0), pGlUseProgramObjectARB(0), pGlDeleteObjectARB(0),
		pGlCreateProgram(0), pGlUseProgram(0),
		pGlDeleteProgram(0), pGlDeleteShader(0),
		pGlGetAttachedObjectsARB(0), pGlGetAttachedShaders(0),
		pGlCreateShader(0), pGlShaderSource(0), pGlCompileShader(0),
		pGlAttachShader(0), pGlLinkProgram(0),
		pGlGetInfoLogARB(0), pGlGetShaderInfoLog(0), pGlGetProgramInfoLog(0),
		pGlGetObjectParameterivARB(0), pGlGetShaderiv(0), pGlGetProgramiv(0),
		pGlGetUniformLocationARB(0), pGlGetUniformLocation(0),
		pGlUniform1fvARB(0), pGlUniform2fvARB(0), pGlUniform3fvARB(0), pGlUniform4fvARB(0),
		pGlUniform1ivARB(0), pGlUniform2ivARB(0), pGlUniform3ivARB(0), pGlUniform4ivARB(0),
		pGlUniform1uiv(0), pGlUniform2uiv(0), pGlUniform3uiv(0), pGlUniform4uiv(0),
		pGlUniformMatrix2fvARB(0), pGlUniformMatrix2x3fv(0), pGlUniformMatrix2x4fv(0),
		pGlUniformMatrix3x2fv(0), pGlUniformMatrix3fvARB(0), pGlUniformMatrix3x4fv(0),
		pGlUniformMatrix4x2fv(0), pGlUniformMatrix4x3fv(0), pGlUniformMatrix4fvARB(0),
		pGlGetActiveUniformARB(0), pGlGetActiveUniform(0),
		pGlPointParameterfARB(0), pGlPointParameterfvARB(0),
		pGlStencilFuncSeparate(0), pGlStencilOpSeparate(0),
		pGlStencilFuncSeparateATI(0), pGlStencilOpSeparateATI(0),
		pGlCompressedTexImage2D(0), pGlCompressedTexSubImage2D(0),
		// ARB framebuffer object
		pGlBindFramebuffer(0), pGlDeleteFramebuffers(0), pGlGenFramebuffers(0),
		pGlCheckFramebufferStatus(0), pGlFramebufferTexture2D(0),
		pGlBindRenderbuffer(0), pGlDeleteRenderbuffers(0), pGlGenRenderbuffers(0),
		pGlRenderbufferStorage(0), pGlFramebufferRenderbuffer(0), pGlGenerateMipmap(0),
		// EXT framebuffer object
		pGlBindFramebufferEXT(0), pGlDeleteFramebuffersEXT(0), pGlGenFramebuffersEXT(0),
		pGlCheckFramebufferStatusEXT(0), pGlFramebufferTexture2DEXT(0),
		pGlBindRenderbufferEXT(0), pGlDeleteRenderbuffersEXT(0), pGlGenRenderbuffersEXT(0),
		pGlRenderbufferStorageEXT(0), pGlFramebufferRenderbufferEXT(0), pGlGenerateMipmapEXT(0),
		pGlActiveStencilFaceEXT(0),
		// MRTs
		pGlDrawBuffersARB(0), pGlDrawBuffersATI(0),
		pGlGenBuffersARB(0), pGlBindBufferARB(0), pGlBufferDataARB(0), pGlDeleteBuffersARB(0),
		pGlBufferSubDataARB(0), pGlGetBufferSubDataARB(0), pGlMapBufferARB(0), pGlUnmapBufferARB(0),
		pGlIsBufferARB(0), pGlGetBufferParameterivARB(0), pGlGetBufferPointervARB(0),
		pGlProvokingVertexARB(0), pGlProvokingVertexEXT(0),
		pGlProgramParameteriARB(0), pGlProgramParameteriEXT(0),
		pGlGenQueriesARB(0), pGlDeleteQueriesARB(0), pGlIsQueryARB(0),
		pGlBeginQueryARB(0), pGlEndQueryARB(0), pGlGetQueryivARB(0),
		pGlGetQueryObjectivARB(0), pGlGetQueryObjectuivARB(0),
		pGlGenOcclusionQueriesNV(0), pGlDeleteOcclusionQueriesNV(0),
		pGlIsOcclusionQueryNV(0), pGlBeginOcclusionQueryNV(0),
		pGlEndOcclusionQueryNV(0), pGlGetOcclusionQueryivNV(0),
		pGlGetOcclusionQueryuivNV(0),
		// Blend
		pGlBlendFuncSeparateEXT(0), pGlBlendFuncSeparate(0),
		pGlBlendEquationEXT(0), pGlBlendEquation(0), pGlBlendEquationSeparateEXT(0), pGlBlendEquationSeparate(0),
		// Indexed
		pGlEnableIndexedEXT(0), pGlDisableIndexedEXT(0),
		pGlColorMaskIndexedEXT(0),
		pGlBlendFuncIndexedAMD(0), pGlBlendFunciARB(0), pGlBlendFuncSeparateIndexedAMD(0), pGlBlendFuncSeparateiARB(0),
		pGlBlendEquationIndexedAMD(0), pGlBlendEquationiARB(0), pGlBlendEquationSeparateIndexedAMD(0), pGlBlendEquationSeparateiARB(0),
		// DSA
		pGlTextureStorage2D(0), pGlTextureStorage3D(0), pGlTextureSubImage2D(0), pGlGetTextureImage(0), pGlNamedFramebufferTexture(0),
		pGlTextureParameteri(0), pGlTextureParameterf(0), pGlTextureParameteriv(0), pGlTextureParameterfv(0),
		pGlCreateTextures(0), pGlCreateFramebuffers(0), pGlBindTextures(0), pGlGenerateTextureMipmap(0),
		// DSA with EXT or functions to simulate it
		pGlTextureStorage2DEXT(0), pGlTexStorage2D(0), pGlTextureStorage3DEXT(0), pGlTexStorage3D(0), pGlTextureSubImage2DEXT(0), pGlGetTextureImageEXT(0),
		pGlNamedFramebufferTextureEXT(0), pGlFramebufferTexture(0), pGlGenerateTextureMipmapEXT(0)
#if defined(GLX_SGI_swap_control)
		,
		pGlxSwapIntervalSGI(0)
#endif
#if defined(GLX_EXT_swap_control)
		,
		pGlxSwapIntervalEXT(0)
#endif
#if defined(WGL_EXT_swap_control)
		,
		pWglSwapIntervalEXT(0)
#endif
#if defined(GLX_MESA_swap_control)
		,
		pGlxSwapIntervalMESA(0)
#endif
{
	for (u32 i = 0; i < IRR_OpenGL_Feature_Count; ++i)
		FeatureAvailable[i] = false;
	DimAliasedLine[0] = 1.f;
	DimAliasedLine[1] = 1.f;
	DimAliasedPoint[0] = 1.f;
	DimAliasedPoint[1] = 1.f;
	DimSmoothedLine[0] = 1.f;
	DimSmoothedLine[1] = 1.f;
	DimSmoothedPoint[0] = 1.f;
	DimSmoothedPoint[1] = 1.f;
}

void COpenGLExtensionHandler::dump(ELOG_LEVEL logLevel) const
{
	for (u32 i = 0; i < IRR_OpenGL_Feature_Count; ++i)
		os::Printer::log(OpenGLFeatureStrings[i], FeatureAvailable[i] ? " true" : " false", logLevel);
}

void COpenGLExtensionHandler::initExtensions(video::IContextManager *cmgr, bool stencilBuffer)
{
	const f32 ogl_ver = core::fast_atof(reinterpret_cast<const c8 *>(glGetString(GL_VERSION)));
	Version = static_cast<u16>(core::floor32(ogl_ver) * 100 + core::round32(core::fract(ogl_ver) * 10.0f));
	if (Version >= 200)
		os::Printer::log("OpenGL driver version is 2.0 or newer.", ELL_INFORMATION);
	else
		os::Printer::log("OpenGL driver version is older than 2.0.", ELL_WARNING);

	{
		const char *t = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
		size_t len = 0;
		c8 *str = 0;
		if (t) {
			len = strlen(t);
			str = new c8[len + 1];
		}
		c8 *p = str;

		for (size_t i = 0; i < len; ++i) {
			str[i] = static_cast<char>(t[i]);

			if (str[i] == ' ') {
				str[i] = 0;
				for (u32 j = 0; j < IRR_OpenGL_Feature_Count; ++j) {
					if (!strcmp(OpenGLFeatureStrings[j], p)) {
						FeatureAvailable[j] = true;
						break;
					}
				}

				p = p + strlen(p) + 1;
			}
		}

		delete[] str;
	}

	TextureCompressionExtension = FeatureAvailable[IRR_ARB_texture_compression];
	StencilBuffer = stencilBuffer;

	const char *renderer = (const char *)glGetString(GL_RENDERER);
	if (renderer) {
		IsAtiRadeonX = (strncmp(renderer, "ATI RADEON X", 12) == 0) || (strncmp(renderer, "ATI MOBILITY RADEON X", 21) == 0);
	}

#define IRR_OGL_LOAD_EXTENSION(x) cmgr->getProcAddress(x)

	// get multitexturing function pointers
	pGlActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)IRR_OGL_LOAD_EXTENSION("glActiveTextureARB");
	pGlClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)IRR_OGL_LOAD_EXTENSION("glClientActiveTextureARB");

	// get fragment and vertex program function pointers
	pGlGenProgramsARB = (PFNGLGENPROGRAMSARBPROC)IRR_OGL_LOAD_EXTENSION("glGenProgramsARB");
	pGlGenProgramsNV = (PFNGLGENPROGRAMSNVPROC)IRR_OGL_LOAD_EXTENSION("glGenProgramsNV");
	pGlBindProgramARB = (PFNGLBINDPROGRAMARBPROC)IRR_OGL_LOAD_EXTENSION("glBindProgramARB");
	pGlBindProgramNV = (PFNGLBINDPROGRAMNVPROC)IRR_OGL_LOAD_EXTENSION("glBindProgramNV");
	pGlProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC)IRR_OGL_LOAD_EXTENSION("glProgramStringARB");
	pGlLoadProgramNV = (PFNGLLOADPROGRAMNVPROC)IRR_OGL_LOAD_EXTENSION("glLoadProgramNV");
	pGlDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)IRR_OGL_LOAD_EXTENSION("glDeleteProgramsARB");
	pGlDeleteProgramsNV = (PFNGLDELETEPROGRAMSNVPROC)IRR_OGL_LOAD_EXTENSION("glDeleteProgramsNV");
	pGlProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC)IRR_OGL_LOAD_EXTENSION("glProgramLocalParameter4fvARB");
	pGlCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC)IRR_OGL_LOAD_EXTENSION("glCreateShaderObjectARB");
	pGlCreateShader = (PFNGLCREATESHADERPROC)IRR_OGL_LOAD_EXTENSION("glCreateShader");
	pGlShaderSourceARB = (PFNGLSHADERSOURCEARBPROC)IRR_OGL_LOAD_EXTENSION("glShaderSourceARB");
	pGlShaderSource = (PFNGLSHADERSOURCEPROC)IRR_OGL_LOAD_EXTENSION("glShaderSource");
	pGlCompileShaderARB = (PFNGLCOMPILESHADERARBPROC)IRR_OGL_LOAD_EXTENSION("glCompileShaderARB");
	pGlCompileShader = (PFNGLCOMPILESHADERPROC)IRR_OGL_LOAD_EXTENSION("glCompileShader");
	pGlCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC)IRR_OGL_LOAD_EXTENSION("glCreateProgramObjectARB");
	pGlCreateProgram = (PFNGLCREATEPROGRAMPROC)IRR_OGL_LOAD_EXTENSION("glCreateProgram");
	pGlAttachObjectARB = (PFNGLATTACHOBJECTARBPROC)IRR_OGL_LOAD_EXTENSION("glAttachObjectARB");
	pGlAttachShader = (PFNGLATTACHSHADERPROC)IRR_OGL_LOAD_EXTENSION("glAttachShader");
	pGlLinkProgramARB = (PFNGLLINKPROGRAMARBPROC)IRR_OGL_LOAD_EXTENSION("glLinkProgramARB");
	pGlLinkProgram = (PFNGLLINKPROGRAMPROC)IRR_OGL_LOAD_EXTENSION("glLinkProgram");
	pGlUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)IRR_OGL_LOAD_EXTENSION("glUseProgramObjectARB");
	pGlUseProgram = (PFNGLUSEPROGRAMPROC)IRR_OGL_LOAD_EXTENSION("glUseProgram");
	pGlDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC)IRR_OGL_LOAD_EXTENSION("glDeleteObjectARB");
	pGlDeleteProgram = (PFNGLDELETEPROGRAMPROC)IRR_OGL_LOAD_EXTENSION("glDeleteProgram");
	pGlDeleteShader = (PFNGLDELETESHADERPROC)IRR_OGL_LOAD_EXTENSION("glDeleteShader");
	pGlGetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC)IRR_OGL_LOAD_EXTENSION("glGetAttachedShaders");
	pGlGetAttachedObjectsARB = (PFNGLGETATTACHEDOBJECTSARBPROC)IRR_OGL_LOAD_EXTENSION("glGetAttachedObjectsARB");
	pGlGetInfoLogARB = (PFNGLGETINFOLOGARBPROC)IRR_OGL_LOAD_EXTENSION("glGetInfoLogARB");
	pGlGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)IRR_OGL_LOAD_EXTENSION("glGetShaderInfoLog");
	pGlGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)IRR_OGL_LOAD_EXTENSION("glGetProgramInfoLog");
	pGlGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC)IRR_OGL_LOAD_EXTENSION("glGetObjectParameterivARB");
	pGlGetShaderiv = (PFNGLGETSHADERIVPROC)IRR_OGL_LOAD_EXTENSION("glGetShaderiv");
	pGlGetProgramiv = (PFNGLGETPROGRAMIVPROC)IRR_OGL_LOAD_EXTENSION("glGetProgramiv");
	pGlGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC)IRR_OGL_LOAD_EXTENSION("glGetUniformLocationARB");
	pGlGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)IRR_OGL_LOAD_EXTENSION("glGetUniformLocation");
	pGlUniform1fvARB = (PFNGLUNIFORM1FVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniform1fvARB");
	pGlUniform2fvARB = (PFNGLUNIFORM2FVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniform2fvARB");
	pGlUniform3fvARB = (PFNGLUNIFORM3FVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniform3fvARB");
	pGlUniform4fvARB = (PFNGLUNIFORM4FVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniform4fvARB");
	pGlUniform1ivARB = (PFNGLUNIFORM1IVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniform1ivARB");
	pGlUniform2ivARB = (PFNGLUNIFORM2IVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniform2ivARB");
	pGlUniform3ivARB = (PFNGLUNIFORM3IVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniform3ivARB");
	pGlUniform4ivARB = (PFNGLUNIFORM4IVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniform4ivARB");
	pGlUniform1uiv = (PFNGLUNIFORM1UIVPROC)IRR_OGL_LOAD_EXTENSION("glUniform1uiv");
	pGlUniform2uiv = (PFNGLUNIFORM2UIVPROC)IRR_OGL_LOAD_EXTENSION("glUniform2uiv");
	pGlUniform3uiv = (PFNGLUNIFORM3UIVPROC)IRR_OGL_LOAD_EXTENSION("glUniform3uiv");
	pGlUniform4uiv = (PFNGLUNIFORM4UIVPROC)IRR_OGL_LOAD_EXTENSION("glUniform4uiv");
	pGlUniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix2fvARB");
	pGlUniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix2x3fv");
	pGlUniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix2x4fv");
	pGlUniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix3x2fv");
	pGlUniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix3fvARB");
	pGlUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix3x4fv");
	pGlUniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix4x2fv");
	pGlUniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix4x3fv");
	pGlUniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC)IRR_OGL_LOAD_EXTENSION("glUniformMatrix4fvARB");
	pGlGetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC)IRR_OGL_LOAD_EXTENSION("glGetActiveUniformARB");
	pGlGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)IRR_OGL_LOAD_EXTENSION("glGetActiveUniform");

	// get point parameter extension
	pGlPointParameterfARB = (PFNGLPOINTPARAMETERFARBPROC)IRR_OGL_LOAD_EXTENSION("glPointParameterfARB");
	pGlPointParameterfvARB = (PFNGLPOINTPARAMETERFVARBPROC)IRR_OGL_LOAD_EXTENSION("glPointParameterfvARB");

	// get stencil extension
	pGlStencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC)IRR_OGL_LOAD_EXTENSION("glStencilFuncSeparate");
	pGlStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC)IRR_OGL_LOAD_EXTENSION("glStencilOpSeparate");
	pGlStencilFuncSeparateATI = (PFNGLSTENCILFUNCSEPARATEATIPROC)IRR_OGL_LOAD_EXTENSION("glStencilFuncSeparateATI");
	pGlStencilOpSeparateATI = (PFNGLSTENCILOPSEPARATEATIPROC)IRR_OGL_LOAD_EXTENSION("glStencilOpSeparateATI");

	// compressed textures
	pGlCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)IRR_OGL_LOAD_EXTENSION("glCompressedTexImage2D");
	pGlCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)IRR_OGL_LOAD_EXTENSION("glCompressedTexSubImage2D");

	// ARB FrameBufferObjects
	pGlBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)IRR_OGL_LOAD_EXTENSION("glBindFramebuffer");
	pGlDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)IRR_OGL_LOAD_EXTENSION("glDeleteFramebuffers");
	pGlGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)IRR_OGL_LOAD_EXTENSION("glGenFramebuffers");
	pGlCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)IRR_OGL_LOAD_EXTENSION("glCheckFramebufferStatus");
	pGlFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)IRR_OGL_LOAD_EXTENSION("glFramebufferTexture2D");
	pGlBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)IRR_OGL_LOAD_EXTENSION("glBindRenderbuffer");
	pGlDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)IRR_OGL_LOAD_EXTENSION("glDeleteRenderbuffers");
	pGlGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)IRR_OGL_LOAD_EXTENSION("glGenRenderbuffers");
	pGlRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)IRR_OGL_LOAD_EXTENSION("glRenderbufferStorage");
	pGlFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)IRR_OGL_LOAD_EXTENSION("glFramebufferRenderbuffer");
	pGlGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)IRR_OGL_LOAD_EXTENSION("glGenerateMipmap");

	// EXT FrameBufferObjects
	pGlBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)IRR_OGL_LOAD_EXTENSION("glBindFramebufferEXT");
	pGlDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC)IRR_OGL_LOAD_EXTENSION("glDeleteFramebuffersEXT");
	pGlGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)IRR_OGL_LOAD_EXTENSION("glGenFramebuffersEXT");
	pGlCheckFramebufferStatusEXT = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)IRR_OGL_LOAD_EXTENSION("glCheckFramebufferStatusEXT");
	pGlFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)IRR_OGL_LOAD_EXTENSION("glFramebufferTexture2DEXT");
	pGlBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)IRR_OGL_LOAD_EXTENSION("glBindRenderbufferEXT");
	pGlDeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC)IRR_OGL_LOAD_EXTENSION("glDeleteRenderbuffersEXT");
	pGlGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)IRR_OGL_LOAD_EXTENSION("glGenRenderbuffersEXT");
	pGlRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)IRR_OGL_LOAD_EXTENSION("glRenderbufferStorageEXT");
	pGlFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)IRR_OGL_LOAD_EXTENSION("glFramebufferRenderbufferEXT");
	pGlGenerateMipmapEXT = (PFNGLGENERATEMIPMAPEXTPROC)IRR_OGL_LOAD_EXTENSION("glGenerateMipmapEXT");
	pGlDrawBuffersARB = (PFNGLDRAWBUFFERSARBPROC)IRR_OGL_LOAD_EXTENSION("glDrawBuffersARB");
	pGlDrawBuffersATI = (PFNGLDRAWBUFFERSATIPROC)IRR_OGL_LOAD_EXTENSION("glDrawBuffersATI");

	// get vertex buffer extension
	pGlGenBuffersARB = (PFNGLGENBUFFERSARBPROC)IRR_OGL_LOAD_EXTENSION("glGenBuffersARB");
	pGlBindBufferARB = (PFNGLBINDBUFFERARBPROC)IRR_OGL_LOAD_EXTENSION("glBindBufferARB");
	pGlBufferDataARB = (PFNGLBUFFERDATAARBPROC)IRR_OGL_LOAD_EXTENSION("glBufferDataARB");
	pGlDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)IRR_OGL_LOAD_EXTENSION("glDeleteBuffersARB");
	pGlBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)IRR_OGL_LOAD_EXTENSION("glBufferSubDataARB");
	pGlGetBufferSubDataARB = (PFNGLGETBUFFERSUBDATAARBPROC)IRR_OGL_LOAD_EXTENSION("glGetBufferSubDataARB");
	pGlMapBufferARB = (PFNGLMAPBUFFERARBPROC)IRR_OGL_LOAD_EXTENSION("glMapBufferARB");
	pGlUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)IRR_OGL_LOAD_EXTENSION("glUnmapBufferARB");
	pGlIsBufferARB = (PFNGLISBUFFERARBPROC)IRR_OGL_LOAD_EXTENSION("glIsBufferARB");
	pGlGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC)IRR_OGL_LOAD_EXTENSION("glGetBufferParameterivARB");
	pGlGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC)IRR_OGL_LOAD_EXTENSION("glGetBufferPointervARB");
	pGlProvokingVertexARB = (PFNGLPROVOKINGVERTEXPROC)IRR_OGL_LOAD_EXTENSION("glProvokingVertex");
	pGlProvokingVertexEXT = (PFNGLPROVOKINGVERTEXEXTPROC)IRR_OGL_LOAD_EXTENSION("glProvokingVertexEXT");
	pGlProgramParameteriARB = (PFNGLPROGRAMPARAMETERIARBPROC)IRR_OGL_LOAD_EXTENSION("glProgramParameteriARB");
	pGlProgramParameteriEXT = (PFNGLPROGRAMPARAMETERIEXTPROC)IRR_OGL_LOAD_EXTENSION("glProgramParameteriEXT");

	// occlusion query
	pGlGenQueriesARB = (PFNGLGENQUERIESARBPROC)IRR_OGL_LOAD_EXTENSION("glGenQueriesARB");
	pGlDeleteQueriesARB = (PFNGLDELETEQUERIESARBPROC)IRR_OGL_LOAD_EXTENSION("glDeleteQueriesARB");
	pGlIsQueryARB = (PFNGLISQUERYARBPROC)IRR_OGL_LOAD_EXTENSION("glIsQueryARB");
	pGlBeginQueryARB = (PFNGLBEGINQUERYARBPROC)IRR_OGL_LOAD_EXTENSION("glBeginQueryARB");
	pGlEndQueryARB = (PFNGLENDQUERYARBPROC)IRR_OGL_LOAD_EXTENSION("glEndQueryARB");
	pGlGetQueryivARB = (PFNGLGETQUERYIVARBPROC)IRR_OGL_LOAD_EXTENSION("glGetQueryivARB");
	pGlGetQueryObjectivARB = (PFNGLGETQUERYOBJECTIVARBPROC)IRR_OGL_LOAD_EXTENSION("glGetQueryObjectivARB");
	pGlGetQueryObjectuivARB = (PFNGLGETQUERYOBJECTUIVARBPROC)IRR_OGL_LOAD_EXTENSION("glGetQueryObjectuivARB");
	pGlGenOcclusionQueriesNV = (PFNGLGENOCCLUSIONQUERIESNVPROC)IRR_OGL_LOAD_EXTENSION("glGenOcclusionQueriesNV");
	pGlDeleteOcclusionQueriesNV = (PFNGLDELETEOCCLUSIONQUERIESNVPROC)IRR_OGL_LOAD_EXTENSION("glDeleteOcclusionQueriesNV");
	pGlIsOcclusionQueryNV = (PFNGLISOCCLUSIONQUERYNVPROC)IRR_OGL_LOAD_EXTENSION("glIsOcclusionQueryNV");
	pGlBeginOcclusionQueryNV = (PFNGLBEGINOCCLUSIONQUERYNVPROC)IRR_OGL_LOAD_EXTENSION("glBeginOcclusionQueryNV");
	pGlEndOcclusionQueryNV = (PFNGLENDOCCLUSIONQUERYNVPROC)IRR_OGL_LOAD_EXTENSION("glEndOcclusionQueryNV");
	pGlGetOcclusionQueryivNV = (PFNGLGETOCCLUSIONQUERYIVNVPROC)IRR_OGL_LOAD_EXTENSION("glGetOcclusionQueryivNV");
	pGlGetOcclusionQueryuivNV = (PFNGLGETOCCLUSIONQUERYUIVNVPROC)IRR_OGL_LOAD_EXTENSION("glGetOcclusionQueryuivNV");

	// blend
	pGlBlendFuncSeparateEXT = (PFNGLBLENDFUNCSEPARATEEXTPROC)IRR_OGL_LOAD_EXTENSION("glBlendFuncSeparateEXT");
	pGlBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)IRR_OGL_LOAD_EXTENSION("glBlendFuncSeparate");
	pGlBlendEquationEXT = (PFNGLBLENDEQUATIONEXTPROC)IRR_OGL_LOAD_EXTENSION("glBlendEquationEXT");
	pGlBlendEquation = (PFNGLBLENDEQUATIONPROC)IRR_OGL_LOAD_EXTENSION("glBlendEquation");
	pGlBlendEquationSeparateEXT = (PFNGLBLENDEQUATIONSEPARATEEXTPROC)IRR_OGL_LOAD_EXTENSION("glBlendEquationSeparateEXT");
	pGlBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)IRR_OGL_LOAD_EXTENSION("glBlendEquationSeparate");

	// indexed
	pGlEnableIndexedEXT = (PFNGLENABLEINDEXEDEXTPROC)IRR_OGL_LOAD_EXTENSION("glEnableIndexedEXT");
	pGlDisableIndexedEXT = (PFNGLDISABLEINDEXEDEXTPROC)IRR_OGL_LOAD_EXTENSION("glDisableIndexedEXT");
	pGlColorMaskIndexedEXT = (PFNGLCOLORMASKINDEXEDEXTPROC)IRR_OGL_LOAD_EXTENSION("glColorMaskIndexedEXT");
	pGlBlendFuncIndexedAMD = (PFNGLBLENDFUNCINDEXEDAMDPROC)IRR_OGL_LOAD_EXTENSION("glBlendFuncIndexedAMD");
	pGlBlendFunciARB = (PFNGLBLENDFUNCIPROC)IRR_OGL_LOAD_EXTENSION("glBlendFunciARB");
	pGlBlendFuncSeparateIndexedAMD = (PFNGLBLENDFUNCSEPARATEINDEXEDAMDPROC)IRR_OGL_LOAD_EXTENSION("glBlendFuncSeparateIndexedAMD");
	pGlBlendFuncSeparateiARB = (PFNGLBLENDFUNCSEPARATEIPROC)IRR_OGL_LOAD_EXTENSION("glBlendFuncSeparateiARB");
	pGlBlendEquationIndexedAMD = (PFNGLBLENDEQUATIONINDEXEDAMDPROC)IRR_OGL_LOAD_EXTENSION("glBlendEquationIndexedAMD");
	pGlBlendEquationiARB = (PFNGLBLENDEQUATIONIPROC)IRR_OGL_LOAD_EXTENSION("glBlendEquationiARB");
	pGlBlendEquationSeparateIndexedAMD = (PFNGLBLENDEQUATIONSEPARATEINDEXEDAMDPROC)IRR_OGL_LOAD_EXTENSION("glBlendEquationSeparateIndexedAMD");
	pGlBlendEquationSeparateiARB = (PFNGLBLENDEQUATIONSEPARATEIPROC)IRR_OGL_LOAD_EXTENSION("glBlendEquationSeparateiARB");

	pGlTextureStorage2D = (PFNGLTEXTURESTORAGE2DPROC)IRR_OGL_LOAD_EXTENSION("glTextureStorage2D");
	pGlTextureStorage3D = (PFNGLTEXTURESTORAGE3DPROC)IRR_OGL_LOAD_EXTENSION("glTextureStorage3D");
	pGlTextureSubImage2D = (PFNGLTEXTURESUBIMAGE2DPROC)IRR_OGL_LOAD_EXTENSION("glTextureSubImage2D");
	pGlGetTextureImage = (PFNGLGETTEXTUREIMAGEPROC)IRR_OGL_LOAD_EXTENSION("glGetTextureImage");
	pGlNamedFramebufferTexture = (PFNGLNAMEDFRAMEBUFFERTEXTUREPROC)IRR_OGL_LOAD_EXTENSION("glNamedFramebufferTexture");
	pGlTextureParameteri = (PFNGLTEXTUREPARAMETERIPROC)IRR_OGL_LOAD_EXTENSION("glTextureParameteri");
	pGlTextureParameterf = (PFNGLTEXTUREPARAMETERFPROC)IRR_OGL_LOAD_EXTENSION("glTextureParameterf");
	pGlTextureParameteriv = (PFNGLTEXTUREPARAMETERIVPROC)IRR_OGL_LOAD_EXTENSION("glTextureParameteriv");
	pGlTextureParameterfv = (PFNGLTEXTUREPARAMETERFVPROC)IRR_OGL_LOAD_EXTENSION("glTextureParameterfv");

	pGlCreateTextures = (PFNGLCREATETEXTURESPROC)IRR_OGL_LOAD_EXTENSION("glCreateTextures");
	pGlCreateFramebuffers = (PFNGLCREATEFRAMEBUFFERSPROC)IRR_OGL_LOAD_EXTENSION("glCreateFramebuffers");
	pGlBindTextures = (PFNGLBINDTEXTURESPROC)IRR_OGL_LOAD_EXTENSION("glBindTextures");
	pGlGenerateTextureMipmap = (PFNGLGENERATETEXTUREMIPMAPPROC)IRR_OGL_LOAD_EXTENSION("glGenerateTextureMipmap");
	//==============================
	pGlTextureStorage2DEXT = (PFNGLTEXTURESTORAGE2DEXTPROC)IRR_OGL_LOAD_EXTENSION("glTextureStorage2DEXT");
	pGlTexStorage2D = (PFNGLTEXSTORAGE2DPROC)IRR_OGL_LOAD_EXTENSION("glTexStorage2D");
	pGlTextureStorage3DEXT = (PFNGLTEXTURESTORAGE3DEXTPROC)IRR_OGL_LOAD_EXTENSION("glTextureStorage3DEXT");
	pGlTexStorage3D = (PFNGLTEXSTORAGE3DPROC)IRR_OGL_LOAD_EXTENSION("glTexStorage3D");
	pGlTextureSubImage2DEXT = (PFNGLTEXTURESUBIMAGE2DEXTPROC)IRR_OGL_LOAD_EXTENSION("glTextureSubImage2DEXT");
	pGlGetTextureImageEXT = (PFNGLGETTEXTUREIMAGEEXTPROC)IRR_OGL_LOAD_EXTENSION("glGetTextureImageEXT");
	pGlNamedFramebufferTextureEXT = (PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC)IRR_OGL_LOAD_EXTENSION("glNamedFramebufferTextureEXT");
	pGlFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)IRR_OGL_LOAD_EXTENSION("glFramebufferTexture");
	pGlActiveTexture = (PFNGLACTIVETEXTUREPROC)IRR_OGL_LOAD_EXTENSION("glActiveTexture");
	pGlGenerateTextureMipmapEXT = (PFNGLGENERATETEXTUREMIPMAPEXTPROC)IRR_OGL_LOAD_EXTENSION("glGenerateTextureMipmapEXT");

// get vsync extension
#if defined(WGL_EXT_swap_control) && !defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	pWglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)IRR_OGL_LOAD_EXTENSION("wglSwapIntervalEXT");
#endif
#if defined(GLX_SGI_swap_control) && !defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	pGlxSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)IRR_OGL_LOAD_EXTENSION("glXSwapIntervalSGI");
#endif
#if defined(GLX_EXT_swap_control) && !defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	pGlxSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)IRR_OGL_LOAD_EXTENSION("glXSwapIntervalEXT");
#endif
#if defined(GLX_MESA_swap_control) && !defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	pGlxSwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC)IRR_OGL_LOAD_EXTENSION("glXSwapIntervalMESA");
#endif

	GLint num = 0;
	// set some properties
#if defined(GL_ARB_multitexture) || defined(GL_VERSION_1_3)
	if (Version > 102 || FeatureAvailable[IRR_ARB_multitexture]) {
#if defined(GL_MAX_TEXTURE_UNITS)
		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &num);
#elif defined(GL_MAX_TEXTURE_UNITS_ARB)
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &num);
#endif
		Feature.MaxTextureUnits = static_cast<u8>(num); // MULTITEXTURING (fixed function pipeline texture units)
	}
#endif
#if defined(GL_ARB_vertex_shader) || defined(GL_VERSION_2_0)
	if (Version >= 200 || FeatureAvailable[IRR_ARB_vertex_shader]) {
		num = 0;
#if defined(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &num);
#elif defined(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB)
		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB, &num);
#endif
		Feature.MaxTextureUnits = core::max_(Feature.MaxTextureUnits, static_cast<u8>(num));
	}
#endif
#ifdef GL_EXT_texture_filter_anisotropic
	if (FeatureAvailable[IRR_EXT_texture_filter_anisotropic]) {
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &num);
		MaxAnisotropy = static_cast<u8>(num);
	}
#endif
#ifdef GL_VERSION_1_2
	if (Version > 101) {
		glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &num);
		MaxIndices = num;
	}
#endif
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &num);
	MaxTextureSize = static_cast<u32>(num);
	if (queryFeature(EVDF_GEOMETRY_SHADER)) {
#if defined(GL_ARB_geometry_shader4) || defined(GL_EXT_geometry_shader4) || defined(GL_NV_geometry_shader4)
		glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT, &num);
		MaxGeometryVerticesOut = static_cast<u32>(num);
#elif defined(GL_NV_geometry_program4)
		extGlGetProgramiv(GEOMETRY_PROGRAM_NV, GL_MAX_PROGRAM_OUTPUT_VERTICES_NV, &num);
		MaxGeometryVerticesOut = static_cast<u32>(num);
#endif
	}
#ifdef GL_EXT_texture_lod_bias
	if (FeatureAvailable[IRR_EXT_texture_lod_bias])
		glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS_EXT, &MaxTextureLODBias);
#endif
	glGetIntegerv(GL_AUX_BUFFERS, &num);
	MaxAuxBuffers = static_cast<u8>(num);
#ifdef GL_ARB_draw_buffers
	if (FeatureAvailable[IRR_ARB_draw_buffers]) {
		glGetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, &num);
		Feature.MultipleRenderTarget = static_cast<u8>(num);
	}
#endif
#if defined(GL_ATI_draw_buffers)
#ifdef GL_ARB_draw_buffers
	else
#endif
			if (FeatureAvailable[IRR_ATI_draw_buffers]) {
		glGetIntegerv(GL_MAX_DRAW_BUFFERS_ATI, &num);
		Feature.MultipleRenderTarget = static_cast<u8>(num);
	}
#endif
#ifdef GL_ARB_framebuffer_object
	if (FeatureAvailable[IRR_ARB_framebuffer_object]) {
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &num);
		Feature.ColorAttachment = static_cast<u8>(num);
	}
#endif
#if defined(GL_EXT_framebuffer_object)
#ifdef GL_ARB_framebuffer_object
	else
#endif
			if (FeatureAvailable[IRR_EXT_framebuffer_object]) {
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &num);
		Feature.ColorAttachment = static_cast<u8>(num);
	}
#endif

	glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, DimAliasedLine);
	glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, DimAliasedPoint);
	glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE, DimSmoothedLine);
	glGetFloatv(GL_SMOOTH_POINT_SIZE_RANGE, DimSmoothedPoint);
#if defined(GL_ARB_shading_language_100) || defined(GL_VERSION_2_0)
	if (FeatureAvailable[IRR_ARB_shading_language_100] || Version >= 200) {
		glGetError(); // clean error buffer
#ifdef GL_SHADING_LANGUAGE_VERSION
		const GLubyte *shaderVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
#else
		const GLubyte *shaderVersion = glGetString(GL_SHADING_LANGUAGE_VERSION_ARB);
#endif
		if (glGetError() == GL_INVALID_ENUM)
			ShaderLanguageVersion = 100;
		else {
			const f32 sl_ver = core::fast_atof(reinterpret_cast<const c8 *>(shaderVersion));
			ShaderLanguageVersion = static_cast<u16>(core::floor32(sl_ver) * 100 + core::round32(core::fract(sl_ver) * 10.0f));
		}
	}
#endif

	if (!pGlActiveTextureARB || !pGlClientActiveTextureARB) {
		Feature.MaxTextureUnits = 1;
		os::Printer::log("Failed to load OpenGL's multitexture extension, proceeding without.", ELL_WARNING);
	} else
		Feature.MaxTextureUnits = core::min_(Feature.MaxTextureUnits, static_cast<u8>(MATERIAL_MAX_TEXTURES));

#ifdef GL_ARB_occlusion_query
	if (FeatureAvailable[IRR_ARB_occlusion_query]) {
		extGlGetQueryiv(GL_SAMPLES_PASSED_ARB, GL_QUERY_COUNTER_BITS_ARB,
				&num);
		OcclusionQuerySupport = (num > 0);
	} else
#endif
#ifdef GL_NV_occlusion_query
			if (FeatureAvailable[IRR_NV_occlusion_query]) {
		glGetIntegerv(GL_PIXEL_COUNTER_BITS_NV, &num);
		OcclusionQuerySupport = (num > 0);
	} else
#endif
		OcclusionQuerySupport = false;

	Feature.BlendOperation = (Version >= 104) ||
							 FeatureAvailable[IRR_EXT_blend_minmax] ||
							 FeatureAvailable[IRR_EXT_blend_subtract] ||
							 FeatureAvailable[IRR_EXT_blend_logic_op];

#ifdef _DEBUG
	if (FeatureAvailable[IRR_NVX_gpu_memory_info]) {
		// undocumented flags, so use the RAW values
		GLint val;
		glGetIntegerv(0x9047, &val);
		os::Printer::log("Dedicated video memory (kB)", core::stringc(val));
		glGetIntegerv(0x9048, &val);
		os::Printer::log("Total video memory (kB)", core::stringc(val));
		glGetIntegerv(0x9049, &val);
		os::Printer::log("Available video memory (kB)", core::stringc(val));
	}
#ifdef GL_ATI_meminfo
	if (FeatureAvailable[IRR_ATI_meminfo]) {
		GLint val[4];
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, val);
		os::Printer::log("Free texture memory (kB)", core::stringc(val[0]));
		glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, val);
		os::Printer::log("Free VBO memory (kB)", core::stringc(val[0]));
		glGetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, val);
		os::Printer::log("Free render buffer memory (kB)", core::stringc(val[0]));
	}
#endif

	if (queryFeature(EVDF_TEXTURE_CUBEMAP_SEAMLESS))
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

#endif
}

const COpenGLCoreFeature &COpenGLExtensionHandler::getFeature() const
{
	return Feature;
}

bool COpenGLExtensionHandler::queryFeature(E_VIDEO_DRIVER_FEATURE feature) const
{
	switch (feature) {
	case EVDF_RENDER_TO_TARGET:
		return true;
	case EVDF_HARDWARE_TL:
		return true; // we cannot tell other things
	case EVDF_MULTITEXTURE:
		return Feature.MaxTextureUnits > 1;
	case EVDF_BILINEAR_FILTER:
		return true;
	case EVDF_MIP_MAP:
		return true;
	case EVDF_MIP_MAP_AUTO_UPDATE:
		return !IsAtiRadeonX && (FeatureAvailable[IRR_SGIS_generate_mipmap] || FeatureAvailable[IRR_EXT_framebuffer_object] || FeatureAvailable[IRR_ARB_framebuffer_object]);
	case EVDF_STENCIL_BUFFER:
		return StencilBuffer;
	case EVDF_VERTEX_SHADER_1_1:
	case EVDF_ARB_VERTEX_PROGRAM_1:
		return FeatureAvailable[IRR_ARB_vertex_program] || FeatureAvailable[IRR_NV_vertex_program1_1];
	case EVDF_PIXEL_SHADER_1_1:
	case EVDF_PIXEL_SHADER_1_2:
	case EVDF_ARB_FRAGMENT_PROGRAM_1:
		return FeatureAvailable[IRR_ARB_fragment_program] || FeatureAvailable[IRR_NV_fragment_program];
	case EVDF_PIXEL_SHADER_2_0:
	case EVDF_VERTEX_SHADER_2_0:
	case EVDF_ARB_GLSL:
		return (FeatureAvailable[IRR_ARB_shading_language_100] || Version >= 200);
	case EVDF_TEXTURE_NSQUARE:
		return true; // non-square is always supported
	case EVDF_TEXTURE_NPOT:
		// Some ATI cards seem to have only SW support in OpenGL 2.0
		// drivers if the extension is not exposed, so we skip this
		// extra test for now!
		// return (FeatureAvailable[IRR_ARB_texture_non_power_of_two]||Version>=200);
		return (FeatureAvailable[IRR_ARB_texture_non_power_of_two]);
	case EVDF_FRAMEBUFFER_OBJECT:
		return FeatureAvailable[IRR_EXT_framebuffer_object] || FeatureAvailable[IRR_ARB_framebuffer_object];
	case EVDF_VERTEX_BUFFER_OBJECT:
		return FeatureAvailable[IRR_ARB_vertex_buffer_object];
	case EVDF_COLOR_MASK:
		return true;
	case EVDF_ALPHA_TO_COVERAGE:
		return FeatureAvailable[IRR_ARB_multisample];
	case EVDF_GEOMETRY_SHADER:
		return FeatureAvailable[IRR_ARB_geometry_shader4] || FeatureAvailable[IRR_EXT_geometry_shader4] || FeatureAvailable[IRR_NV_geometry_program4] || FeatureAvailable[IRR_NV_geometry_shader4];
	case EVDF_MULTIPLE_RENDER_TARGETS:
		return FeatureAvailable[IRR_ARB_draw_buffers] || FeatureAvailable[IRR_ATI_draw_buffers];
	case EVDF_MRT_BLEND:
	case EVDF_MRT_COLOR_MASK:
		return FeatureAvailable[IRR_EXT_draw_buffers2];
	case EVDF_MRT_BLEND_FUNC:
		return FeatureAvailable[IRR_ARB_draw_buffers_blend] || FeatureAvailable[IRR_AMD_draw_buffers_blend];
	case EVDF_OCCLUSION_QUERY:
		return FeatureAvailable[IRR_ARB_occlusion_query] && OcclusionQuerySupport;
	case EVDF_POLYGON_OFFSET:
		// both features supported with OpenGL 1.1
		return Version >= 101;
	case EVDF_BLEND_OPERATIONS:
		return Feature.BlendOperation;
	case EVDF_BLEND_SEPARATE:
		return (Version >= 104) || FeatureAvailable[IRR_EXT_blend_func_separate];
	case EVDF_TEXTURE_MATRIX:
		return true;
	case EVDF_TEXTURE_CUBEMAP:
		return (Version >= 103) || FeatureAvailable[IRR_ARB_texture_cube_map] || FeatureAvailable[IRR_EXT_texture_cube_map];
	case EVDF_TEXTURE_CUBEMAP_SEAMLESS:
		return FeatureAvailable[IRR_ARB_seamless_cube_map];
	case EVDF_DEPTH_CLAMP:
		return FeatureAvailable[IRR_NV_depth_clamp] || FeatureAvailable[IRR_ARB_depth_clamp];
	case EVDF_TEXTURE_MULTISAMPLE:
		return (Version >= 302) || FeatureAvailable[IRR_ARB_texture_multisample];

	default:
		return false;
	};
}

}

#endif
