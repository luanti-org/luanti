
#pragma once

#include "irrlichttypes_bloated.h"
#include "IEventReceiver.h"

#include "util/basic_macros.h"
#include "client/keycode.h"

#include <array>

class IrrlichtDevice;

using namespace core;
using namespace gui;

class GyroControls
{
public:
	GyroControls(IrrlichtDevice *device);
	~GyroControls();
	DISABLE_CLASS_COPY(GyroControls);

	bool OnEvent(const SEvent &event);
	bool isActive(const bool toggle);

	v2f32 getVector()
	{
		if (samples == 0)
			return v2f32(0.0, 0.0);
		v2f32 v = vec / samples;
		vec = v2f32(0.0, 0.0);
		samples = 0;
		return v;
	}

private:
	IrrlichtDevice *m_device = nullptr;
	IEventReceiver *m_receiver = nullptr;

	// accumulated rotation in degrees
	v2f32 vec = v2f32(0.0, 0.0);
	int samples = 0;
};

extern GyroControls *g_gyrocontrols;
