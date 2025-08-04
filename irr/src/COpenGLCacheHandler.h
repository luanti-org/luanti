// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "COpenGLCommon.h"

#include "COpenGLCoreFeature.h"
#include "COpenGLCoreTexture.h"
#include "COpenGLCoreCacheHandler.h"

namespace video
{

class COpenGLCacheHandler : public COpenGLCoreCacheHandler<COpenGLDriver, COpenGLTexture>
{
public:
	COpenGLCacheHandler(COpenGLDriver *driver);
	virtual ~COpenGLCacheHandler();

	// Alpha calls.

	void setAlphaFunc(GLenum mode, GLclampf ref);

	void setAlphaTest(bool enable);

	// Client state calls.

	void setClientState(bool vertex, bool normal, bool color, bool texCoord0);

	// Matrix calls.

	void setMatrixMode(GLenum mode);

	// Texture calls.

	void setClientActiveTexture(GLenum texture);

protected:
	GLenum AlphaMode;
	GLclampf AlphaRef;
	bool AlphaTest;

	GLenum MatrixMode;

	GLenum ClientActiveTexture;

	bool ClientStateVertex;
	bool ClientStateNormal;
	bool ClientStateColor;
	bool ClientStateTexCoord0;
};

} // end namespace video

#endif // _IRR_COMPILE_WITH_OPENGL_
