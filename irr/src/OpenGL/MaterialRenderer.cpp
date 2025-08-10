// Copyright (C) 2014 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "MaterialRenderer.h"

#include "EVertexAttributes.h"
#include "IGPUProgrammingServices.h"
#include "IShaderConstantSetCallBack.h"
#include "IVideoDriver.h"
#include "os.h"

#include "Driver.h"

#include "COpenGLCoreTexture.h"
#include "COpenGLCoreCacheHandler.h"

namespace video
{

COpenGL3MaterialRenderer::COpenGL3MaterialRenderer(COpenGL3DriverBase *driver,
		s32 &outMaterialTypeNr,
		const c8 *vertexShaderProgram,
		const c8 *pixelShaderProgram,
		const c8 *debugName,
		IShaderConstantSetCallBack *callback,
		E_MATERIAL_TYPE baseMaterial,
		s32 userData) :
		Driver(driver),
		CallBack(callback), Alpha(false), Blending(false), Program(0), UserData(userData)
{
	switch (baseMaterial) {
	case EMT_TRANSPARENT_VERTEX_ALPHA:
	case EMT_TRANSPARENT_ALPHA_CHANNEL:
		Alpha = true;
		break;
	case EMT_ONETEXTURE_BLEND:
		Blending = true;
		break;
	default:
		break;
	}

	if (CallBack)
		CallBack->grab();

	init(outMaterialTypeNr, vertexShaderProgram, pixelShaderProgram, debugName);
}

COpenGL3MaterialRenderer::COpenGL3MaterialRenderer(COpenGL3DriverBase *driver,
		IShaderConstantSetCallBack *callback,
		E_MATERIAL_TYPE baseMaterial, s32 userData) :
		Driver(driver),
		CallBack(callback), Alpha(false), Blending(false), Program(0), UserData(userData)
{
	switch (baseMaterial) {
	case EMT_TRANSPARENT_VERTEX_ALPHA:
	case EMT_TRANSPARENT_ALPHA_CHANNEL:
		Alpha = true;
		break;
	case EMT_ONETEXTURE_BLEND:
		Blending = true;
		break;
	default:
		break;
	}

	if (CallBack)
		CallBack->grab();
}

COpenGL3MaterialRenderer::~COpenGL3MaterialRenderer()
{
	if (CallBack)
		CallBack->drop();

	if (Program) {
		GLuint shaders[8];
		GLint count;
		GL.GetAttachedShaders(Program, 8, &count, shaders);

		count = core::min_(count, 8);
		for (GLint i = 0; i < count; ++i)
			GL.DeleteShader(shaders[i]);
		GL.DeleteProgram(Program);
		Program = 0;
	}

	UniformInfo.clear();
}

GLuint COpenGL3MaterialRenderer::getProgram() const
{
	return Program;
}

void COpenGL3MaterialRenderer::init(s32 &outMaterialTypeNr,
		const c8 *vertexShaderProgram,
		const c8 *pixelShaderProgram,
		const c8 *debugName,
		bool addMaterial)
{
	outMaterialTypeNr = -1;

	Program = GL.CreateProgram();

	if (!Program)
		return;

	if (vertexShaderProgram)
		if (!createShader(GL_VERTEX_SHADER, vertexShaderProgram))
			return;

	if (pixelShaderProgram)
		if (!createShader(GL_FRAGMENT_SHADER, pixelShaderProgram))
			return;

	for (size_t i = 0; i < EVA_COUNT; ++i)
		GL.BindAttribLocation(Program, i, sBuiltInVertexAttributeNames[i]);

	if (!linkProgram())
		return;

	if (debugName)
		Driver->irrGlObjectLabel(GL_PROGRAM, Program, debugName);

	if (addMaterial)
		outMaterialTypeNr = Driver->addMaterialRenderer(this);
}

bool COpenGL3MaterialRenderer::OnRender(IMaterialRendererServices *service, E_VERTEX_TYPE vtxtype)
{
	if (CallBack && Program)
		CallBack->OnSetConstants(this, UserData);

	return true;
}

void COpenGL3MaterialRenderer::OnSetMaterial(const video::SMaterial &material,
		const video::SMaterial &lastMaterial,
		bool resetAllRenderstates,
		video::IMaterialRendererServices *services)
{
	COpenGL3CacheHandler *cacheHandler = Driver->getCacheHandler();

	cacheHandler->setProgram(Program);

	Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);

