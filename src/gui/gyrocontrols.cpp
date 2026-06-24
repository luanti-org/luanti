
#include "gyrocontrols.h"

#include "settings.h"
#include "client/inputhandler.h"
#include <IrrlichtDevice.h>
#include "irrMath.h"

GyroControls *g_gyrocontrols;

GyroControls::GyroControls(IrrlichtDevice *device):
		m_device(device),
		m_receiver(device->getEventReceiver())
{

}

GyroControls::~GyroControls()
{

}

bool GyroControls::OnEvent(const SEvent &event)
{
	if (event.EventType != EET_GAMEPAD_SENSOR_EVENT || event.GamepadSensorEvent.Type != ESENSOR_GYRO)
		return false;

	const double yaw_sens = g_settings->getFloat("gyro_yaw_sensitivity", 0.1f, 20.0f);
	const double pitch_sens = g_settings->getFloat("gyro_pitch_sensitivity", 0.1f, 20.0f);

	vec.X += radToDeg(event.GamepadSensorEvent.Y) * yaw_sens;
	vec.Y += radToDeg(event.GamepadSensorEvent.X) * pitch_sens;
	samples++;

	return false;
}
