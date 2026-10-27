#include "gyro_android.h"
#include "camera.h"

#ifdef __ANDROID__

#include <android/sensor.h>
#include <android/looper.h>
#include <android/log.h>
#include <cstdint>
#include <cmath>

#define LOGI(...) ((void)0)

// ============================================================
// PARÁMETROS AJUSTABLES (recompilar tras cambiarlos)
// ============================================================
static constexpr float DEADZONE_YAW    = 0.0025f;  // yaw: filtro anti-temblor
static constexpr float DEADZONE_PITCH  = 0.0008f;  // pitch: mucho más sensible
static constexpr float SMOOTH_ALPHA    = 0.20f;    // 0.1=suave, 0.5=reactivo
static constexpr float INTERNAL_SCALE  = 5.0f;     // sensibilidad general
static constexpr float PITCH_SCALE     = 1.8f;     // pitch más sensible que yaw
static constexpr float PITCH_SIGN      =  1.0f;    // +1 = normal, -1 = invertido
static constexpr float YAW_SIGN        =  1.0f;    // -1 si el yaw va invertido
static constexpr float BIAS_THRESHOLD  = 0.004f;   // umbral "quieto" para calibrar
static constexpr float BIAS_ALPHA      = 0.008f;    // velocidad de calibración de bias

// Estado
static ASensorManager      *s_sensorMgr = nullptr;
static const ASensor       *s_gyro      = nullptr;
static ASensorEventQueue   *s_queue     = nullptr;
static ALooper             *s_looper    = nullptr;
static Camera              *s_target    = nullptr;
static int64_t              s_lastTime  = 0;

static float s_lpf_yaw   = 0.0f;
static float s_lpf_pitch = 0.0f;
static float s_bias_yaw   = 0.0f;
static float s_bias_pitch = 0.0f;

bool gyro_init(Camera *cam)
{
	s_target = cam;
	LOGI("gyro_init() start");

	s_sensorMgr = ASensorManager_getInstance();
	if (!s_sensorMgr) { LOGI("ERROR: sensor manager null"); return false; }

	s_gyro = ASensorManager_getDefaultSensor(s_sensorMgr, ASENSOR_TYPE_GYROSCOPE);
	if (!s_gyro) { LOGI("ERROR: no gyroscope"); return false; }
	LOGI("gyro sensor = %s", ASensor_getName(s_gyro));

	s_looper = ALooper_forThread();
	if (!s_looper)
	    s_looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

	s_queue = ASensorManager_createEventQueue(s_sensorMgr, s_looper, 1, nullptr, nullptr);
	if (!s_queue) { LOGI("ERROR: queue null"); return false; }

	ASensorEventQueue_enableSensor(s_queue, s_gyro);
	ASensorEventQueue_setEventRate(s_queue, s_gyro, 16667);

	s_lastTime = 0;
	s_lpf_yaw = s_lpf_pitch = 0.0f;
	s_bias_yaw = s_bias_pitch = 0.0f;
	LOGI("gyro_init() OK");
	return true;
}

void gyro_poll(float /*sensitivity ignorado, usamos INTERNAL_SCALE*/)
{
	if (!s_target || !s_queue)
	    return;

	ASensorEvent ev;
	float yawRate = 0.0f;
	float pitchRate = 0.0f;
	int64_t latestTime = s_lastTime;
	int events = 0;

	while (ASensorEventQueue_getEvents(s_queue, &ev, 1) > 0) {
	    if (ev.type == ASENSOR_TYPE_GYROSCOPE) {
	        if (s_lastTime != 0 && ev.timestamp > s_lastTime) {
	            float dt = (ev.timestamp - s_lastTime) * 1e-9f;
	            // Landscape: yaw = eje X, pitch = eje Y
	            pitchRate += ev.gyro.y * dt;
	            yawRate   += ev.gyro.x * dt;
	        }
	        latestTime = ev.timestamp;
	        events++;
	    }
	}
	s_lastTime = latestTime;

	if (events == 0)
	    return;

	// ---- Calibración de bias: solo se actualiza cuando el móvil está "quieto" ----
	if (std::fabs(yawRate) < BIAS_THRESHOLD)
	    s_bias_yaw = s_bias_yaw * (1.0f - BIAS_ALPHA) + yawRate * BIAS_ALPHA;
	if (std::fabs(pitchRate) < BIAS_THRESHOLD)
	    s_bias_pitch = s_bias_pitch * (1.0f - BIAS_ALPHA) + pitchRate * BIAS_ALPHA;

	// ---- Restar bias ----
	float corr_yaw   = yawRate   - s_bias_yaw;
	float corr_pitch = pitchRate - s_bias_pitch;

	// ---- Suavizado exponencial (baja frecuencia) ----
	s_lpf_yaw   = s_lpf_yaw   * (1.0f - SMOOTH_ALPHA) + corr_yaw   * SMOOTH_ALPHA;
	s_lpf_pitch = s_lpf_pitch * (1.0f - SMOOTH_ALPHA) + corr_pitch * SMOOTH_ALPHA;

	// ---- Deadzone sobre el valor suavizado ----
	float out_yaw   = (std::fabs(s_lpf_yaw)   < DEADZONE_YAW)   ? 0.0f : s_lpf_yaw;
	float out_pitch = (std::fabs(s_lpf_pitch) < DEADZONE_PITCH) ? 0.0f : s_lpf_pitch;

	if (out_yaw != 0.0f || out_pitch != 0.0f) {
	    s_target->addGyroDelta(
	        out_yaw   * INTERNAL_SCALE * YAW_SIGN,
	        out_pitch * INTERNAL_SCALE * PITCH_SCALE * PITCH_SIGN);
	}
}

void gyro_deinit()
{
	if (s_queue && s_gyro)
	    ASensorEventQueue_disableSensor(s_queue, s_gyro);
	if (s_sensorMgr && s_queue)
	    ASensorManager_destroyEventQueue(s_sensorMgr, s_queue);
	s_queue = nullptr;
	s_gyro = nullptr;
	s_looper = nullptr;
	s_target = nullptr;
	s_lastTime = 0;
}

#else // !__ANDROID__

bool gyro_init(Camera *) { return false; }
void gyro_poll(float) {}
void gyro_deinit() {}

#endif