	if (Alpha) {
		cacheHandler->setBlend(true);
		cacheHandler->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else if (Blending) {
		E_BLEND_FACTOR srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact;
		E_MODULATE_FUNC modulate;
		u32 alphaSource;
		unpack_textureBlendFuncSeparate(srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulate, alphaSource, material.MaterialTypeParam);

		cacheHandler->setBlendFuncSeparate(Driver->getGLBlend(srcRGBFact), Driver->getGLBlend(dstRGBFact),
				Driver->getGLBlend(srcAlphaFact), Driver->getGLBlend(dstAlphaFact));

		cacheHandler->setBlend(true);
	}

	if (CallBack)
		CallBack->OnSetMaterial(material);
}

void COpenGL3MaterialRenderer::OnUnsetMaterial()
{
}

bool COpenGL3MaterialRenderer::isTransparent() const
{
	return (Alpha || Blending);
}

s32 COpenGL3MaterialRenderer::getRenderCapability() const
{
	return 0;
}

bool COpenGL3MaterialRenderer::createShader(GLenum shaderType, const char *shader)
{
	if (Program) {
		GLuint shaderHandle = GL.CreateShader(shaderType);
		GL.ShaderSource(shaderHandle, 1, &shader, NULL);
		GL.CompileShader(shaderHandle);

		GLint status = 0;

		GL.GetShaderiv(shaderHandle, GL_COMPILE_STATUS, &status);

		if (status != GL_TRUE) {
			os::Printer::log("GLSL shader failed to compile", ELL_ERROR);

			GLint maxLength = 0;
			GLint length;

			GL.GetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH,
					&maxLength);

			if (maxLength) {
				GLchar *infoLog = new GLchar[maxLength];
				GL.GetShaderInfoLog(shaderHandle, maxLength, &length, infoLog);
				os::Printer::log(reinterpret_cast<const c8 *>(infoLog), ELL_ERROR);
				delete[] infoLog;
			}

			return false;
		}

		GL.AttachShader(Program, shaderHandle);
	}

	return true;
}

bool COpenGL3MaterialRenderer::linkProgram()
{
	if (Program) {
		GL.LinkProgram(Program);

		GLint status = 0;

		GL.GetProgramiv(Program, GL_LINK_STATUS, &status);

		if (!status) {
			os::Printer::log("GLSL shader program failed to link", ELL_ERROR);

			GLint maxLength = 0;
			GLsizei length;

			GL.GetProgramiv(Program, GL_INFO_LOG_LENGTH, &maxLength);

			if (maxLength) {
				GLchar *infoLog = new GLchar[maxLength];
				GL.GetProgramInfoLog(Program, maxLength, &length, infoLog);
				os::Printer::log(reinterpret_cast<const c8 *>(infoLog), ELL_ERROR);
				delete[] infoLog;
			}

			return false;
		}

		GLint num = 0;

		GL.GetProgramiv(Program, GL_ACTIVE_UNIFORMS, &num);

		if (num == 0)
			return true;

		GLint maxlen = 0;

		GL.GetProgramiv(Program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxlen);

		if (maxlen == 0) {
			os::Printer::log("GLSL: failed to retrieve uniform information", ELL_ERROR);
			return false;
		}

		// seems that some implementations use an extra null terminator.
		++maxlen;
		std::vector<c8> buf(maxlen);

		UniformInfo.clear();
		UniformInfo.reserve(num);

		for (GLint i = 0; i < num; ++i) {
			SUniformInfo ui;
			memset(buf.data(), 0, buf.size());

			GLint size;
			GL.GetActiveUniform(Program, i, maxlen, 0, &size, &ui.type, reinterpret_cast<GLchar *>(buf.data()));

			// array support, workaround for some bugged drivers.
			for (s32 i = 0; i < maxlen; ++i) {
				if (buf[i] == '[' || buf[i] == '\0')
					break;

				ui.name += buf[i];
			}

			ui.location = GL.GetUniformLocation(Program, buf.data());

			UniformInfo.push_back(std::move(ui));
		}
	}

	return true;
}

