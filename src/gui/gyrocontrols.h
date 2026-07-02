
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
extern const struct EnumString es_GyroToggleMode[];

enum GyroTurnAxis : u8
{
	GYRO_YAW,
	GYRO_ROLL
};
extern const struct EnumString es_GyroTurnAxis[];

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

	v2f32 getCameraOffset()
	{
		v3f average_vector = getAverageGyro();
		v2f32 out_vector = v2f32(0.0, 0.0);
		if (m_turn_axis == GYRO_YAW) {
			out_vector.X = average_vector.X;
			out_vector.Y = average_vector.Y;
		} else if (m_turn_axis == GYRO_ROLL) {
			// roll needs to be inverted
			out_vector.X = -average_vector.Z;
			out_vector.Y = average_vector.Y;
		}
		out_vector.X *= m_yaw_sensitivity * m_dir;
		out_vector.Y *= m_pitch_sensitivity * m_dir;
		return out_vector;
	}

	v3f getAverageGyro()
	{
		if (m_gyro_samples_count == 0)
			return v3f(0.0, 0.0, 0.0);
		v3f average_vector = m_gyro_samples / m_gyro_samples_count;
		m_gyro_samples = v3f(0.0, 0.0, 0.0);
		m_gyro_samples_count = 0;
		return average_vector;
	}

private:
	IrrlichtDevice *m_device = nullptr;
	IEventReceiver *m_receiver = nullptr;

	static void settingChangedCallback(const std::string &name, void *data);

	// cached settings
	GyroToggleMode m_toggle_mode;
	GyroTurnAxis m_turn_axis;
	double m_yaw_sensitivity;
	double m_pitch_sensitivity;

	bool m_active = false;
	int m_dir = 1;

	// accumulated rotation in degrees
	v3f m_gyro_samples = v3f(0.0, 0.0, 0.0);
	// number of samples accumulated
	int m_gyro_samples_count = 0;
};

extern GyroControls *g_gyrocontrols;
