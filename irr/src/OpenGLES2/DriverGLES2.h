// Copyright (C) 2023 Vitaliy Lobachevskiy
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once
#include "OpenGL/Driver.h"

namespace video
{

/// OpenGL ES 2+ driver
///
/// For OpenGL ES 2.0 and higher.
class COpenGLES2Driver final : public COpenGL3DriverBase
{
	friend IVideoDriver *createOGLES2Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager);

public:
	using COpenGL3DriverBase::COpenGL3DriverBase;
	E_DRIVER_TYPE getDriverType() const override;

protected:
	OpenGLVersion getVersionFromOpenGL() const override;
	void initFeatures() override;
};

}