void COpenGL3MaterialRenderer::setBasicRenderStates(const SMaterial &material,
		const SMaterial &lastMaterial,
		bool resetAllRenderstates)
{
	Driver->setBasicRenderStates(material, lastMaterial, resetAllRenderstates);
}

s32 COpenGL3MaterialRenderer::getVertexShaderConstantID(const c8 *name)
{
	return getPixelShaderConstantID(name);
}

s32 COpenGL3MaterialRenderer::getPixelShaderConstantID(const c8 *name)
{
	for (u32 i = 0; i < UniformInfo.size(); ++i) {
		if (UniformInfo[i].name == name)
			return i;
	}

	return -1;
}

bool COpenGL3MaterialRenderer::setVertexShaderConstant(s32 index, const f32 *floats, int count)
{
	return setPixelShaderConstant(index, floats, count);
}

bool COpenGL3MaterialRenderer::setVertexShaderConstant(s32 index, const s32 *ints, int count)
{
	return setPixelShaderConstant(index, ints, count);
}

bool COpenGL3MaterialRenderer::setVertexShaderConstant(s32 index, const u32 *ints, int count)
{
	return setPixelShaderConstant(index, ints, count);
}

bool COpenGL3MaterialRenderer::setPixelShaderConstant(s32 index, const f32 *floats, int count)
{
	if (index < 0 || UniformInfo[index].location < 0)
		return false;

	bool status = true;

	switch (UniformInfo[index].type) {
	case GL_FLOAT:
		GL.Uniform1fv(UniformInfo[index].location, count, floats);
		break;
	case GL_FLOAT_VEC2:
		GL.Uniform2fv(UniformInfo[index].location, count / 2, floats);
		break;
	case GL_FLOAT_VEC3:
		GL.Uniform3fv(UniformInfo[index].location, count / 3, floats);
		break;
	case GL_FLOAT_VEC4:
		GL.Uniform4fv(UniformInfo[index].location, count / 4, floats);
		break;
	case GL_FLOAT_MAT2:
		GL.UniformMatrix2fv(UniformInfo[index].location, count / 4, false, floats);
		break;
	case GL_FLOAT_MAT3:
		GL.UniformMatrix3fv(UniformInfo[index].location, count / 9, false, floats);
		break;
	case GL_FLOAT_MAT4:
		GL.UniformMatrix4fv(UniformInfo[index].location, count / 16, false, floats);
		break;
	case GL_SAMPLER_2D:
	case GL_SAMPLER_CUBE: {
		if (floats) {
			const GLint id = (GLint)(*floats);
			GL.Uniform1iv(UniformInfo[index].location, 1, &id);
		} else
			status = false;
	} break;
	default:
		status = false;
		break;
	}

	return status;
}

bool COpenGL3MaterialRenderer::setPixelShaderConstant(s32 index, const s32 *ints, int count)
{
	if (index < 0 || UniformInfo[index].location < 0)
		return false;

	bool status = true;

	switch (UniformInfo[index].type) {
	case GL_INT:
	case GL_BOOL:
		GL.Uniform1iv(UniformInfo[index].location, count, ints);
		break;
	case GL_INT_VEC2:
	case GL_BOOL_VEC2:
		GL.Uniform2iv(UniformInfo[index].location, count / 2, ints);
		break;
	case GL_INT_VEC3:
	case GL_BOOL_VEC3:
		GL.Uniform3iv(UniformInfo[index].location, count / 3, ints);
		break;
	case GL_INT_VEC4:
	case GL_BOOL_VEC4:
		GL.Uniform4iv(UniformInfo[index].location, count / 4, ints);
		break;
	case GL_SAMPLER_2D:
	case GL_SAMPLER_CUBE:
		GL.Uniform1iv(UniformInfo[index].location, 1, ints);
		break;
	default:
		status = false;
		break;
	}

	return status;
}

bool COpenGL3MaterialRenderer::setPixelShaderConstant(s32 index, const u32 *ints, int count)
{
	os::Printer::log("Unsigned int support is unimplemented", ELL_WARNING);
	return false;
}

IVideoDriver *COpenGL3MaterialRenderer::getVideoDriver()
{
	return Driver;
}

}
