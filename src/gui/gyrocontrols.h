
#pragma once

#include "irrlichttypes_bloated.h"
#include "IEventReceiver.h"

#include "util/basic_macros.h"
#include "client/keycode.h"

#include <array>

class IrrlichtDevice;

using namespace core;
using namespace gui;

enum GyroToggleMode : u8
{
	GYRO_ENABLE,
	GYRO_DISABLE,
	GYRO_INVERT
};
extern const struct EnumString es_TouchInteractionStyle[];

class GyroControls
{
public:
	GyroControls(IrrlichtDevice *device);
	~GyroControls();
	DISABLE_CLASS_COPY(GyroControls);

	void readSettings();
	void initActive();

	bool OnEvent(const SEvent &event);
	void toggle();

	v2f32 getVector()
	{
		if (m_samples == 0)
			return v2f32(0.0, 0.0);
		v2f32 v = m_vector / m_samples;
		m_vector = v2f32(0.0, 0.0);
		m_samples = 0;
		return v;
	}

private:
	IrrlichtDevice *m_device = nullptr;
	IEventReceiver *m_receiver = nullptr;

	static void settingChangedCallback(const std::string &name, void *data);

	// cached settings
	GyroToggleMode m_toggle_mode;
	double m_yaw_sensitivity;
	double m_pitch_sensitivity;

	bool m_active = false;
	int m_dir = 1;

	// accumulated rotation in degrees
	v2f32 m_vector = v2f32(0.0, 0.0);
	// number of samples accumulated
	int m_samples = 0;
};

extern GyroControls *g_gyrocontrols;
