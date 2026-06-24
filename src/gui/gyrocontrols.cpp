
#include "gyrocontrols.h"
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

	vec.X += radToDeg(event.GamepadSensorEvent.Y);
	vec.Y += radToDeg(event.GamepadSensorEvent.X);
	samples++;

	return false;
}
