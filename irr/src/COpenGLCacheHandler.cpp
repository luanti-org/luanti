// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "COpenGLCacheHandler.h"

#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "COpenGLDriver.h"

namespace video
{

/* COpenGLCacheHandler */

COpenGLCacheHandler::COpenGLCacheHandler(COpenGLDriver *driver) :
		COpenGLCoreCacheHandler<COpenGLDriver, COpenGLTexture>(driver), AlphaMode(GL_ALWAYS), AlphaRef(0.f), AlphaTest(false),
		MatrixMode(GL_MODELVIEW), ClientActiveTexture(GL_TEXTURE0), ClientStateVertex(false),
		ClientStateNormal(false), ClientStateColor(false), ClientStateTexCoord0(false)
{
	// Initial OpenGL values from specification.

	glAlphaFunc(AlphaMode, AlphaRef);
	glDisable(GL_ALPHA_TEST);

	glMatrixMode(MatrixMode);

	Driver->irrGlClientActiveTexture(ClientActiveTexture);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

COpenGLCacheHandler::~COpenGLCacheHandler()
{
}

void COpenGLCacheHandler::setAlphaFunc(GLenum mode, GLclampf ref)
{
	if (AlphaMode != mode || AlphaRef != ref) {
		glAlphaFunc(mode, ref);

		AlphaMode = mode;
		AlphaRef = ref;
	}
}

void COpenGLCacheHandler::setAlphaTest(bool enable)
{
	if (AlphaTest != enable) {
		if (enable)
			glEnable(GL_ALPHA_TEST);
		else
			glDisable(GL_ALPHA_TEST);
		AlphaTest = enable;
	}
}

void COpenGLCacheHandler::setClientState(bool vertex, bool normal, bool color, bool texCoord0)
{
	if (ClientStateVertex != vertex) {
		if (vertex)
			glEnableClientState(GL_VERTEX_ARRAY);
		else
			glDisableClientState(GL_VERTEX_ARRAY);

		ClientStateVertex = vertex;
	}

	if (ClientStateNormal != normal) {
		if (normal)
			glEnableClientState(GL_NORMAL_ARRAY);
		else
			glDisableClientState(GL_NORMAL_ARRAY);

		ClientStateNormal = normal;
	}

	if (ClientStateColor != color) {
		if (color)
			glEnableClientState(GL_COLOR_ARRAY);
		else
			glDisableClientState(GL_COLOR_ARRAY);

		ClientStateColor = color;
	}

	if (ClientStateTexCoord0 != texCoord0) {
		setClientActiveTexture(GL_TEXTURE0_ARB);

		if (texCoord0)
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		else
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		ClientStateTexCoord0 = texCoord0;
	}
}

void COpenGLCacheHandler::setMatrixMode(GLenum mode)
{
	if (MatrixMode != mode) {
		glMatrixMode(mode);
		MatrixMode = mode;
	}
}

void COpenGLCacheHandler::setClientActiveTexture(GLenum texture)
{
	if (ClientActiveTexture != texture) {
		Driver->irrGlClientActiveTexture(texture);
		ClientActiveTexture = texture;
	}
}

} // end namespace

#endif // _IRR_COMPILE_WITH_OPENGL_
