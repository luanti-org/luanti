
#include "gyrocontrols.h"

#include "log.h"
#include "settings.h"
#include "client/inputhandler.h"
#include <IrrlichtDevice.h>
#include "irrMath.h"
#include "util/enum_string.h"

static const char *setting_names[] = {
	"gyro_yaw_sensitivity",
	"gyro_pitch_sensitivity",
	"gyro_toggle_mode",
	"gyro_turn_axis"
};

const struct EnumString es_GyroToggleMode[] =
{
	{GYRO_ENABLE, "enable"},
	{GYRO_DISABLE, "disable"},
	{GYRO_INVERT, "invert"},
	{0, NULL},
};

const struct EnumString es_GyroTurnAxis[] =
{
	{GYRO_YAW, "yaw"},
	{GYRO_ROLL, "roll"},
	{0, NULL},
};

GyroControls *g_gyrocontrols;

GyroControls::GyroControls(IrrlichtDevice *device):
		m_device(device),
		m_receiver(device->getEventReceiver())
{
	readSettings();
	for (auto name : setting_names)
		g_settings->registerChangedCallback(name, settingChangedCallback, this);
}

GyroControls::~GyroControls()
{

}

void GyroControls::readSettings()
{
	const std::string &toggle = g_settings->get("gyro_toggle_mode");
	if (!string_to_enum(es_GyroToggleMode, m_toggle_mode, toggle)) {
		m_toggle_mode = GYRO_DISABLE;
		warningstream << "Invalid gyro_toggle_mode value" << std::endl;
	}
	initActive();

	const std::string &axis = g_settings->get("gyro_turn_axis");
	if (!string_to_enum(es_GyroTurnAxis, m_turn_axis, axis)) {
		m_turn_axis = GYRO_YAW;
		warningstream << "Invalid gyro_turn_axis value" << std::endl;
	}

	m_yaw_sensitivity = g_settings->getFloat("gyro_yaw_sensitivity", 0.1f, 20.0f);
	m_pitch_sensitivity = g_settings->getFloat("gyro_pitch_sensitivity", 0.1f, 20.0f);
}

void GyroControls::settingChangedCallback(const std::string &name, void *data)
{
	static_cast<GyroControls *>(data)->readSettings();
}

void GyroControls::initActive()
{
	if (m_toggle_mode == GYRO_DISABLE || m_toggle_mode == GYRO_INVERT)
		m_active = true;
	if (m_toggle_mode == GYRO_ENABLE)
		m_active = false;
}

void GyroControls::toggle()
{
	if (m_toggle_mode == GYRO_ENABLE || m_toggle_mode == GYRO_DISABLE)
		m_active = !m_active;
	if (m_toggle_mode == GYRO_INVERT)
		m_dir = -m_dir;
}

bool GyroControls::OnEvent(const SEvent &event)
{
	if (!m_active)
		return false;
	if (event.EventType != EET_GAMEPAD_SENSOR_EVENT || event.GamepadSensorEvent.Type != ESENSOR_GYRO)
		return false;

	m_gyro_samples.X += radToDeg(event.GamepadSensorEvent.Y);
	m_gyro_samples.Y += radToDeg(event.GamepadSensorEvent.X);
	m_gyro_samples.Z += radToDeg(event.GamepadSensorEvent.Z);
	m_gyro_samples_count++;

	return false;
}
