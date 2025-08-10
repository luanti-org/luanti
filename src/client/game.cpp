// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "game.h"

#include <cmath>
#include "client/renderingengine.h"
#include "camera.h"
#include "client.h"
#include "client/clientevent.h"
#include "client/gameui.h"
#include "client/game_formspec.h"
#include "client/inputhandler.h"
#include "client/texturepaths.h"
#include "client/keys.h"
#include "client/joystick_controller.h"
#include "client/mapblock_mesh.h"
#include "client/sound.h"
#include "clientmap.h"
#include "clientmedia.h" // For clientMediaUpdateCacheCopy
#include "clouds.h"
#include "config.h"
#include "content_cao.h"
#include "content/subgames.h"
#include "client/event_manager.h"
#include "fontengine.h"
#include "gui/touchcontrols.h"
#include "itemdef.h"
#include "log.h"
#include "log_internal.h"
#include "gameparams.h"
#include "gettext.h"
#include "gui/guiChatConsole.h"
#include "texturesource.h"
#include "gui/mainmenumanager.h"
#include "gui/profilergraph.h"
#include "minimap.h"
#include "nodedef.h"         // Needed for determining pointing to nodes
#include "nodemetadata.h"
#include "particles.h"
#include "porting.h"
#include "profiler.h"
#include "raycast.h"
#include "server.h"
#include "settings.h"
#include "shader.h"
#include "sky.h"
#include "threading/lambda.h"
#include "translation.h"
#include "util/basic_macros.h"
#include "util/directiontables.h"
#include "util/pointedthing.h"
#include "util/quicktune_shortcutter.h"
#include "irr_ptr.h"
#include "version.h"
#include "script/scripting_client.h"
#include "hud.h"
#include "clientdynamicinfo.h"
#include <IAnimatedMeshSceneNode.h>
#include "util/tracy_wrapper.h"
#include "item_visuals_manager.h"

#if USE_SOUND
	#include "client/sound/sound_openal.h"
#endif

#include <csignal>

class NodeDugEvent : public MtEvent
{
public:
	v3s16 p;
	MapNode n;

	NodeDugEvent(v3s16 p, MapNode n):
		p(p),
		n(n)
	{}
	Type getType() const { return NODE_DUG; }
};

class SoundMaker
{
	ISoundManager *m_sound;
	const NodeDefManager *m_ndef;

public:
	bool makes_footstep_sound = true;
	float m_player_step_timer = 0.0f;
	float m_player_jump_timer = 0.0f;

	SoundSpec m_player_step_sound;
	SoundSpec m_player_leftpunch_sound;
	// Second sound made on left punch, currently used for item 'use' sound
	SoundSpec m_player_leftpunch_sound2;
	SoundSpec m_player_rightpunch_sound;

	SoundMaker(ISoundManager *sound, const NodeDefManager *ndef) :
		m_sound(sound), m_ndef(ndef) {}

	void playPlayerStep()
	{
		if (m_player_step_timer <= 0 && m_player_step_sound.exists()) {
			m_player_step_timer = 0.03;
			if (makes_footstep_sound)
				m_sound->playSound(0, m_player_step_sound);
		}
	}

	void playPlayerJump()
	{
		if (m_player_jump_timer <= 0.0f) {
			m_player_jump_timer = 0.2f;
			m_sound->playSound(0, SoundSpec("player_jump", 0.5f));
		}
	}

	static void viewBobbingStep(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->playPlayerStep();
	}

	static void playerRegainGround(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->playPlayerStep();
	}

	static void playerJump(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->playPlayerJump();
	}

	static void cameraPunchLeft(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(0, sm->m_player_leftpunch_sound);
		sm->m_sound->playSound(0, sm->m_player_leftpunch_sound2);
	}

	static void cameraPunchRight(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(0, sm->m_player_rightpunch_sound);
	}

	static void nodeDug(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		NodeDugEvent *nde = (NodeDugEvent *)e;
		sm->m_sound->playSound(0, sm->m_ndef->get(nde->n).sound_dug);
	}

	static void playerDamage(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(0, SoundSpec("player_damage", 0.5));
	}

	static void playerFallingDamage(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(0, SoundSpec("player_falling_damage", 0.5));
	}

	void registerReceiver(MtEventManager *mgr)
	{
		mgr->reg(MtEvent::VIEW_BOBBING_STEP, SoundMaker::viewBobbingStep, this);
		mgr->reg(MtEvent::PLAYER_REGAIN_GROUND, SoundMaker::playerRegainGround, this);
		mgr->reg(MtEvent::PLAYER_JUMP, SoundMaker::playerJump, this);
		mgr->reg(MtEvent::CAMERA_PUNCH_LEFT, SoundMaker::cameraPunchLeft, this);
		mgr->reg(MtEvent::CAMERA_PUNCH_RIGHT, SoundMaker::cameraPunchRight, this);
		mgr->reg(MtEvent::NODE_DUG, SoundMaker::nodeDug, this);
		mgr->reg(MtEvent::PLAYER_DAMAGE, SoundMaker::playerDamage, this);
		mgr->reg(MtEvent::PLAYER_FALLING_DAMAGE, SoundMaker::playerFallingDamage, this);
	}

	void step(float dtime)
	{
		m_player_step_timer -= dtime;
		m_player_jump_timer -= dtime;
	}
};


typedef s32 SamplerLayer_t;


class GameGlobalShaderUniformSetter : public IShaderUniformSetter
{
	Sky *m_sky;
	Client *m_client;
	CachedVertexShaderSetting<float> m_animation_timer_vertex{"animationTimer"};
	CachedPixelShaderSetting<float> m_animation_timer_pixel{"animationTimer"};
	CachedVertexShaderSetting<float>
		m_animation_timer_delta_vertex{"animationTimerDelta"};
	CachedPixelShaderSetting<float>
		m_animation_timer_delta_pixel{"animationTimerDelta"};
	CachedPixelShaderSetting<float, 3> m_day_light{"dayLight"};
	CachedPixelShaderSetting<float, 3> m_minimap_yaw{"yawVec"};
	CachedPixelShaderSetting<float, 3> m_camera_offset_pixel{"cameraOffset"};
	CachedVertexShaderSetting<float, 3> m_camera_offset_vertex{"cameraOffset"};
	CachedPixelShaderSetting<float, 3> m_camera_position_pixel{ "cameraPosition" };
	CachedVertexShaderSetting<float, 3> m_camera_position_vertex{ "cameraPosition" };
	CachedVertexShaderSetting<float, 2> m_texel_size0_vertex{"texelSize0"};
	CachedPixelShaderSetting<float, 2> m_texel_size0_pixel{"texelSize0"};
	v2f m_texel_size0;
	CachedStructPixelShaderSetting<float, 7> m_exposure_params_pixel{
		"exposureParams",
		std::array<const char*, 7> {
			"luminanceMin", "luminanceMax", "exposureCorrection",
			"speedDarkBright", "speedBrightDark", "centerWeightPower",
			"compensationFactor"
		}};
	float m_user_exposure_compensation;
	bool m_bloom_enabled;
	CachedPixelShaderSetting<float> m_bloom_intensity_pixel{"bloomIntensity"};
	CachedPixelShaderSetting<float> m_bloom_strength_pixel{"bloomStrength"};
	CachedPixelShaderSetting<float> m_bloom_radius_pixel{"bloomRadius"};
	CachedPixelShaderSetting<float> m_saturation_pixel{"saturation"};
	bool m_volumetric_light_enabled;
	CachedPixelShaderSetting<float, 3>
		m_sun_position_pixel{"sunPositionScreen"};
	CachedPixelShaderSetting<float> m_sun_brightness_pixel{"sunBrightness"};
	CachedPixelShaderSetting<float, 3>
		m_moon_position_pixel{"moonPositionScreen"};
	CachedPixelShaderSetting<float> m_moon_brightness_pixel{"moonBrightness"};
	CachedPixelShaderSetting<float>
		m_volumetric_light_strength_pixel{"volumetricLightStrength"};

	static constexpr std::array<const char*, 1> SETTING_CALLBACKS = {
		"exposure_compensation",
	};

public:
	void onSettingsChange(const std::string &name)
	{
		if (name == "exposure_compensation")
			m_user_exposure_compensation = g_settings->getFloat("exposure_compensation", -1.0f, 1.0f);
	}

	static void settingsCallback(const std::string &name, void *userdata)
	{
		reinterpret_cast<GameGlobalShaderUniformSetter*>(userdata)->onSettingsChange(name);
	}

	void setSky(Sky *sky) { m_sky = sky; }

	GameGlobalShaderUniformSetter(Sky *sky, Client *client) :
		m_sky(sky),
		m_client(client)
	{
		for (auto &name : SETTING_CALLBACKS)
			g_settings->registerChangedCallback(name, settingsCallback, this);

		m_user_exposure_compensation = g_settings->getFloat("exposure_compensation", -1.0f, 1.0f);
		m_bloom_enabled = g_settings->getBool("enable_bloom");
		m_volumetric_light_enabled = g_settings->getBool("enable_volumetric_lighting") && m_bloom_enabled;
	}

	~GameGlobalShaderUniformSetter()
	{
		g_settings->deregisterAllChangedCallbacks(this);
	}

	void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		u32 daynight_ratio = (float)m_client->getEnv().getDayNightRatio();
		video::SColorf sunlight;
		get_sunlight_color(&sunlight, daynight_ratio);
		m_day_light.set(sunlight, services);

		u32 animation_timer = m_client->getEnv().getFrameTime() % 1000000;
		float animation_timer_f = (float)animation_timer / 100000.f;
		m_animation_timer_vertex.set(&animation_timer_f, services);
		m_animation_timer_pixel.set(&animation_timer_f, services);

		float animation_timer_delta_f = (float)m_client->getEnv().getFrameTimeDelta() / 100000.f;
		m_animation_timer_delta_vertex.set(&animation_timer_delta_f, services);
		m_animation_timer_delta_pixel.set(&animation_timer_delta_f, services);

		if (m_client->getMinimap()) {
			v3f minimap_yaw = m_client->getMinimap()->getYawVec();
			m_minimap_yaw.set(minimap_yaw, services);
		}

		v3f offset = intToFloat(m_client->getCamera()->getOffset(), BS);
		m_camera_offset_pixel.set(offset, services);
		m_camera_offset_vertex.set(offset, services);

		v3f camera_position = m_client->getCamera()->getPosition();
		m_camera_position_pixel.set(camera_position, services);
		m_camera_position_pixel.set(camera_position, services);

		m_texel_size0_vertex.set(m_texel_size0, services);
		m_texel_size0_pixel.set(m_texel_size0, services);

		const auto &lighting = m_client->getEnv().getLocalPlayer()->getLighting();

		const AutoExposure &exposure_params = lighting.exposure;
		std::array<float, 7> exposure_buffer = {
			std::pow(2.0f, exposure_params.luminance_min),
			std::pow(2.0f, exposure_params.luminance_max),
			exposure_params.exposure_correction,
			exposure_params.speed_dark_bright,
			exposure_params.speed_bright_dark,
			exposure_params.center_weight_power,
			powf(2.f, m_user_exposure_compensation)
		};
		m_exposure_params_pixel.set(exposure_buffer.data(), services);

		if (m_bloom_enabled) {
			float intensity = std::max(lighting.bloom_intensity, 0.0f);
			m_bloom_intensity_pixel.set(&intensity, services);
			float strength_factor = std::max(lighting.bloom_strength_factor, 0.0f);
			m_bloom_strength_pixel.set(&strength_factor, services);
			float radius = std::max(lighting.bloom_radius, 0.0f);
			m_bloom_radius_pixel.set(&radius, services);
		}

		float saturation = lighting.saturation;
		m_saturation_pixel.set(&saturation, services);

		if (m_volumetric_light_enabled) {
			// Map directional light to screen space
			auto camera_node = m_client->getCamera()->getCameraNode();
			core::matrix4 transform = camera_node->getProjectionMatrix();
			transform *= camera_node->getViewMatrix();

			if (m_sky->getSunVisible()) {
				v3f sun_position = camera_node->getAbsolutePosition() +
						10000.f * m_sky->getSunDirection();
				transform.transformVect(sun_position);
				sun_position.normalize();

				m_sun_position_pixel.set(sun_position, services);

				float sun_brightness = core::clamp(107.143f * m_sky->getSunDirection().Y, 0.f, 1.f);
				m_sun_brightness_pixel.set(&sun_brightness, services);
			} else {
				m_sun_position_pixel.set(v3f(0.f, 0.f, -1.f), services);

				float sun_brightness = 0.f;
				m_sun_brightness_pixel.set(&sun_brightness, services);
			}

			if (m_sky->getMoonVisible()) {
				v3f moon_position = camera_node->getAbsolutePosition() +
						10000.f * m_sky->getMoonDirection();
				transform.transformVect(moon_position);
				moon_position.normalize();

				m_moon_position_pixel.set(moon_position, services);

				float moon_brightness = core::clamp(107.143f * m_sky->getMoonDirection().Y, 0.f, 1.f);
				m_moon_brightness_pixel.set(&moon_brightness, services);
			} else {
				m_moon_position_pixel.set(v3f(0.f, 0.f, -1.f), services);

				float moon_brightness = 0.f;
				m_moon_brightness_pixel.set(&moon_brightness, services);
			}

			float volumetric_light_strength = lighting.volumetric_light_strength;
			m_volumetric_light_strength_pixel.set(&volumetric_light_strength, services);
		}
	}

	void onSetMaterial(const video::SMaterial &material) override
	{
		video::ITexture *texture = material.getTexture(0);
		if (texture) {
			core::dimension2du size = texture->getSize();
			m_texel_size0 = v2f(1.f / size.Width, 1.f / size.Height);
		} else {
			m_texel_size0 = v2f();
		}
	}
};


class GameGlobalShaderUniformSetterFactory : public IShaderUniformSetterFactory
{
	Sky *m_sky = nullptr;
	Client *m_client;
	std::vector<GameGlobalShaderUniformSetter *> created_nosky;
public:
	GameGlobalShaderUniformSetterFactory(Client *client) :
		m_client(client)
	{}

	void setSky(Sky *sky)
	{
		m_sky = sky;
		for (GameGlobalShaderUniformSetter *ggscs : created_nosky) {
			ggscs->setSky(m_sky);
		}
		created_nosky.clear();
	}

	virtual IShaderUniformSetter* create()
	{
		auto *scs = new GameGlobalShaderUniformSetter(m_sky, m_client);
		if (!m_sky)
			created_nosky.push_back(scs);
		return scs;
	}
};

class NodeShaderConstantSetter : public IShaderConstantSetter
{
public:
	NodeShaderConstantSetter() = default;
	~NodeShaderConstantSetter() = default;

	void onGenerate(const std::string &name, ShaderConstants &constants) override
	{
		if (constants.find("DRAWTYPE") == constants.end())
			return; // not a node shader
		[[maybe_unused]] const auto drawtype =
			static_cast<NodeDrawType>(std::get<int>(constants["DRAWTYPE"]));
		[[maybe_unused]] const auto material_type =
			static_cast<MaterialType>(std::get<int>(constants["MATERIAL_TYPE"]));

#define PROVIDE(constant) constants[ #constant ] = (int)constant

		PROVIDE(NDT_NORMAL);
		PROVIDE(NDT_AIRLIKE);
		PROVIDE(NDT_LIQUID);
		PROVIDE(NDT_FLOWINGLIQUID);
		PROVIDE(NDT_GLASSLIKE);
		PROVIDE(NDT_ALLFACES);
		PROVIDE(NDT_ALLFACES_OPTIONAL);
		PROVIDE(NDT_TORCHLIKE);
		PROVIDE(NDT_SIGNLIKE);
		PROVIDE(NDT_PLANTLIKE);
		PROVIDE(NDT_FENCELIKE);
		PROVIDE(NDT_RAILLIKE);
		PROVIDE(NDT_NODEBOX);
		PROVIDE(NDT_GLASSLIKE_FRAMED);
		PROVIDE(NDT_FIRELIKE);
		PROVIDE(NDT_GLASSLIKE_FRAMED_OPTIONAL);
		PROVIDE(NDT_PLANTLIKE_ROOTED);

		PROVIDE(TILE_MATERIAL_BASIC);
		PROVIDE(TILE_MATERIAL_ALPHA);
		PROVIDE(TILE_MATERIAL_LIQUID_TRANSPARENT);
		PROVIDE(TILE_MATERIAL_LIQUID_OPAQUE);
		PROVIDE(TILE_MATERIAL_WAVING_LEAVES);
		PROVIDE(TILE_MATERIAL_WAVING_PLANTS);
		PROVIDE(TILE_MATERIAL_OPAQUE);
		PROVIDE(TILE_MATERIAL_WAVING_LIQUID_BASIC);
		PROVIDE(TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT);
		PROVIDE(TILE_MATERIAL_WAVING_LIQUID_OPAQUE);
		PROVIDE(TILE_MATERIAL_PLAIN);
		PROVIDE(TILE_MATERIAL_PLAIN_ALPHA);

#undef PROVIDE

		bool enable_waving_water = g_settings->getBool("enable_waving_water");
		constants["ENABLE_WAVING_WATER"] = enable_waving_water ? 1 : 0;
		if (enable_waving_water) {
			constants["WATER_WAVE_HEIGHT"] = g_settings->getFloat("water_wave_height");
			constants["WATER_WAVE_LENGTH"] = g_settings->getFloat("water_wave_length");
			constants["WATER_WAVE_SPEED"] = g_settings->getFloat("water_wave_speed");
		}
		switch (material_type) {
			case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			case TILE_MATERIAL_WAVING_LIQUID_OPAQUE:
			case TILE_MATERIAL_WAVING_LIQUID_BASIC:
				constants["MATERIAL_WAVING_LIQUID"] = 1;
				break;
			default:
				constants["MATERIAL_WAVING_LIQUID"] = 0;
				break;
		}
		switch (material_type) {
			case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			case TILE_MATERIAL_WAVING_LIQUID_OPAQUE:
			case TILE_MATERIAL_WAVING_LIQUID_BASIC:
			case TILE_MATERIAL_LIQUID_TRANSPARENT:
				constants["MATERIAL_WATER_REFLECTIONS"] = 1;
				break;
			default:
				constants["MATERIAL_WATER_REFLECTIONS"] = 0;
				break;
		}

		constants["ENABLE_WAVING_LEAVES"] = g_settings->getBool("enable_waving_leaves") ? 1 : 0;
		constants["ENABLE_WAVING_PLANTS"] = g_settings->getBool("enable_waving_plants") ? 1 : 0;
	}
};

/****************************************************************************
 ****************************************************************************/

const static float object_hit_delay = 0.2;

const static u16 bbox_debug_flag = scene::EDS_BBOX_ALL;

/* The reason the following structs are not anonymous structs within the
 * class is that they are not used by the majority of member functions and
 * many functions that do require objects of thse types do not modify them
 * (so they can be passed as a const qualified parameter)
 */

struct GameRunData {
	u16 dig_index;
	u16 new_playeritem;
	PointedThing pointed_old;
	bool digging;
	bool punching;
	bool btn_down_for_dig;
	bool dig_instantly;
	bool digging_blocked;
	bool reset_jump_timer;
	float nodig_delay_timer;
	float dig_time;
	float dig_time_complete;
	float repeat_place_timer;
	float object_hit_delay_timer;
	float time_from_last_punch;
	ClientActiveObject *selected_object;

	float jump_timer_up;          // from key up until key down
	float jump_timer_down;        // since last key down
	float jump_timer_down_before; // from key down until key down again

	float damage_flash;
	float update_draw_list_timer;
	float touch_blocks_timer;

	f32 fog_range;

	v3f update_draw_list_last_cam_dir;

	float time_of_day_smooth;
};

class Game;

struct ClientEventHandler
{
	void (Game::*handler)(ClientEvent *, CameraOrientation *);
};

/****************************************************************************
 THE GAME
 ****************************************************************************/

using PausedNodesList = std::vector<std::pair<irr_ptr<scene::IAnimatedMeshSceneNode>, float>>;

/* This is not intended to be a public class. If a public class becomes
 * desirable then it may be better to create another 'wrapper' class that
 * hides most of the stuff in this class (nothing in this class is required
 * by any other file) but exposes the public methods/data only.
 */
class Game {
public:
	Game();
	~Game();

	bool startup(volatile std::sig_atomic_t *kill,
			InputHandler *input,
			RenderingEngine *rendering_engine,
			const GameStartData &game_params,
			std::string &error_message,
			bool *reconnect,
			ChatBackend *chat_backend);

	void run();
	void shutdown();

protected:

	// Basic initialisation
	bool init(const std::string &map_dir, const std::string &address,
			u16 port, const SubgameSpec &gamespec);
	bool initSound();
	bool createServer(const std::string &map_dir,
			const SubgameSpec &gamespec, u16 port);
	void copyServerClientCache();

	// Client creation
	bool createClient(const GameStartData &start_data);
	bool initGui();

	// Client connection
	bool connectToServer(const GameStartData &start_data,
			bool *connect_ok, bool *aborted);
	bool getServerContent(bool *aborted);

	// Main loop

	void updateInteractTimers(f32 dtime);
	bool checkConnection();
	void processQueues();
	void updateProfilers(const RunStats &stats, const FpsControl &draw_times, f32 dtime);
	void updateDebugState();
	void updateStats(RunStats *stats, const FpsControl &draw_times, f32 dtime);
	void updateProfilerGraphs(ProfilerGraph *graph);

	// Input related
	void processUserInput(f32 dtime);
	void processKeyInput();
	void processItemSelection(u16 *new_playeritem);
	bool shouldShowTouchControls();

	void dropSelectedItem(bool single_item = false);
	void openConsole(float scale, const wchar_t *line=NULL);
	void toggleFreeMove();
	void toggleFreeMoveAlt();
	void togglePitchMove();
	void toggleFast();
	void toggleNoClip();
	void toggleCinematic();
	void toggleBlockBounds();
	void toggleAutoforward();

	void toggleMinimap(bool shift_pressed);
	void toggleFog();
	void toggleDebug();
	void toggleUpdateCamera();

	void increaseViewRange();
	void decreaseViewRange();
	void toggleFullViewRange();
	void checkZoomEnabled();

	void updateCameraDirection(CameraOrientation *cam, float dtime);
	void updateCameraOrientation(CameraOrientation *cam, float dtime);
	bool getTogglableKeyState(GameKeyType key, bool toggling_enabled, bool prev_key_state);
	void updatePlayerControl(const CameraOrientation &cam);
	void updatePauseState();
	void step(f32 dtime);
	void processClientEvents(CameraOrientation *cam);
	void updateCameraMode(); // call after changing it
	void updateCameraOffset();
	void updateCamera(f32 dtime);
	void updateSound(f32 dtime);
	void processPlayerInteraction(f32 dtime, bool show_hud);
	/*!
	 * Returns the object or node the player is pointing at.
	 * Also updates the selected thing in the Hud.
	 *
	 * @param[in]  shootline         the shootline, starting from
	 * the camera position. This also gives the maximal distance
	 * of the search.
	 * @param[in]  liquids_pointable if false, liquids are ignored
	 * @param[in]  pointabilities    item specific pointable overriding
	 * @param[in]  look_for_object   if false, objects are ignored
	 * @param[in]  camera_offset     offset of the camera
	 * @param[out] selected_object   the selected object or
	 * NULL if not found
	 */
	PointedThing updatePointedThing(
			const core::line3d<f32> &shootline, bool liquids_pointable,
			const std::optional<Pointabilities> &pointabilities,
			bool look_for_object, const v3s16 &camera_offset);
	void handlePointingAtNothing(const ItemStack &playerItem);
	void handlePointingAtNode(const PointedThing &pointed,
			const ItemStack &selected_item, const ItemStack &hand_item, f32 dtime);
	void handlePointingAtObject(const PointedThing &pointed, const ItemStack &playeritem,
			const ItemStack &hand_item, const v3f &player_position, bool show_debug);
	void handleDigging(const PointedThing &pointed, const v3s16 &nodepos,
			const ItemStack &selected_item, const ItemStack &hand_item, f32 dtime);
	void updateFrame(ProfilerGraph *graph, RunStats *stats, f32 dtime,
			const CameraOrientation &cam);
	void updateClouds(float dtime);
	void updateShadows();
	void drawScene(ProfilerGraph *graph, RunStats *stats);

	// Misc
	void showOverlayMessage(const char *msg, float dtime, int percent,
			float *indef_pos = nullptr);

	inline bool fogEnabled()
	{
		// Client setting only takes effect if fog distance unlimited or debug priv
		if (sky->getFogDistance() < 0 || client->checkPrivilege("debug"))
			return m_cache_enable_fog;
		return true;
	}

	static void settingChangedCallback(const std::string &setting_name, void *data);
	void readSettings();

	inline bool isKeyDown(GameKeyType k)
	{
		return input->isKeyDown(k);
	}
	inline bool wasKeyDown(GameKeyType k)
	{
		return input->wasKeyDown(k);
	}
	inline bool wasKeyPressed(GameKeyType k)
	{
		return input->wasKeyPressed(k);
	}
	inline bool wasKeyReleased(GameKeyType k)
	{
		return input->wasKeyReleased(k);
	}

#ifdef __ANDROID__
	void handleAndroidChatInput();
#endif

private:
	struct Flags {
		bool disable_camera_update = false;
		/// 0 = no debug text active, see toggleDebug() for the rest
		int debug_state = 0;
	};

	void pauseAnimation();
	void resumeAnimation();

	// ClientEvent handlers
	void handleClientEvent_None(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_PlayerDamage(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_PlayerForceMove(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_DeathscreenLegacy(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_ShowFormSpec(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_ShowCSMFormSpec(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_ShowPauseMenuFormSpec(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_HandleParticleEvent(ClientEvent *event,
		CameraOrientation *cam);
	void handleClientEvent_HudAdd(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_HudRemove(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_HudChange(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_SetSky(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_SetSun(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_SetMoon(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_SetStars(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_OverrideDayNigthRatio(ClientEvent *event,
		CameraOrientation *cam);
	void handleClientEvent_CloudParams(ClientEvent *event, CameraOrientation *cam);
	void handleClientEvent_UpdateCamera(ClientEvent *event, CameraOrientation *cam);

	void updateChat(f32 dtime);

	bool nodePlacement(const ItemDefinition &selected_def, const ItemStack &selected_item,
		const v3s16 &nodepos, const v3s16 &neighborpos, const PointedThing &pointed,
		const NodeMetadata *meta);
	static const ClientEventHandler clientEventHandler[CLIENTEVENT_MAX];

	f32 getSensitivityScaleFactor() const;

	InputHandler *input = nullptr;

	Client *client = nullptr;
	Server *server = nullptr;

	ClientDynamicInfo client_display_info{};
	float dynamic_info_send_timer = 0;

	IWritableTextureSource *texture_src = nullptr;
	IWritableShaderSource *shader_src = nullptr;

	// When created, these will be filled with data received from the server
	IWritableItemDefManager *itemdef_manager = nullptr;
	NodeDefManager *nodedef_manager = nullptr;
	std::unique_ptr<ItemVisualsManager> m_item_visuals_manager;

	std::unique_ptr<ISoundManager> sound_manager;
	SoundMaker *soundmaker = nullptr;

	ChatBackend *chat_backend = nullptr;
	CaptureLogOutput m_chat_log_buf;

	EventManager *eventmgr = nullptr;
	QuicktuneShortcutter *quicktune = nullptr;

	std::unique_ptr<GameUI> m_game_ui;
	irr_ptr<GUIChatConsole> gui_chat_console;
	MapDrawControl *draw_control = nullptr;
	Camera *camera = nullptr;
	irr_ptr<Clouds> clouds;
	irr_ptr<Sky> sky;
	Hud *hud = nullptr;
	Minimap *mapper = nullptr;
	GameFormSpec m_game_formspec;

	// Map server hud ids to client hud ids
	std::unordered_map<u32, u32> m_hud_server_to_client;

	GameRunData runData;
	Flags m_flags;

	/* 'cache'
	   This class does take ownership/responsibily for cleaning up etc of any of
	   these items (e.g. device)
	*/
	IrrlichtDevice             *device;
	RenderingEngine            *m_rendering_engine;
	video::IVideoDriver        *driver;
	scene::ISceneManager       *smgr;
	volatile std::sig_atomic_t *kill;
	std::string                *error_message;
	bool                       *reconnect_requested;
	PausedNodesList             paused_animated_nodes;

	bool simple_singleplayer_mode;
	/* End 'cache' */

	/* Pre-calculated values
	 */
	int crack_animation_length;

	IntervalLimiter profiler_interval;

	/*
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	bool m_cache_doubletap_jump;
	bool m_cache_toggle_sneak_key;
	bool m_cache_toggle_aux1_key;
	bool m_cache_enable_joysticks;
	bool m_cache_enable_fog;
	bool m_cache_enable_noclip;
	bool m_cache_enable_free_move;
	f32  m_cache_mouse_sensitivity;
	f32  m_cache_joystick_frustum_sensitivity;
	f32  m_repeat_place_time;
	f32  m_repeat_dig_time;
	f32  m_cache_cam_smoothing;

	bool m_invert_mouse;
	bool m_enable_hotbar_mouse_wheel;
	bool m_invert_hotbar_mouse_wheel;

	bool m_first_loop_after_window_activation = false;
	bool m_camera_offset_changed = false;
	bool m_game_focused = false;

	bool m_does_lost_focus_pause_game = false;

	// if true, (almost) the whole game is paused
	// this happens in pause menu in singleplayer
	bool m_is_paused = false;

	bool m_touch_simulate_aux1 = false;
	inline bool isTouchShootlineUsed()
	{
		return g_touchcontrols && g_touchcontrols->isShootlineAvailable() &&
				camera->getCameraMode() == CAMERA_MODE_FIRST;
	}
#ifdef __ANDROID__
	bool m_android_chat_open;
#endif

	float m_shutdown_progress = 0.0f;
};

Game::Game() :
	m_chat_log_buf(g_logger),
	m_game_ui(new GameUI())
{
	clearTextureNameCache();

	const char *settings[] = {
		"chat_log_level", "doubletap_jump", "toggle_sneak_key", "toggle_aux1_key",
		"enable_joysticks", "enable_fog", "mouse_sensitivity", "joystick_frustum_sensitivity",
		"repeat_place_time", "repeat_dig_time", "noclip", "free_move", "fog_start",
		"cinematic", "cinematic_camera_smoothing", "camera_smoothing", "invert_mouse",
		"enable_hotbar_mouse_wheel", "invert_hotbar_mouse_wheel", "pause_on_lost_focus",
	};
	for (auto s : settings)
		g_settings->registerChangedCallback(s, &settingChangedCallback, this);

	readSettings();
}


Game::~Game()
{
	delete client;
	delete soundmaker;
	sound_manager.reset();

	delete server;

	delete hud;
	delete camera;
	delete quicktune;
	delete eventmgr;
	delete texture_src;
	delete shader_src;
	delete nodedef_manager;
	delete itemdef_manager;
	delete draw_control;

	clearTextureNameCache();

	g_settings->deregisterAllChangedCallbacks(this);

	if (m_rendering_engine)
		m_rendering_engine->finalize();
}

bool Game::startup(volatile std::sig_atomic_t *kill,
		InputHandler *input,
		RenderingEngine *rendering_engine,
		const GameStartData &start_data,
		std::string &error_message,
		bool *reconnect,
		ChatBackend *chat_backend)
{

	// "cache"
	m_rendering_engine        = rendering_engine;
	device                    = m_rendering_engine->get_raw_device();
	this->kill                = kill;
	this->error_message       = &error_message;
	reconnect_requested       = reconnect;
	this->input               = input;
	this->chat_backend        = chat_backend;
	simple_singleplayer_mode  = start_data.isSinglePlayer();

	input->reloadKeybindings();

	driver = device->getVideoDriver();
	smgr = m_rendering_engine->get_scene_manager();

	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, g_settings->getBool("mip_map"));

	// Reinit runData
	runData = GameRunData();
	runData.time_from_last_punch = 10.0;

	m_game_ui->initFlags();
	if (g_settings->getBool("show_debug")) {
		m_flags.debug_state = 1;
		m_game_ui->m_flags.show_minimal_debug = true;
	}

	m_first_loop_after_window_activation = true;

	g_client_translations->clear();

	if (!init(start_data.world_spec.path, start_data.address,
			start_data.socket_port, start_data.game_spec))
		return false;

	if (!createClient(start_data))
		return false;

	m_rendering_engine->initialize(client, hud);

	m_game_formspec.init(client, m_rendering_engine, input);

	return true;
}


void Game::run()
{
	ZoneScoped;

	ProfilerGraph graph;
	RunStats stats = {};
	CameraOrientation cam_view_target = {};
	CameraOrientation cam_view = {};
	FpsControl draw_times;
	f32 dtime; // in seconds

	// Clear the profiler
	{
		Profiler::GraphValues dummyvalues;
		g_profiler->graphPop(dummyvalues);
	}

	draw_times.reset();

	set_light_table(g_settings->getFloat("display_gamma"));

	m_touch_simulate_aux1 = g_settings->getBool("fast_move")
			&& client->checkPrivilege("fast");

	const core::dimension2du initial_screen_size(
			g_settings->getU16("screen_w"),
			g_settings->getU16("screen_h")
		);
	const bool initial_window_maximized = !g_settings->getBool("fullscreen") &&
			g_settings->getBool("window_maximized");

#ifdef __ANDROID__
	porting::setPlayingNowNotification(true);
#endif

	auto framemarker = FrameMarker("Game::run()-frame").started();

	while (m_rendering_engine->run()
			&& !(*kill || g_gamecallback->shutdown_requested
			|| (server && server->isShutdownRequested()))) {

		framemarker.end();

		// Calculate dtime =
		//    m_rendering_engine->run() from this iteration
		//  + Sleep time until the wanted FPS are reached
		draw_times.limit(device, &dtime);

		framemarker.start();

		g_fontengine->handleReload();

		const auto current_dynamic_info = ClientDynamicInfo::getCurrent();
		if (!current_dynamic_info.equal(client_display_info)) {
			client_display_info = current_dynamic_info;
			dynamic_info_send_timer = 0.2f;
		}

		if (dynamic_info_send_timer > 0.0f) {
			dynamic_info_send_timer -= dtime;
			if (dynamic_info_send_timer <= 0.0f) {
				client->sendUpdateClientInfo(current_dynamic_info);
			}
		}

		// Prepare render data for next iteration

		updateStats(&stats, draw_times, dtime);
		updateInteractTimers(dtime);

		if (!checkConnection())
			break;
		if (!m_game_formspec.handleCallbacks())
			break;

		processQueues();

		m_game_ui->clearInfoText();

		updateProfilers(stats, draw_times, dtime);

		// Update camera offset once before doing anything.
		// In contrast to other updates the latency of this doesn't matter,
		// since it's invisible to the user. But it needs to be consistent.
		updateCameraOffset();

		processUserInput(dtime);
		// Update camera before player movement to avoid camera lag of one frame
		updateCameraDirection(&cam_view_target, dtime);
		cam_view.camera_yaw += (cam_view_target.camera_yaw -
				cam_view.camera_yaw) * m_cache_cam_smoothing;
		cam_view.camera_pitch += (cam_view_target.camera_pitch -
				cam_view.camera_pitch) * m_cache_cam_smoothing;
		updatePlayerControl(cam_view);

		updatePauseState();
		if (m_is_paused)
			dtime = 0.0f;

		step(dtime);

		processClientEvents(&cam_view_target);
		updateDebugState();
		// Update camera here so it is in-sync with CAO position
		updateCamera(dtime);
		updateSound(dtime);
		processPlayerInteraction(dtime, m_game_ui->m_flags.show_hud);
		updateFrame(&graph, &stats, dtime, cam_view);
		updateProfilerGraphs(&graph);

		if (m_does_lost_focus_pause_game && !device->isWindowFocused() && !isMenuActive()) {
			m_game_formspec.showPauseMenu();
		}
	}

	framemarker.end();

#ifdef __ANDROID__
	porting::setPlayingNowNotification(false);
#endif

	RenderingEngine::autosaveScreensizeAndCo(initial_screen_size, initial_window_maximized);
}


void Game::shutdown()
{
	// Delete text and menus first
	m_game_ui->clearText();
	m_game_formspec.reset();
	while (g_menumgr.menuCount() > 0) {
		g_menumgr.deleteFront();
	}

	if (g_touchcontrols)
		g_touchcontrols->hide();

	clouds.reset();

	gui_chat_console.reset();

	sky.reset();

	// only if the shutdown progress bar isn't shown yet
	if (m_shutdown_progress == 0.0f)
		showOverlayMessage(N_("Shutting down..."), 0, 0);

	chat_backend->addMessage(L"", L"# Disconnected.");
	chat_backend->addMessage(L"", L"");

	if (client) {
		client->Stop();
		while (!client->isShutdown()) {
			assert(texture_src != NULL);
			assert(shader_src != NULL);
			texture_src->processQueue();
			shader_src->processQueue();
			sleep_ms(100);
		}
	}

	delete client;
	client = nullptr;
	delete soundmaker;
	soundmaker = nullptr;
	sound_manager.reset();

	auto stop_thread = runInThread([=] {
		delete server;
		server = nullptr;
	}, "ServerStop");

	FpsControl fps_control;
	fps_control.reset();

	while (stop_thread->isRunning()) {
		m_rendering_engine->run();
		f32 dtime;
		fps_control.limit(device, &dtime);
		showOverlayMessage(N_("Shutting down..."), dtime, 0, &m_shutdown_progress);
	}

	stop_thread->rethrow();

	// to be continued in Game::~Game
}


/****************************************************************************/
/****************************************************************************
 Startup
 ****************************************************************************/
/****************************************************************************/

bool Game::init(
		const std::string &map_dir,
		const std::string &address,
		u16 port,
		const SubgameSpec &gamespec)
{
	texture_src = createTextureSource();

	showOverlayMessage(N_("Loading..."), 0, 0);

	shader_src = createShaderSource();

	itemdef_manager = createItemDefManager();
	nodedef_manager = createNodeDefManager();

	m_item_visuals_manager = std::make_unique<ItemVisualsManager>();

	eventmgr = new EventManager();
	quicktune = new QuicktuneShortcutter();

	if (!(texture_src && shader_src && itemdef_manager && nodedef_manager
			&& eventmgr && quicktune))
		return false;

	if (!initSound())
		return false;

	// Create a server if not connecting to an existing one
	if (address.empty()) {
		if (!createServer(map_dir, gamespec, port))
			return false;
	}

	return true;
}

bool Game::initSound()
{
#if USE_SOUND
	if (g_sound_manager_singleton.get()) {
		infostream << "Attempting to use OpenAL audio" << std::endl;
		sound_manager = createOpenALSoundManager(g_sound_manager_singleton.get(),
				std::make_unique<SoundFallbackPathProvider>());
		if (!sound_manager)
			infostream << "Failed to initialize OpenAL audio" << std::endl;
	} else {
		infostream << "Sound disabled." << std::endl;
	}
#endif

	if (!sound_manager) {
		infostream << "Using dummy audio." << std::endl;
		sound_manager = std::make_unique<DummySoundManager>();
	}

	soundmaker = new SoundMaker(sound_manager.get(), nodedef_manager);
	if (!soundmaker)
		return false;

	soundmaker->registerReceiver(eventmgr);

	return true;
}

bool Game::createServer(const std::string &map_dir,
		const SubgameSpec &gamespec, u16 port)
{
	showOverlayMessage(N_("Creating server..."), 0, 5);

	std::string bind_str;
	if (simple_singleplayer_mode) {
		// Make the simple singleplayer server only accept connections from localhost,
		// which also makes Windows Defender not show a warning.
		bind_str = "127.0.0.1";
	} else {
		bind_str = g_settings->get("bind_address");
	}

	Address bind_addr(0, 0, 0, 0, port);

	if (g_settings->getBool("ipv6_server"))
		bind_addr.setAddress(static_cast<IPv6AddressBytes*>(nullptr));
	try {
		bind_addr.Resolve(bind_str.c_str());
	} catch (const ResolveError &e) {
		warningstream << "Resolving bind address \"" << bind_str
			<< "\" failed: " << e.what()
			<< " -- Listening on all addresses." << std::endl;
	}
	if (bind_addr.isIPv6() && !g_settings->getBool("enable_ipv6")) {
		*error_message = fmtgettext("Unable to listen on %s because IPv6 is disabled",
			bind_addr.serializeString().c_str());
		errorstream << *error_message << std::endl;
		return false;
	}

	server = new Server(map_dir, gamespec, simple_singleplayer_mode, bind_addr,
			false, nullptr, error_message);

	auto start_thread = runInThread([=] {
		server->start();
		copyServerClientCache();
	}, "ServerStart");

	input->clear();
	bool success = true;

	FpsControl fps_control;
	fps_control.reset();

	while (start_thread->isRunning()) {
		if (!m_rendering_engine->run() || input->cancelPressed())
			success = false;
		f32 dtime;
		fps_control.limit(device, &dtime);

		if (success)
			showOverlayMessage(N_("Creating server..."), dtime, 5);
		else
			showOverlayMessage(N_("Shutting down..."), dtime, 0, &m_shutdown_progress);
	}

	start_thread->rethrow();

	return success;
}

void Game::copyServerClientCache()
{
	// It would be possible to let the client directly read the media files
	// from where the server knows they are. But aside from being more complicated
	// it would also *not* fill the media cache and cause slower joining of
	// remote servers.
	// (Imagine that you launch a game once locally and then connect to a server.)

	assert(server);
	auto map = server->getMediaList();
	u32 n = 0;
	for (auto &it : map) {
		assert(it.first.size() == 20); // SHA1
		if (clientMediaUpdateCacheCopy(it.first, it.second))
			n++;
	}
	infostream << "Copied " << n << " files directly from server to client cache"
		<< std::endl;
}

bool Game::createClient(const GameStartData &start_data)
{
	showOverlayMessage(N_("Creating client..."), 0, 10);

	draw_control = new MapDrawControl();
	if (!draw_control)
		return false;

	bool could_connect, connect_aborted;
	if (!connectToServer(start_data, &could_connect, &connect_aborted))
		return false;

	if (!could_connect) {
		if (error_message->empty() && !connect_aborted) {
			// Should not happen if error messages are set properly
			*error_message = gettext("Connection failed for unknown reason");
			errorstream << *error_message << std::endl;
		}
		return false;
	}

	if (!getServerContent(&connect_aborted)) {
		if (error_message->empty() && !connect_aborted) {
			// Should not happen if error messages are set properly
			*error_message = gettext("Connection failed for unknown reason");
			errorstream << *error_message << std::endl;
		}
		return false;
	}

	shader_src->addShaderConstantSetter(
		std::make_unique<NodeShaderConstantSetter>());

	auto scsf_up = std::make_unique<GameGlobalShaderUniformSetterFactory>(client);
	auto* scsf = scsf_up.get();
	shader_src->addShaderUniformSetterFactory(std::move(scsf_up));

	shader_src->addShaderUniformSetterFactory(
		std::make_unique<FogShaderUniformSetterFactory>());

	ShadowRenderer::preInit(shader_src);

	// Update cached textures, meshes and materials
	client->afterContentReceived();

	/* Camera
	 */
	camera = new Camera(*draw_control, client, m_rendering_engine);
	if (client->modsLoaded())
		client->getScript()->on_camera_ready(camera);
	client->setCamera(camera);

	/* Clouds
	 */
	clouds = make_irr<Clouds>(smgr, shader_src, -1, myrand());

	/* Skybox
	 */
	sky = make_irr<Sky>(-1, m_rendering_engine, texture_src, shader_src);
	scsf->setSky(sky.get());

	/* Pre-calculated values
	 */
	video::ITexture *t = texture_src->getTexture("crack_anylength.png");
	if (t) {
		v2u32 size = t->getOriginalSize();
		crack_animation_length = size.Y / size.X;
	} else {
		crack_animation_length = 5;
	}

	if (!initGui())
		return false;

	/* Set window caption
	 */
	auto driver_name = driver->getName();
	std::string str = std::string(PROJECT_NAME_C) +
			" " + g_version_hash + " [";
	str += simple_singleplayer_mode ? gettext("Singleplayer")
			: gettext("Multiplayer");
	str += "] [";
	str += driver_name;
	str += "]";

	device->setWindowCaption(utf8_to_wide(str).c_str());

	LocalPlayer *player = client->getEnv().getLocalPlayer();
	player->hurt_tilt_timer = 0;
	player->hurt_tilt_strength = 0;

	hud = new Hud(client, player, &player->inventory);

	mapper = client->getMinimap();

	if (mapper && client->modsLoaded())
		client->getScript()->on_minimap_ready(mapper);

	return true;
}

bool Game::shouldShowTouchControls()
{
	if (!device->supportsTouchEvents())
		return false;

	const std::string &touch_controls = g_settings->get("touch_controls");
	if (touch_controls == "auto")
		return RenderingEngine::getLastPointerType() == PointerType::Touch;
	return is_yes(touch_controls);
}

bool Game::initGui()
{
	m_game_ui->init();

	// Remove stale "recent" chat messages from previous connections
	chat_backend->clearRecentChat();

	// Make sure the size of the recent messages buffer is right
	chat_backend->applySettings();

	// Chat backend and console
	gui_chat_console = make_irr<GUIChatConsole>(guienv, guienv->getRootGUIElement(),
			-1, chat_backend, client, &g_menumgr);

	if (shouldShowTouchControls())
		g_touchcontrols = new TouchControls(device, texture_src);

	return true;
}

bool Game::connectToServer(const GameStartData &start_data,
		bool *connect_ok, bool *connection_aborted)
{
	*connect_ok = false;	// Let's not be overly optimistic
	*connection_aborted = false;
	const auto &address_name = start_data.address;

	showOverlayMessage(N_("Resolving address..."), 0, 15);

	Address connect_address(0, 0, 0, 0, start_data.socket_port);
	Address fallback_address;

	try {
		connect_address.Resolve(address_name.c_str(), &fallback_address);

		if (connect_address.isAny()) {
			// replace with localhost IP
			if (connect_address.isIPv6()) {
				IPv6AddressBytes addr_bytes;
				addr_bytes.bytes[15] = 1;
				connect_address.setAddress(&addr_bytes);
			} else {
				connect_address.setAddress(127, 0, 0, 1);
			}
		}
	} catch (ResolveError &e) {
		*error_message = fmtgettext("Couldn't resolve address: %s", e.what());

		errorstream << *error_message << std::endl;
		return false;
	}

	// this shouldn't normally happen since Address::Resolve() checks for enable_ipv6
	if (g_settings->getBool("enable_ipv6")) {
		// empty
	} else if (connect_address.isIPv6()) {
		*error_message = fmtgettext("Unable to connect to %s because IPv6 is disabled", connect_address.serializeString().c_str());
		errorstream << *error_message << std::endl;
		return false;
	} else if (fallback_address.isIPv6()) {
		fallback_address = Address();
	}

	fallback_address.setPort(connect_address.getPort());
	if (fallback_address.isValid()) {
		infostream << "Resolved two addresses for \"" << address_name
			<< "\" isIPv6[0]=" << connect_address.isIPv6()
			<< " isIPv6[1]=" << fallback_address.isIPv6() << std::endl;
	} else {
		infostream << "Resolved one address for \"" << address_name
			<< "\" isIPv6=" << connect_address.isIPv6() << std::endl;
	}


	try {
		client = new Client(start_data.name.c_str(),
				start_data.password,
				*draw_control, texture_src, shader_src,
				itemdef_manager, nodedef_manager, sound_manager.get(), eventmgr,
				m_rendering_engine,
				m_item_visuals_manager.get(),
				start_data.allow_login_or_register);
	} catch (const BaseException &e) {
		*error_message = fmtgettext("Error creating client: %s", e.what());
		errorstream << *error_message << std::endl;
		return false;
	}

	client->migrateModStorage();
	client->m_simple_singleplayer_mode = simple_singleplayer_mode;
	client->m_internal_server = !!server;

	/*
		Wait for server to accept connection
	*/

	client->connect(connect_address, address_name);

	try {
		input->clear();

		FpsControl fps_control;
		f32 dtime;
		f32 wait_time = 0; // in seconds
		bool did_fallback = false;

		fps_control.reset();

		auto framemarker = FrameMarker("Game::connectToServer()-frame").started();

		while (m_rendering_engine->run()) {

			framemarker.end();
			fps_control.limit(device, &dtime);
			framemarker.start();

			// Update client and server
			step(dtime);

			// End condition
			if (client->getState() == LC_Init) {
				*connect_ok = true;
				break;
			}

			// Break conditions
			if (*connection_aborted)
				break;

			if (client->accessDenied()) {
				*error_message = fmtgettext("Access denied. Reason: %s", client->accessDeniedReason().c_str());
				*reconnect_requested = client->reconnectRequested();
				errorstream << *error_message << std::endl;
				break;
			}

			if (input->cancelPressed()) {
				*connection_aborted = true;
				infostream << "Connect aborted [Escape]" << std::endl;
				break;
			}

			wait_time += dtime;
			if (server) {
				// never time out
			} else if (wait_time > GAME_FALLBACK_TIMEOUT && !did_fallback) {
				if (!client->hasServerReplied() && fallback_address.isValid()) {
					client->connect(fallback_address, address_name);
				}
				did_fallback = true;
			} else if (wait_time > GAME_CONNECTION_TIMEOUT) {
				*error_message = gettext("Connection timed out.");
				errorstream << *error_message << std::endl;
				break;
			}

			// Update status
			showOverlayMessage(N_("Connecting to server..."), dtime, 20);
		}
		framemarker.end();
	} catch (con::PeerNotFoundException &e) {
		warningstream << "This should not happen. Please report a bug." << std::endl;
		return false;
	}

	return true;
}

bool Game::getServerContent(bool *aborted)
{
	input->clear();

	FpsControl fps_control;
	f32 dtime; // in seconds

	fps_control.reset();

	auto framemarker = FrameMarker("Game::getServerContent()-frame").started();
	while (m_rendering_engine->run()) {
		framemarker.end();
		fps_control.limit(device, &dtime);
		framemarker.start();

		// Update client and server
		step(dtime);

		// End condition
		if (client->mediaReceived() && client->itemdefReceived() &&
				client->nodedefReceived()) {
			return true;
		}

		// Error conditions
		if (!checkConnection())
			return false;

		if (client->getState() < LC_Init) {
			*error_message = gettext("Client disconnected");
			errorstream << *error_message << std::endl;
			return false;
		}

		if (input->cancelPressed()) {
			*aborted = true;
			infostream << "Connect aborted [Escape]" << std::endl;
			return false;
		}

		// Display status
		int progress = 25;

		if (!client->itemdefReceived()) {
			progress = 25;
			m_rendering_engine->draw_load_screen(wstrgettext("Item definitions..."),
					guienv, texture_src, dtime, progress);
		} else if (!client->nodedefReceived()) {
			progress = 30;
			m_rendering_engine->draw_load_screen(wstrgettext("Node definitions..."),
					guienv, texture_src, dtime, progress);
		} else {
			std::ostringstream message;
			std::fixed(message);
			message.precision(0);
			float receive = client->mediaReceiveProgress() * 100;
			message << gettext("Media...");
			if (receive > 0)
				message << " " << receive << "%";
			message.precision(2);

			if ((USE_CURL == 0) ||
					(!g_settings->getBool("enable_remote_media_server"))) {
				float cur = client->getCurRate();
				std::string cur_unit = gettext("KiB/s");

				if (cur > 900) {
					cur /= 1024.0;
					cur_unit = gettext("MiB/s");
				}

				message << " (" << cur << ' ' << cur_unit << ")";
			}

			progress = 30 + client->mediaReceiveProgress() * 35 + 0.5;
			m_rendering_engine->draw_load_screen(utf8_to_wide(message.str()), guienv,
				texture_src, dtime, progress);
		}
	}
	framemarker.end();

	*aborted = true;
	infostream << "Connect aborted [device]" << std::endl;
	return false;
}


/****************************************************************************/
/****************************************************************************
 Run
 ****************************************************************************/
/****************************************************************************/

inline void Game::updateInteractTimers(f32 dtime)
{
	if (runData.nodig_delay_timer >= 0)
		runData.nodig_delay_timer -= dtime;

	if (runData.object_hit_delay_timer >= 0)
		runData.object_hit_delay_timer -= dtime;

	runData.time_from_last_punch += dtime;
}


/* returns false if game should exit, otherwise true
 */
inline bool Game::checkConnection()
{
	if (client->accessDenied()) {
		*error_message = fmtgettext("Access denied. Reason: %s", client->accessDeniedReason().c_str());
		*reconnect_requested = client->reconnectRequested();
		errorstream << *error_message << std::endl;
		return false;
	}

	return true;
}

void Game::processQueues()
{
	texture_src->processQueue();
	shader_src->processQueue();
}

void Game::updateDebugState()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	// debug UI and wireframe
	bool has_debug = client->checkPrivilege("debug");
	bool has_basic_debug = has_debug || (player->hud_flags & HUD_FLAG_BASIC_DEBUG);

	if (m_game_ui->m_flags.show_basic_debug) {
		if (!has_basic_debug)
			m_game_ui->m_flags.show_basic_debug = false;
	} else if (m_game_ui->m_flags.show_minimal_debug) {
		if (has_basic_debug)
			m_game_ui->m_flags.show_basic_debug = true;
	}
	if (!has_basic_debug)
		hud->disableBlockBounds();
	if (!has_debug) {
		draw_control->show_wireframe = false;
		smgr->setGlobalDebugData(0, bbox_debug_flag);
		m_flags.disable_camera_update = false;
		m_game_formspec.disableDebugView();
	}

	// noclip
	draw_control->allow_noclip = m_cache_enable_noclip && client->checkPrivilege("noclip");
}

void Game::updateProfilers(const RunStats &stats, const FpsControl &draw_times,
		f32 dtime)
{
	float profiler_print_interval =
			g_settings->getFloat("profiler_print_interval");
	bool print_to_log = true;

	// Update game UI anyway but don't log
	if (profiler_print_interval <= 0) {
		print_to_log = false;
		profiler_print_interval = 3;
	}

	if (profiler_interval.step(dtime, profiler_print_interval)) {
		if (print_to_log) {
			infostream << "Profiler:" << std::endl;
			g_profiler->print(infostream);
		}

		m_game_ui->updateProfiler();
		g_profiler->clear();
	}

	// Update graphs
	g_profiler->graphAdd("Time non-rendering [us]",
		draw_times.busy_time - stats.drawtime);
	g_profiler->graphAdd("Sleep [us]", draw_times.sleep_time);

	g_profiler->graphSet("FPS", 1.0f / dtime);

	auto stats2 = driver->getFrameStats();
	g_profiler->avg("Irr: drawcalls", stats2.Drawcalls);
	if (stats2.Drawcalls > 0)
		g_profiler->avg("Irr: primitives per drawcall",
			stats2.PrimitivesDrawn / float(stats2.Drawcalls));
	g_profiler->avg("Irr: HW buffers uploaded", stats2.HWBuffersUploaded);
	g_profiler->avg("Irr: HW buffers active", stats2.HWBuffersActive);
}

void Game::updateStats(RunStats *stats, const FpsControl &draw_times,
		f32 dtime)
{

	f32 jitter;
	Jitter *jp;

	/* Time average and jitter calculation
	 */
	jp = &stats->dtime_jitter;
	jp->avg = jp->avg * 0.96 + dtime * 0.04;

	jitter = dtime - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;

	jp->counter += dtime;

	if (jp->counter > 0.0) {
		jp->counter -= 3.0;
		jp->max_sample = jp->max;
		jp->max_fraction = jp->max_sample / (jp->avg + 0.001);
		jp->max = 0.0;
	}

	/* Busytime average and jitter calculation
	 */
	jp = &stats->busy_time_jitter;
	jp->avg = jp->avg + draw_times.getBusyMs() * 0.02;

	jitter = draw_times.getBusyMs() - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;
	if (jitter < jp->min)
		jp->min = jitter;

	jp->counter += dtime;

	if (jp->counter > 0.0) {
		jp->counter -= 3.0;
		jp->max_sample = jp->max;
		jp->min_sample = jp->min;
		jp->max = 0.0;
		jp->min = 0.0;
	}
}



/****************************************************************************
 Input handling
 ****************************************************************************/

void Game::processUserInput(f32 dtime)
{
	bool desired = shouldShowTouchControls();
	if (desired && !g_touchcontrols) {
		g_touchcontrols = new TouchControls(device, texture_src);

	} else if (!desired && g_touchcontrols) {
		delete g_touchcontrols;
		g_touchcontrols = nullptr;
	}

	// Reset input if window not active or some menu is active
	if (!device->isWindowActive() || isMenuActive() || guienv->hasFocus(gui_chat_console.get())) {
		if (m_game_focused) {
			m_game_focused = false;
			infostream << "Game lost focus" << std::endl;
			input->releaseAllKeys();
		} else {
			input->clear();
		}

		if (g_touchcontrols)
			g_touchcontrols->hide();

	} else {
		if (g_touchcontrols) {
			/* on touchcontrols step may generate own input events which ain't
			 * what we want in case we just did clear them */
			g_touchcontrols->show();
			g_touchcontrols->step(dtime);
		}

		m_game_focused = true;
	}

	if (!guienv->hasFocus(gui_chat_console.get()) && gui_chat_console->isOpen()
		&& !gui_chat_console->isMyChild(guienv->getFocus()))
	{
		gui_chat_console->closeConsoleAtOnce();
	}

	// Input handler step() (used by the random input generator)
	input->step(dtime);

#ifdef __ANDROID__
	if (!m_game_formspec.handleAndroidUIInput())
		handleAndroidChatInput();
#endif

	// Increase timer for double tap of "keymap_jump"
	if (m_cache_doubletap_jump && runData.jump_timer_up <= 0.2f)
		runData.jump_timer_up += dtime;
	if (m_cache_doubletap_jump && runData.jump_timer_down <= 0.4f)
		runData.jump_timer_down += dtime;

	processKeyInput();
	processItemSelection(&runData.new_playeritem);
}


void Game::processKeyInput()
{
	if (wasKeyDown(KeyType::DROP)) {
		dropSelectedItem(isKeyDown(KeyType::SNEAK));
	} else if (wasKeyDown(KeyType::AUTOFORWARD)) {
		toggleAutoforward();
	} else if (wasKeyDown(KeyType::BACKWARD)) {
		if (g_settings->getBool("continuous_forward"))
			toggleAutoforward();
	} else if (wasKeyDown(KeyType::INVENTORY)) {
		m_game_formspec.showPlayerInventory(nullptr);
	} else if (input->cancelPressed()) {
#ifdef __ANDROID__
		m_android_chat_open = false;
#endif
		if (!gui_chat_console->isOpenInhibited()) {
			m_game_formspec.showPauseMenu();
		}
	} else if (wasKeyDown(KeyType::CHAT)) {
		openConsole(0.2, L"");
	} else if (wasKeyDown(KeyType::CMD)) {
		openConsole(0.2, L"/");
	} else if (wasKeyDown(KeyType::CMD_LOCAL)) {
		if (client->modsLoaded())
			openConsole(0.2, L".");
		else
			m_game_ui->showTranslatedStatusText("Client side scripting is disabled");
	} else if (wasKeyDown(KeyType::CONSOLE)) {
		openConsole(core::clamp(g_settings->getFloat("console_height"), 0.1f, 1.0f));
	} else if (wasKeyDown(KeyType::FREEMOVE)) {
		toggleFreeMove();
	} else if (wasKeyDown(KeyType::JUMP)) {
		toggleFreeMoveAlt();
	} else if (wasKeyDown(KeyType::PITCHMOVE)) {
		togglePitchMove();
	} else if (wasKeyDown(KeyType::FASTMOVE)) {
		toggleFast();
	} else if (wasKeyDown(KeyType::NOCLIP)) {
		toggleNoClip();
#if USE_SOUND
	} else if (wasKeyDown(KeyType::MUTE)) {
		bool new_mute_sound = !g_settings->getBool("mute_sound");
		g_settings->setBool("mute_sound", new_mute_sound);
		if (new_mute_sound)
			m_game_ui->showTranslatedStatusText("Sound muted");
		else
			m_game_ui->showTranslatedStatusText("Sound unmuted");
	} else if (wasKeyDown(KeyType::INC_VOLUME)) {
		float new_volume = g_settings->getFloat("sound_volume", 0.0f, 0.9f) + 0.1f;
		g_settings->setFloat("sound_volume", new_volume);
		std::wstring msg = fwgettext("Volume changed to %d%%", myround(new_volume * 100));
		m_game_ui->showStatusText(msg);
	} else if (wasKeyDown(KeyType::DEC_VOLUME)) {
		float new_volume = g_settings->getFloat("sound_volume", 0.1f, 1.0f) - 0.1f;
		g_settings->setFloat("sound_volume", new_volume);
		std::wstring msg = fwgettext("Volume changed to %d%%", myround(new_volume * 100));
		m_game_ui->showStatusText(msg);
#else
	} else if (wasKeyDown(KeyType::MUTE) || wasKeyDown(KeyType::INC_VOLUME)
			|| wasKeyDown(KeyType::DEC_VOLUME)) {
		m_game_ui->showTranslatedStatusText("Sound system is not supported on this build");
#endif
	} else if (wasKeyDown(KeyType::CINEMATIC)) {
		toggleCinematic();
	} else if (wasKeyPressed(KeyType::SCREENSHOT)) {
		client->makeScreenshot();
	} else if (wasKeyPressed(KeyType::TOGGLE_BLOCK_BOUNDS)) {
		toggleBlockBounds();
	} else if (wasKeyPressed(KeyType::TOGGLE_HUD)) {
		m_game_ui->toggleHud();
	} else if (wasKeyPressed(KeyType::MINIMAP)) {
		toggleMinimap(isKeyDown(KeyType::SNEAK));
	} else if (wasKeyPressed(KeyType::TOGGLE_CHAT)) {
		m_game_ui->toggleChat(client);
	} else if (wasKeyPressed(KeyType::TOGGLE_FOG)) {
		toggleFog();
	} else if (wasKeyDown(KeyType::TOGGLE_UPDATE_CAMERA)) {
		toggleUpdateCamera();
	} else if (wasKeyPressed(KeyType::CAMERA_MODE)) {
		camera->toggleCameraMode();
		updateCameraMode();
	} else if (wasKeyPressed(KeyType::TOGGLE_DEBUG)) {
		toggleDebug();
	} else if (wasKeyPressed(KeyType::TOGGLE_PROFILER)) {
		m_game_ui->toggleProfiler();
	} else if (wasKeyDown(KeyType::INCREASE_VIEWING_RANGE)) {
		increaseViewRange();
	} else if (wasKeyDown(KeyType::DECREASE_VIEWING_RANGE)) {
		decreaseViewRange();
	} else if (wasKeyPressed(KeyType::RANGESELECT)) {
		toggleFullViewRange();
	} else if (wasKeyDown(KeyType::ZOOM)) {
		checkZoomEnabled();
	} else if (wasKeyDown(KeyType::QUICKTUNE_NEXT)) {
		quicktune->next();
	} else if (wasKeyDown(KeyType::QUICKTUNE_PREV)) {
		quicktune->prev();
	} else if (wasKeyDown(KeyType::QUICKTUNE_INC)) {
		quicktune->inc();
	} else if (wasKeyDown(KeyType::QUICKTUNE_DEC)) {
		quicktune->dec();
	}

	if (!isKeyDown(KeyType::JUMP) && runData.reset_jump_timer) {
		runData.reset_jump_timer = false;
		runData.jump_timer_up = 0.0f;
	}

	if (quicktune->hasMessage()) {
		m_game_ui->showStatusText(utf8_to_wide(quicktune->getMessage()));
	}
}

void Game::processItemSelection(u16 *new_playeritem)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	*new_playeritem = player->getWieldIndex();
	u16 max_item = player->getMaxHotbarItemcount();
	if (max_item == 0)
		return;
	max_item -= 1;

	/* Item selection using mouse wheel
	 */
	s32 wheel = input->getMouseWheel();
	if (!m_enable_hotbar_mouse_wheel)
		wheel = 0;
	if (m_invert_hotbar_mouse_wheel)
		wheel *= -1;

	s32 dir = wheel;

	if (wasKeyDown(KeyType::HOTBAR_NEXT))
		dir = -1;

	if (wasKeyDown(KeyType::HOTBAR_PREV))
		dir = 1;

	if (dir < 0)
		*new_playeritem = *new_playeritem < max_item ? *new_playeritem + 1 : 0;
	else if (dir > 0)
		*new_playeritem = *new_playeritem > 0 ? *new_playeritem - 1 : max_item;
	// else dir == 0

	/* Item selection using hotbar slot keys
	 */
	for (u16 i = 0; i <= max_item; i++) {
		if (wasKeyDown((GameKeyType) (KeyType::SLOT_1 + i))) {
			*new_playeritem = i;
			break;
		}
	}

	if (g_touchcontrols) {
		std::optional<u16> selection = g_touchcontrols->getHotbarSelection();
		if (selection)
			*new_playeritem = *selection;
	}

	// Clamp selection again in case it wasn't changed but max_item was
	*new_playeritem = MYMIN(*new_playeritem, max_item);
}


void Game::dropSelectedItem(bool single_item)
{
	IDropAction *a = new IDropAction();
	a->count = single_item ? 1 : 0;
	a->from_inv.setCurrentPlayer();
	a->from_list = "main";
	a->from_i = client->getEnv().getLocalPlayer()->getWieldIndex();
	client->inventoryAction(a);
}

void Game::openConsole(float scale, const wchar_t *line)
{
	assert(scale > 0.0f && scale <= 1.0f);

#ifdef __ANDROID__
	if (!porting::hasPhysicalKeyboardAndroid()) {
		porting::showTextInputDialog("", "", 2);
		m_android_chat_open = true;
	} else {
#endif
	if (gui_chat_console->isOpenInhibited())
		return;
	gui_chat_console->openConsole(scale);
	if (line) {
		gui_chat_console->setCloseOnEnter(true);
		gui_chat_console->replaceAndAddToHistory(line);
	}
#ifdef __ANDROID__
	} // else
#endif
}

#ifdef __ANDROID__
void Game::handleAndroidChatInput()
{
	// It has to be a text input
	if (m_android_chat_open && porting::getLastInputDialogType() == porting::TEXT_INPUT) {
		porting::AndroidDialogState dialogState = porting::getInputDialogState();
		if (dialogState == porting::DIALOG_INPUTTED) {
			std::string text = porting::getInputDialogMessage();
			client->typeChatMessage(utf8_to_wide(text));
		}
		if (dialogState != porting::DIALOG_SHOWN)
			m_android_chat_open = false;
	}
}
#endif

void Game::toggleFreeMove()
{
	bool free_move = !g_settings->getBool("free_move");
	g_settings->set("free_move", bool_to_cstr(free_move));

	if (free_move) {
		if (client->checkPrivilege("fly")) {
			m_game_ui->showTranslatedStatusText("Fly mode enabled");
		} else {
			m_game_ui->showTranslatedStatusText("Fly mode enabled (note: no 'fly' privilege)");
		}
	} else {
		m_game_ui->showTranslatedStatusText("Fly mode disabled");
	}
}

void Game::toggleFreeMoveAlt()
{
	if (!runData.reset_jump_timer) {
		runData.jump_timer_down_before = runData.jump_timer_down;
		runData.jump_timer_down = 0.0f;
	}

	// key down (0.2 s max.), then key up (0.2 s max.), then key down
	if (m_cache_doubletap_jump && runData.jump_timer_up < 0.2f &&
			runData.jump_timer_down_before < 0.4f) // 0.2 + 0.2
		toggleFreeMove();

	runData.reset_jump_timer = true;
}


void Game::togglePitchMove()
{
	bool pitch_move = !g_settings->getBool("pitch_move");
	g_settings->set("pitch_move", bool_to_cstr(pitch_move));

	if (pitch_move) {
		m_game_ui->showTranslatedStatusText("Pitch move mode enabled");
	} else {
		m_game_ui->showTranslatedStatusText("Pitch move mode disabled");
	}
}


void Game::toggleFast()
{
	bool fast_move = !g_settings->getBool("fast_move");
	bool has_fast_privs = client->checkPrivilege("fast");
	g_settings->set("fast_move", bool_to_cstr(fast_move));

	if (fast_move) {
		if (has_fast_privs) {
			m_game_ui->showTranslatedStatusText("Fast mode enabled");
		} else {
			m_game_ui->showTranslatedStatusText("Fast mode enabled (note: no 'fast' privilege)");
		}
	} else {
		m_game_ui->showTranslatedStatusText("Fast mode disabled");
	}

	m_touch_simulate_aux1 = fast_move && has_fast_privs;
}


void Game::toggleNoClip()
{
	bool noclip = !g_settings->getBool("noclip");
	g_settings->set("noclip", bool_to_cstr(noclip));

	if (noclip) {
		if (client->checkPrivilege("noclip")) {
			m_game_ui->showTranslatedStatusText("Noclip mode enabled");
		} else {
			m_game_ui->showTranslatedStatusText("Noclip mode enabled (note: no 'noclip' privilege)");
		}
	} else {
		m_game_ui->showTranslatedStatusText("Noclip mode disabled");
	}
}

void Game::toggleCinematic()
{
	bool cinematic = !g_settings->getBool("cinematic");
	g_settings->set("cinematic", bool_to_cstr(cinematic));

	if (cinematic)
		m_game_ui->showTranslatedStatusText("Cinematic mode enabled");
	else
		m_game_ui->showTranslatedStatusText("Cinematic mode disabled");
}

void Game::toggleBlockBounds()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	if (!(client->checkPrivilege("debug") || (player->hud_flags & HUD_FLAG_BASIC_DEBUG))) {
		m_game_ui->showTranslatedStatusText("Can't show block bounds (disabled by game or mod)");
		return;
	}
	enum Hud::BlockBoundsMode newmode = hud->toggleBlockBounds();
	switch (newmode) {
		case Hud::BLOCK_BOUNDS_OFF:
			m_game_ui->showTranslatedStatusText("Block bounds hidden");
			break;
		case Hud::BLOCK_BOUNDS_CURRENT:
			m_game_ui->showTranslatedStatusText("Block bounds shown for current block");
			break;
		case Hud::BLOCK_BOUNDS_NEAR:
			m_game_ui->showTranslatedStatusText("Block bounds shown for nearby blocks");
			break;
		default:
			break;
	}
}

// Autoforward by toggling continuous forward.
void Game::toggleAutoforward()
{
	bool autorun_enabled = !g_settings->getBool("continuous_forward");
	g_settings->set("continuous_forward", bool_to_cstr(autorun_enabled));

	if (autorun_enabled)
		m_game_ui->showTranslatedStatusText("Automatic forward enabled");
	else
		m_game_ui->showTranslatedStatusText("Automatic forward disabled");
}

void Game::toggleMinimap(bool shift_pressed)
{
	if (!mapper || !m_game_ui->m_flags.show_hud || !g_settings->getBool("enable_minimap"))
		return;

	if (shift_pressed)
		mapper->toggleMinimapShape();
	else
		mapper->nextMode();

	// TODO: When legacy minimap is deprecated, keep only HUD minimap stuff here

	// Not so satisying code to keep compatibility with old fixed mode system
	// -->
	u32 hud_flags = client->getEnv().getLocalPlayer()->hud_flags;

	if (hud_flags & HUD_FLAG_MINIMAP_VISIBLE) {
	// If radar is disabled, try to find a non radar mode or fall back to 0
		if (!(hud_flags & HUD_FLAG_MINIMAP_RADAR_VISIBLE))
			while (mapper->getModeIndex() &&
					mapper->getModeDef().type == MINIMAP_TYPE_RADAR)
				mapper->nextMode();
	}
	// <--
	// End of 'not so satifying code'
	if (hud && hud->hasElementOfType(HUD_ELEM_MINIMAP))
		m_game_ui->showStatusText(utf8_to_wide(mapper->getModeDef().label));
	else
		m_game_ui->showTranslatedStatusText("Minimap currently disabled by game or mod");
}

void Game::toggleFog()
{
	bool flag = !g_settings->getBool("enable_fog");
	g_settings->setBool("enable_fog", flag);
	bool allowed = sky->getFogDistance() < 0 || client->checkPrivilege("debug");
	if (!allowed)
		m_game_ui->showTranslatedStatusText("Fog enabled by game or mod");
	else if (flag)
		m_game_ui->showTranslatedStatusText("Fog enabled");
	else
		m_game_ui->showTranslatedStatusText("Fog disabled");
}


void Game::toggleDebug()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	bool has_debug = client->checkPrivilege("debug");
	bool has_basic_debug = has_debug || (player->hud_flags & HUD_FLAG_BASIC_DEBUG);

	// Initial: No debug info
	// 1x toggle: Debug text
	// 2x toggle: Debug text with profiler graph
	// 3x toggle: Debug text and wireframe (needs "debug" priv)
	// 4x toggle: Debug text and bbox (needs "debug" priv)
	//
	// The debug text can be in 2 modes: minimal and basic.
	// * Minimal: Only technical client info that not gameplay-relevant
	// * Basic: Info that might give gameplay advantage, e.g. pos, angle
	// Basic mode is used when player has the debug HUD flag set,
	// otherwise the Minimal mode is used.

	auto &state = m_flags.debug_state;
	state = (state + 1) % 5;
	if (state >= 3 && !has_debug)
		state = 0;

	m_game_ui->m_flags.show_minimal_debug = state > 0;
	m_game_ui->m_flags.show_basic_debug = state > 0 && has_basic_debug;
	m_game_ui->m_flags.show_profiler_graph = state == 2;
	draw_control->show_wireframe = state == 3;
	smgr->setGlobalDebugData(state == 4 ? bbox_debug_flag : 0,
			state == 4 ? 0 : bbox_debug_flag);

	if (state == 1) {
		m_game_ui->showTranslatedStatusText("Debug info shown");
	} else if (state == 2) {
		m_game_ui->showTranslatedStatusText("Profiler graph shown");
	} else if (state == 3) {
		if (driver->getDriverType() == video::EDT_OGLES2)
			m_game_ui->showTranslatedStatusText("Wireframe not supported by video driver");
		else
			m_game_ui->showTranslatedStatusText("Wireframe shown");
	} else if (state == 4) {
		m_game_ui->showTranslatedStatusText("Bounding boxes shown");
	} else {
		m_game_ui->showTranslatedStatusText("All debug info hidden");
	}
}


void Game::toggleUpdateCamera()
{
	auto &flag = m_flags.disable_camera_update;
	flag = client->checkPrivilege("debug") ? !flag : false;
	if (flag)
		m_game_ui->showTranslatedStatusText("Camera update disabled");
	else
		m_game_ui->showTranslatedStatusText("Camera update enabled");
}


void Game::increaseViewRange()
{
	s16 range = g_settings->getS16("viewing_range");
	s16 range_new = range + 10;
	s16 server_limit = sky->getFogDistance();

	if (range_new >= 4000) {
		range_new = 4000;
		std::wstring msg = server_limit >= 0 && range_new > server_limit ?
				fwgettext("Viewing range changed to %d (the maximum), but limited to %d by game or mod", range_new, server_limit) :
				fwgettext("Viewing range changed to %d (the maximum)", range_new);
		m_game_ui->showStatusText(msg);
	} else {
		std::wstring msg = server_limit >= 0 && range_new > server_limit ?
				fwgettext("Viewing range changed to %d, but limited to %d by game or mod", range_new, server_limit) :
				fwgettext("Viewing range changed to %d", range_new);
		m_game_ui->showStatusText(msg);
	}
	g_settings->set("viewing_range", itos(range_new));
}


void Game::decreaseViewRange()
{
	s16 range = g_settings->getS16("viewing_range");
	s16 range_new = range - 10;
	s16 server_limit = sky->getFogDistance();

	if (range_new <= 20) {
		range_new = 20;
		std::wstring msg = server_limit >= 0 && range_new > server_limit ?
				fwgettext("Viewing changed to %d (the minimum), but limited to %d by game or mod", range_new, server_limit) :
				fwgettext("Viewing changed to %d (the minimum)", range_new);
		m_game_ui->showStatusText(msg);
	} else {
		std::wstring msg = server_limit >= 0 && range_new > server_limit ?
				fwgettext("Viewing range changed to %d, but limited to %d by game or mod", range_new, server_limit) :
				fwgettext("Viewing range changed to %d", range_new);
		m_game_ui->showStatusText(msg);
	}
	g_settings->set("viewing_range", itos(range_new));
}


void Game::toggleFullViewRange()
{
	draw_control->range_all = !draw_control->range_all;
	if (draw_control->range_all) {
		if (sky->getFogDistance() >= 0) {
			m_game_ui->showTranslatedStatusText("Unlimited viewing range enabled, but forbidden by game or mod");
		} else {
			m_game_ui->showTranslatedStatusText("Unlimited viewing range enabled");
		}
	} else {
		m_game_ui->showTranslatedStatusText("Unlimited viewing range disabled");
	}
}


void Game::checkZoomEnabled()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	if (player->getZoomFOV() < 0.001f || player->getFov().fov > 0.0f)
		m_game_ui->showTranslatedStatusText("Zoom currently disabled by game or mod");
}

void Game::updateCameraDirection(CameraOrientation *cam, float dtime)
{
	auto *cur_control = device->getCursorControl();

	/* With CIrrDeviceSDL on Linux and Windows, enabling relative mouse mode
	somehow results in simulated mouse events being generated from touch events,
	although SDL_HINT_MOUSE_TOUCH_EVENTS and SDL_HINT_TOUCH_MOUSE_EVENTS are set to 0.
	Since Minetest has its own code to synthesize mouse events from touch events,
	this results in duplicated input. To avoid that, we don't enable relative
	mouse mode if we're in touchscreen mode. */
	if (cur_control)
		cur_control->setRelativeMode(!g_touchcontrols && !isMenuActive());

	if ((device->isWindowActive() && device->isWindowFocused()
			&& !isMenuActive()) || input->isRandom()) {

		if (cur_control && !input->isRandom()) {
			// Mac OSX gets upset if this is set every frame
			if (cur_control->isVisible())
				cur_control->setVisible(false);
		}

		if (m_first_loop_after_window_activation && !g_touchcontrols) {
			m_first_loop_after_window_activation = false;

			input->setMousePos(driver->getScreenSize().Width / 2,
				driver->getScreenSize().Height / 2);
		} else {
			updateCameraOrientation(cam, dtime);
		}

	} else {
		// Mac OSX gets upset if this is set every frame
		if (cur_control && !cur_control->isVisible())
			cur_control->setVisible(true);

		m_first_loop_after_window_activation = true;
	}
	if (g_touchcontrols)
		m_first_loop_after_window_activation = true;
}

// Get the factor to multiply with sensitivity to get the same mouse/joystick
// responsiveness independently of FOV.
f32 Game::getSensitivityScaleFactor() const
{
	f32 fov_y = client->getCamera()->getFovY();

	// Multiply by a constant such that it becomes 1.0 at 72 degree FOV and
	// 16:9 aspect ratio to minimize disruption of existing sensitivity
	// settings.
	return std::tan(fov_y / 2.0f) * 1.3763819f;
}

void Game::updateCameraOrientation(CameraOrientation *cam, float dtime)
{
	if (g_touchcontrols) {
		// User setting is already applied by TouchControls.
		f32 sens_scale = getSensitivityScaleFactor();
		cam->camera_yaw   += g_touchcontrols->getYawChange()   * sens_scale;
		cam->camera_pitch += g_touchcontrols->getPitchChange() * sens_scale;
	} else {
		v2s32 center(driver->getScreenSize().Width / 2, driver->getScreenSize().Height / 2);
		v2s32 dist = input->getMousePos() - center;

		if (m_invert_mouse || camera->getCameraMode() == CAMERA_MODE_THIRD_FRONT) {
			dist.Y = -dist.Y;
		}

		f32 sens_scale = getSensitivityScaleFactor();
		cam->camera_yaw   -= dist.X * m_cache_mouse_sensitivity * sens_scale;
		cam->camera_pitch += dist.Y * m_cache_mouse_sensitivity * sens_scale;

		if (dist.X != 0 || dist.Y != 0)
			input->setMousePos(center.X, center.Y);
	}

	if (m_cache_enable_joysticks) {
		f32 sens_scale = getSensitivityScaleFactor();
		f32 c = m_cache_joystick_frustum_sensitivity * dtime * sens_scale;
		cam->camera_yaw -= input->joystick.getAxisWithoutDead(JA_FRUSTUM_HORIZONTAL) * c;
		cam->camera_pitch += input->joystick.getAxisWithoutDead(JA_FRUSTUM_VERTICAL) * c;
	}

	cam->camera_pitch = rangelim(cam->camera_pitch, -90, 90);
}


// Get the state of an optionally togglable key
bool Game::getTogglableKeyState(GameKeyType key, bool toggling_enabled, bool prev_key_state)
{
	if (!toggling_enabled)
		return isKeyDown(key);
	else
		return prev_key_state ^ wasKeyPressed(key);
}


void Game::updatePlayerControl(const CameraOrientation &cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	// In free move (fly), the "toggle_sneak_key" setting would prevent precise
	// up/down movements. Hence, enable the feature only during 'normal' movement.
	const bool allow_sneak_toggle = m_cache_toggle_sneak_key &&
		!player->getPlayerSettings().free_move;

	//TimeTaker tt("update player control", NULL, PRECISION_NANO);

	PlayerControl control(
		isKeyDown(KeyType::FORWARD),
		isKeyDown(KeyType::BACKWARD),
		isKeyDown(KeyType::LEFT),
		isKeyDown(KeyType::RIGHT),
		isKeyDown(KeyType::JUMP) || player->getAutojump(),
		getTogglableKeyState(KeyType::AUX1,  m_cache_toggle_aux1_key, player->control.aux1),
		getTogglableKeyState(KeyType::SNEAK, allow_sneak_toggle,      player->control.sneak),
		isKeyDown(KeyType::ZOOM),
		isKeyDown(KeyType::DIG),
		isKeyDown(KeyType::PLACE),
		cam.camera_pitch,
		cam.camera_yaw,
		input->getJoystickSpeed(),
		input->getJoystickDirection()
	);
	control.setMovementFromKeys();

	// autoforward if set: move at maximum speed
	if (player->getPlayerSettings().continuous_forward &&
			client->activeObjectsReceived() && !player->isDead()) {
		control.movement_speed = 1.0f;
		// sideways movement only
		float dx = std::sin(control.movement_direction);
		control.movement_direction = std::atan2(dx, 1.0f);
	}

	/* For touch, simulate holding down AUX1 (fast move) if the user has
	 * the fast_move setting toggled on. If there is an aux1 key defined for
	 * touch then its meaning is inverted (i.e. holding aux1 means walk and
	 * not fast)
	 */
	if (g_touchcontrols && m_touch_simulate_aux1) {
		control.aux1 = control.aux1 ^ true;
	}

	client->setPlayerControl(control);

	//tt.stop();
}

void Game::updatePauseState()
{
	bool was_paused = this->m_is_paused;
	this->m_is_paused = this->simple_singleplayer_mode && g_menumgr.pausesGame();

	if (!was_paused && this->m_is_paused) {
		this->pauseAnimation();
		this->sound_manager->pauseAll();
	} else if (was_paused && !this->m_is_paused) {
		this->resumeAnimation();
		this->sound_manager->resumeAll();
	}
}


inline void Game::step(f32 dtime)
{
	ZoneScoped;

	if (server) {
		float fps_max = !device->isWindowFocused() && simple_singleplayer_mode ?
				g_settings->getFloat("fps_max_unfocused") :
				g_settings->getFloat("fps_max");
		fps_max = std::max(fps_max, 1.0f);
		/*
		 * Unless you have a barebones game, running the server at more than 60Hz
		 * is hardly realistic and you're at the point of diminishing returns.
		 * fps_max is also not necessarily anywhere near the FPS actually achieved
		 * (also due to vsync).
		 */
		fps_max = std::min(fps_max, 60.0f);

		server->setStepSettings(Server::StepSettings{
				1.0f / fps_max,
				m_is_paused
			});

		server->step();
	}

	if (!m_is_paused)
		client->step(dtime);
}

static void pauseNodeAnimation(PausedNodesList &paused, scene::ISceneNode *node) {
	if (!node)
		return;
	for (auto &&child: node->getChildren())
		pauseNodeAnimation(paused, child);
	if (node->getType() != scene::ESNT_ANIMATED_MESH)
		return;
	auto animated_node = static_cast<scene::IAnimatedMeshSceneNode *>(node);
	float speed = animated_node->getAnimationSpeed();
	if (!speed)
		return;
	paused.emplace_back(grab(animated_node), speed);
	animated_node->setAnimationSpeed(0.0f);
}

void Game::pauseAnimation()
{
	pauseNodeAnimation(paused_animated_nodes, smgr->getRootSceneNode());
}

void Game::resumeAnimation()
{
	for (auto &&pair: paused_animated_nodes)
		pair.first->setAnimationSpeed(pair.second);
	paused_animated_nodes.clear();
}

const ClientEventHandler Game::clientEventHandler[CLIENTEVENT_MAX] = {
	{&Game::handleClientEvent_None},
	{&Game::handleClientEvent_PlayerDamage},
	{&Game::handleClientEvent_PlayerForceMove},
	{&Game::handleClientEvent_DeathscreenLegacy},
	{&Game::handleClientEvent_ShowFormSpec},
	{&Game::handleClientEvent_ShowCSMFormSpec},
	{&Game::handleClientEvent_ShowPauseMenuFormSpec},
	{&Game::handleClientEvent_HandleParticleEvent},
	{&Game::handleClientEvent_HandleParticleEvent},
	{&Game::handleClientEvent_HandleParticleEvent},
	{&Game::handleClientEvent_HudAdd},
	{&Game::handleClientEvent_HudRemove},
	{&Game::handleClientEvent_HudChange},
	{&Game::handleClientEvent_SetSky},
	{&Game::handleClientEvent_SetSun},
	{&Game::handleClientEvent_SetMoon},
	{&Game::handleClientEvent_SetStars},
	{&Game::handleClientEvent_OverrideDayNigthRatio},
	{&Game::handleClientEvent_CloudParams},
	{&Game::handleClientEvent_UpdateCamera},
};

void Game::handleClientEvent_None(ClientEvent *event, CameraOrientation *cam)
{
	FATAL_ERROR("ClientEvent type None received");
}

void Game::handleClientEvent_PlayerDamage(ClientEvent *event, CameraOrientation *cam)
{
	if (client->modsLoaded())
		client->getScript()->on_damage_taken(event->player_damage.amount);

	if (!event->player_damage.effect)
		return;

	// Damage flash and hurt tilt are not used at death
	if (client->getHP() > 0) {
		LocalPlayer *player = client->getEnv().getLocalPlayer();

		f32 hp_max = player->getCAO() ?
			player->getCAO()->getProperties().hp_max : PLAYER_MAX_HP_DEFAULT;
		f32 damage_ratio = event->player_damage.amount / hp_max;

		runData.damage_flash += 95.0f + 64.f * damage_ratio;
		runData.damage_flash = MYMIN(runData.damage_flash, 127.0f);

		player->hurt_tilt_timer = 1.5f;
		player->hurt_tilt_strength =
			rangelim(damage_ratio * 5.0f, 1.0f, 4.0f);
	}

	// Play damage sound
	client->getEventManager()->put(new SimpleTriggerEvent(MtEvent::PLAYER_DAMAGE));
}

void Game::handleClientEvent_PlayerForceMove(ClientEvent *event, CameraOrientation *cam)
{
	cam->camera_yaw = event->player_force_move.yaw;
	cam->camera_pitch = event->player_force_move.pitch;
}

void Game::handleClientEvent_DeathscreenLegacy(ClientEvent *event, CameraOrientation *cam)
{
	m_game_formspec.showDeathFormspecLegacy();
}

void Game::handleClientEvent_ShowFormSpec(ClientEvent *event, CameraOrientation *cam)
{
	auto &fs = event->show_formspec;

	if (fs.formname->empty() && !fs.formspec->empty()) {
		m_game_formspec.showPlayerInventory(fs.formspec);
	} else {
		m_game_formspec.showFormSpec(*fs.formspec, *fs.formname);
	}

	delete fs.formspec;
	delete fs.formname;
}

void Game::handleClientEvent_ShowCSMFormSpec(ClientEvent *event, CameraOrientation *cam)
{
	m_game_formspec.showCSMFormSpec(*event->show_formspec.formspec,
		*event->show_formspec.formname);

	delete event->show_formspec.formspec;
	delete event->show_formspec.formname;
}

void Game::handleClientEvent_ShowPauseMenuFormSpec(ClientEvent *event, CameraOrientation *cam)
{
	m_game_formspec.showPauseMenuFormSpec(*event->show_formspec.formspec,
		*event->show_formspec.formname);

	delete event->show_formspec.formspec;
	delete event->show_formspec.formname;
}

void Game::handleClientEvent_HandleParticleEvent(ClientEvent *event,
		CameraOrientation *cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	client->getParticleManager()->handleParticleEvent(event, client, player);
}

void Game::handleClientEvent_HudAdd(ClientEvent *event, CameraOrientation *cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	u32 server_id = event->hudadd->server_id;
	// ignore if we already have a HUD with that ID
	auto i = m_hud_server_to_client.find(server_id);
	if (i != m_hud_server_to_client.end()) {
		delete event->hudadd;
		return;
	}

	HudElement *e = new HudElement;
	e->type   = static_cast<HudElementType>(event->hudadd->type);
	e->pos    = event->hudadd->pos;
	e->name   = event->hudadd->name;
	e->scale  = event->hudadd->scale;
	e->text   = event->hudadd->text;
	e->number = event->hudadd->number;
	e->item   = event->hudadd->item;
	e->dir    = event->hudadd->dir;
	e->align  = event->hudadd->align;
	e->offset = event->hudadd->offset;
	e->world_pos = event->hudadd->world_pos;
	e->size      = event->hudadd->size;
	e->z_index   = event->hudadd->z_index;
	e->text2     = event->hudadd->text2;
	e->style     = event->hudadd->style;
	m_hud_server_to_client[server_id] = player->addHud(e);

	delete event->hudadd;
}

void Game::handleClientEvent_HudRemove(ClientEvent *event, CameraOrientation *cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	auto i = m_hud_server_to_client.find(event->hudrm.id);
	if (i != m_hud_server_to_client.end()) {
		HudElement *e = player->removeHud(i->second);
		delete e;
		m_hud_server_to_client.erase(i);
	}

}

void Game::handleClientEvent_HudChange(ClientEvent *event, CameraOrientation *cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	HudElement *e = nullptr;

	auto i = m_hud_server_to_client.find(event->hudchange->id);
	if (i != m_hud_server_to_client.end()) {
		e = player->getHud(i->second);
	}

	if (e == nullptr) {
		delete event->hudchange;
		return;
	}

#define CASE_SET(statval, prop, dataprop) \
	case statval: \
		e->prop = event->hudchange->dataprop; \
		break

	switch (event->hudchange->stat) {
		CASE_SET(HUD_STAT_POS, pos, v2fdata);

		CASE_SET(HUD_STAT_NAME, name, sdata);

		CASE_SET(HUD_STAT_SCALE, scale, v2fdata);

		CASE_SET(HUD_STAT_TEXT, text, sdata);

		CASE_SET(HUD_STAT_NUMBER, number, data);

		CASE_SET(HUD_STAT_ITEM, item, data);

		CASE_SET(HUD_STAT_DIR, dir, data);

		CASE_SET(HUD_STAT_ALIGN, align, v2fdata);

		CASE_SET(HUD_STAT_OFFSET, offset, v2fdata);

		CASE_SET(HUD_STAT_WORLD_POS, world_pos, v3fdata);

		CASE_SET(HUD_STAT_SIZE, size, v2s32data);

		CASE_SET(HUD_STAT_Z_INDEX, z_index, data);

		CASE_SET(HUD_STAT_TEXT2, text2, sdata);

		CASE_SET(HUD_STAT_STYLE, style, data);

		case HudElementStat_END:
			break;
	}

#undef CASE_SET

	delete event->hudchange;
}

void Game::handleClientEvent_SetSky(ClientEvent *event, CameraOrientation *cam)
{
	sky->setVisible(false);
	// Whether clouds are visible in front of a custom skybox.
	sky->setCloudsEnabled(event->set_sky->clouds);

	// Clear the old textures out in case we switch rendering type.
	sky->clearSkyboxTextures();
	// Handle according to type
	if (event->set_sky->type == "regular") {
		// Shows the mesh skybox
		sky->setVisible(true);
		// Update mesh based skybox colours if applicable.
		sky->setSkyColors(event->set_sky->sky_color);
		sky->setHorizonTint(
			event->set_sky->fog_sun_tint,
			event->set_sky->fog_moon_tint,
			event->set_sky->fog_tint_type
		);
	} else if (event->set_sky->type == "skybox" &&
			event->set_sky->textures.size() == 6) {
		// Disable the dyanmic mesh skybox:
		sky->setVisible(false);
		// Set fog colors:
		sky->setFallbackBgColor(event->set_sky->bgcolor);
		// Set sunrise and sunset fog tinting:
		sky->setHorizonTint(
			event->set_sky->fog_sun_tint,
			event->set_sky->fog_moon_tint,
			event->set_sky->fog_tint_type
		);
		// Add textures to skybox.
		for (int i = 0; i < 6; i++)
			sky->addTextureToSkybox(event->set_sky->textures[i], i, texture_src);
	} else {
		// Handle everything else as plain color.
		if (event->set_sky->type != "plain")
			infostream << "Unknown sky type: "
				<< (event->set_sky->type) << std::endl;
		sky->setVisible(false);
		sky->setFallbackBgColor(event->set_sky->bgcolor);
		// Disable directional sun/moon tinting on plain or invalid skyboxes.
		sky->setHorizonTint(
			event->set_sky->bgcolor,
			event->set_sky->bgcolor,
			"custom"
		);
	}

	// Orbit Tilt:
	sky->setBodyOrbitTilt(event->set_sky->body_orbit_tilt);

	// fog
	// do not override a potentially smaller client setting.
	sky->setFogDistance(event->set_sky->fog_distance);

	// if the fog distance is reset, switch back to the client's viewing_range
	if (event->set_sky->fog_distance < 0)
		draw_control->wanted_range = g_settings->getS16("viewing_range");

	if (event->set_sky->fog_start >= 0)
		sky->setFogStart(rangelim(event->set_sky->fog_start, 0.0f, 0.99f));
	else
		sky->setFogStart(rangelim(g_settings->getFloat("fog_start"), 0.0f, 0.99f));

	sky->setFogColor(event->set_sky->fog_color);

	delete event->set_sky;
}

void Game::handleClientEvent_SetSun(ClientEvent *event, CameraOrientation *cam)
{
	sky->setSunVisible(event->sun_params->visible);
	sky->setSunTexture(event->sun_params->texture,
		event->sun_params->tonemap, texture_src);
	sky->setSunScale(event->sun_params->scale);
	sky->setSunriseVisible(event->sun_params->sunrise_visible);
	sky->setSunriseTexture(event->sun_params->sunrise, texture_src);
	delete event->sun_params;
}

void Game::handleClientEvent_SetMoon(ClientEvent *event, CameraOrientation *cam)
{
	sky->setMoonVisible(event->moon_params->visible);
	sky->setMoonTexture(event->moon_params->texture,
		event->moon_params->tonemap, texture_src);
	sky->setMoonScale(event->moon_params->scale);
	delete event->moon_params;
}

void Game::handleClientEvent_SetStars(ClientEvent *event, CameraOrientation *cam)
{
	sky->setStarsVisible(event->star_params->visible);
	sky->setStarCount(event->star_params->count);
	sky->setStarColor(event->star_params->starcolor);
	sky->setStarScale(event->star_params->scale);
	sky->setStarDayOpacity(event->star_params->day_opacity);
	delete event->star_params;
}

void Game::handleClientEvent_OverrideDayNigthRatio(ClientEvent *event,
		CameraOrientation *cam)
{
	client->getEnv().setDayNightRatioOverride(
		event->override_day_night_ratio.do_override,
		event->override_day_night_ratio.ratio_f * 1000.0f);
}

void Game::handleClientEvent_CloudParams(ClientEvent *event, CameraOrientation *cam)
{
	clouds->setDensity(event->cloud_params.density);
	clouds->setColorBright(video::SColor(event->cloud_params.color_bright));
	clouds->setColorAmbient(video::SColor(event->cloud_params.color_ambient));
	clouds->setColorShadow(video::SColor(event->cloud_params.color_shadow));
	clouds->setHeight(event->cloud_params.height);
	clouds->setThickness(event->cloud_params.thickness);
	clouds->setSpeed(v2f(event->cloud_params.speed_x, event->cloud_params.speed_y));
}

void Game::handleClientEvent_UpdateCamera(ClientEvent *event, CameraOrientation *cam)
{
	// no parameters to update here, this just makes sure the camera is in the
	// state it should be after something was changed.
	updateCameraMode();
}

void Game::processClientEvents(CameraOrientation *cam)
{
	while (client->hasClientEvents()) {
		std::unique_ptr<ClientEvent> event(client->getClientEvent());
		FATAL_ERROR_IF(event->type >= CLIENTEVENT_MAX, "Invalid clientevent type");
		const ClientEventHandler& evHandler = clientEventHandler[event->type];
		(this->*evHandler.handler)(event.get(), cam);
	}
}

void Game::updateChat(f32 dtime)
{
	auto color_for = [](LogLevel level) -> const char* {
		switch (level) {
		case LL_ERROR  : return "\x1b(c@#F00)"; // red
		case LL_WARNING: return "\x1b(c@#EE0)"; // yellow
		case LL_INFO   : return "\x1b(c@#BBB)"; // grey
		case LL_VERBOSE: return "\x1b(c@#888)"; // dark grey
		case LL_TRACE  : return "\x1b(c@#888)"; // dark grey
		default        : return "";
		}
	};

	// Get new messages from error log buffer
	std::vector<LogEntry> entries = m_chat_log_buf.take();
	for (const auto& entry : entries) {
		std::string line;
		line.append(color_for(entry.level)).append(entry.combined);
		chat_backend->addMessage(L"", utf8_to_wide(line));
	}

	// Get new messages from client
	std::wstring message;
	while (client->getChatMessage(message)) {
		chat_backend->addUnparsedMessage(message);
	}

	// Remove old messages
	chat_backend->step(dtime);

	// Display all messages in a static text element
	auto &buf = chat_backend->getRecentBuffer();
	if (buf.getLinesModified()) {
		buf.resetLinesModified();
		m_game_ui->setChatText(chat_backend->getRecentChat(), buf.getLineCount());
	}

	// Make sure that the size is still correct
	m_game_ui->updateChatSize();
}

void Game::updateCamera(f32 dtime)
{
	ClientEnvironment &env = client->getEnv();
	LocalPlayer *player = env.getLocalPlayer();

	// For interaction purposes, get info about the held item
	ItemStack playeritem, hand;
	{
		ItemStack selected;
		playeritem = player->getWieldedItem(&selected, &hand);
	}

	ToolCapabilities playeritem_toolcap =
		playeritem.getToolCapabilities(itemdef_manager, &hand);

	float full_punch_interval = playeritem_toolcap.full_punch_interval;
	float tool_reload_ratio = runData.time_from_last_punch / full_punch_interval;

	tool_reload_ratio = std::min(tool_reload_ratio, 1.0f);
	camera->update(player, dtime, tool_reload_ratio);
	camera->step(dtime);

	if (!m_flags.disable_camera_update) {
		client->getEnv().getClientMap().updateCamera(camera->getPosition(),
			camera->getDirection(), camera->getFovMax(), camera->getOffset(),
			player->light_color);
	}
}

void Game::updateCameraMode()
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	// Obey server choice
	if (player->allowed_camera_mode != CAMERA_MODE_ANY)
		camera->setCameraMode(player->allowed_camera_mode);

	GenericCAO *playercao = player->getCAO();
	if (playercao) {
		// Make the player visible depending on camera mode.
		playercao->updateMeshCulling();
		playercao->setChildrenVisible(camera->getCameraMode() > CAMERA_MODE_FIRST);
	}
}

void Game::updateCameraOffset()
{
	ClientEnvironment &env = client->getEnv();

	v3s16 old_camera_offset = camera->getOffset();

	camera->updateOffset();

	v3s16 camera_offset = camera->getOffset();

	m_camera_offset_changed = camera_offset != old_camera_offset;
	if (!m_camera_offset_changed)
		return;

	if (!m_flags.disable_camera_update) {
		auto *shadow = RenderingEngine::get_shadow_renderer();
		if (shadow) {
			shadow->getDirectionalLight().updateCameraOffset(camera);
			// FIXME: I bet we can be smarter about this and don't need to redraw
			// the shadow map at all, but this is for someone else to figure out.
			if (!g_settings->getFlag("performance_tradeoffs"))
				shadow->setForceUpdateShadowMap();
		}

		env.getClientMap().updateCamera(camera->getPosition(),
			camera->getDirection(), camera->getFovMax(), camera_offset,
			env.getLocalPlayer()->light_color);

		env.updateCameraOffset(camera_offset);
		clouds->updateCameraOffset(camera_offset);
	}
}

void Game::updateSound(f32 dtime)
{
	// Update sound listener
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	ClientActiveObject *parent = player->getParent();
	v3s16 camera_offset = camera->getOffset();
	sound_manager->updateListener(
			(1.0f/BS) * camera->getCameraNode()->getPosition()
					+ intToFloat(camera_offset, 1.0f),
			(1.0f/BS) * (parent ? parent->getVelocity() : player->getSpeed()),
			camera->getDirection(),
			camera->getCameraNode()->getUpVector());

	sound_volume_control(sound_manager.get(), device->isWindowActive());

	// Tell the sound maker whether to make footstep sounds
	soundmaker->makes_footstep_sound = player->makes_footstep_sound;

	//	Update sound maker
	if (player->makes_footstep_sound)
		soundmaker->step(dtime);

	ClientMap &map = client->getEnv().getClientMap();
	MapNode n = map.getNode(player->getFootstepNodePos());
	soundmaker->m_player_step_sound = nodedef_manager->get(n).sound_footstep;
}


void Game::processPlayerInteraction(f32 dtime, bool show_hud)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	const v3f camera_direction = camera->getDirection();
	const v3s16 camera_offset  = camera->getOffset();

	/*
		Calculate what block is the crosshair pointing to
	*/

	ItemStack selected_item, hand_item;
	const ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);

	const ItemDefinition &selected_def = tool_item.getDefinition(itemdef_manager);
	f32 d = getToolRange(tool_item, hand_item, itemdef_manager);

	core::line3d<f32> shootline;

	switch (camera->getCameraMode()) {
	case CAMERA_MODE_ANY:
		assert(false);
		break;
	case CAMERA_MODE_FIRST:
		// Shoot from camera position, with bobbing
		shootline.start = camera->getPosition();
		break;
	case CAMERA_MODE_THIRD:
		// Shoot from player head, no bobbing
		shootline.start = camera->getHeadPosition();
		break;
	case CAMERA_MODE_THIRD_FRONT:
		shootline.start = camera->getHeadPosition();
		// prevent player pointing anything in front-view
		d = 0;
		break;
	}
	shootline.end = shootline.start + camera_direction * BS * d;

	if (isTouchShootlineUsed()) {
		shootline = g_touchcontrols->getShootline();
		// Scale shootline to the acual distance the player can reach
		shootline.end = shootline.start +
				shootline.getVector().normalize() * BS * d;
		shootline.start += intToFloat(camera_offset, BS);
		shootline.end += intToFloat(camera_offset, BS);
	}

	PointedThing pointed = updatePointedThing(shootline,
			selected_def.liquids_pointable,
			selected_def.pointabilities,
			!runData.btn_down_for_dig,
			camera_offset);

	if (pointed != runData.pointed_old)
		infostream << "Pointing at " << pointed.dump() << std::endl;

	if (g_touchcontrols) {
		auto mode = selected_def.touch_interaction.getMode(selected_def, pointed.type);
		g_touchcontrols->applyContextControls(mode);
		// applyContextControls may change dig/place input.
		// Update again so that TOSERVER_INTERACT packets have the correct controls set.
		player->control.dig = isKeyDown(KeyType::DIG);
		player->control.place = isKeyDown(KeyType::PLACE);
	}

	// Note that updating the selection mesh every frame is not particularly efficient,
	// but the halo rendering code is already inefficient so there's no point in optimizing it here
	hud->updateSelectionMesh(camera_offset);

	// Allow digging again if button is not pressed
	if (runData.digging_blocked && !isKeyDown(KeyType::DIG))
		runData.digging_blocked = false;

	/*
		Stop digging when
		- releasing dig button
		- pointing away from node
	*/
	if (runData.digging) {
		if (wasKeyReleased(KeyType::DIG)) {
			infostream << "Dig button released (stopped digging)" << std::endl;
			runData.digging = false;
		} else if (pointed != runData.pointed_old) {
			if (pointed.type == POINTEDTHING_NODE
					&& runData.pointed_old.type == POINTEDTHING_NODE
					&& pointed.node_undersurface
							== runData.pointed_old.node_undersurface) {
				// Still pointing to the same node, but a different face.
				// Don't reset.
			} else {
				infostream << "Pointing away from node (stopped digging)" << std::endl;
				runData.digging = false;
				hud->updateSelectionMesh(camera_offset);
			}
		}

		if (!runData.digging) {
			client->interact(INTERACT_STOP_DIGGING, runData.pointed_old);
			client->setCrack(-1, v3s16(0, 0, 0));
			runData.dig_time = 0.0;
		}
	} else if (runData.dig_instantly && wasKeyReleased(KeyType::DIG)) {
		// Remove e.g. torches faster when clicking instead of holding dig button
		runData.nodig_delay_timer = 0;
		runData.dig_instantly = false;
	}

	if (!runData.digging && runData.btn_down_for_dig && !isKeyDown(KeyType::DIG))
		runData.btn_down_for_dig = false;

	runData.punching = false;

	soundmaker->m_player_leftpunch_sound = SoundSpec();
	soundmaker->m_player_leftpunch_sound2 = pointed.type != POINTEDTHING_NOTHING ?
		selected_def.sound_use : selected_def.sound_use_air;

	// Prepare for repeating, unless we're not supposed to
	if (isKeyDown(KeyType::PLACE) && !g_settings->getBool("safe_dig_and_place"))
		runData.repeat_place_timer += dtime;
	else
		runData.repeat_place_timer = 0;

	if (selected_def.usable && isKeyDown(KeyType::DIG)) {
		if (wasKeyPressed(KeyType::DIG) && (!client->modsLoaded() ||
				!client->getScript()->on_item_use(selected_item, pointed)))
			client->interact(INTERACT_USE, pointed);
	} else if (pointed.type == POINTEDTHING_NODE) {
		handlePointingAtNode(pointed, selected_item, hand_item, dtime);
	} else if (pointed.type == POINTEDTHING_OBJECT) {
		v3f player_position  = player->getPosition();
		bool basic_debug_allowed = client->checkPrivilege("debug") || (player->hud_flags & HUD_FLAG_BASIC_DEBUG);
		handlePointingAtObject(pointed, tool_item, hand_item, player_position,
				m_game_ui->m_flags.show_basic_debug && basic_debug_allowed);
	} else if (isKeyDown(KeyType::DIG)) {
		// When button is held down in air, show continuous animation
		runData.punching = true;
		// Run callback even though item is not usable
		if (wasKeyPressed(KeyType::DIG) && client->modsLoaded())
			client->getScript()->on_item_use(selected_item, pointed);
	} else if (wasKeyPressed(KeyType::PLACE)) {
		handlePointingAtNothing(selected_item);
	}

	runData.pointed_old = pointed;

	if (runData.punching || wasKeyPressed(KeyType::DIG))
		camera->setDigging(0); // dig animation

	input->clearWasKeyPressed();
	input->clearWasKeyReleased();
	// Ensure DIG & PLACE are marked as handled
	wasKeyDown(KeyType::DIG);
	wasKeyDown(KeyType::PLACE);

	input->joystick.clearWasKeyPressed(KeyType::DIG);
	input->joystick.clearWasKeyPressed(KeyType::PLACE);

	input->joystick.clearWasKeyReleased(KeyType::DIG);
	input->joystick.clearWasKeyReleased(KeyType::PLACE);
}


PointedThing Game::updatePointedThing(
	const core::line3d<f32> &shootline,
	bool liquids_pointable,
	const std::optional<Pointabilities> &pointabilities,
	bool look_for_object,
	const v3s16 &camera_offset)
{
	std::vector<aabb3f> *selectionboxes = hud->getSelectionBoxes();
	selectionboxes->clear();
	hud->setSelectedFaceNormal(v3f());
	static thread_local const bool show_entity_selectionbox = g_settings->getBool(
		"show_entity_selectionbox");

	ClientEnvironment &env = client->getEnv();
	ClientMap &map = env.getClientMap();
	const NodeDefManager *nodedef = map.getNodeDefManager();

	runData.selected_object = NULL;
	hud->pointing_at_object = false;

	RaycastState s(shootline, look_for_object, liquids_pointable, pointabilities);
	PointedThing result;
	env.continueRaycast(&s, &result);
	if (result.type == POINTEDTHING_OBJECT) {
		hud->pointing_at_object = true;

		runData.selected_object = client->getEnv().getActiveObject(result.object_id);
		aabb3f selection_box{{0.0f, 0.0f, 0.0f}};
		if (show_entity_selectionbox && runData.selected_object->doShowSelectionBox() &&
				runData.selected_object->getSelectionBox(&selection_box)) {
			v3f pos = runData.selected_object->getPosition();
			selectionboxes->push_back(selection_box);
			hud->setSelectionPos(pos, camera_offset);
			GenericCAO* gcao = dynamic_cast<GenericCAO*>(runData.selected_object);
			if (gcao != nullptr && gcao->getProperties().rotate_selectionbox)
				hud->setSelectionRotationRadians(gcao->getSceneNode()
						->getAbsoluteTransformation().getRotationRadians());
			else
				hud->setSelectionRotationRadians(v3f());
		}
		hud->setSelectedFaceNormal(result.raw_intersection_normal);
	} else if (result.type == POINTEDTHING_NODE) {
		// Update selection boxes
		MapNode n = map.getNode(result.node_undersurface);
		std::vector<aabb3f> boxes;
		n.getSelectionBoxes(nodedef, &boxes,
			n.getNeighbors(result.node_undersurface, &map));

		f32 d = 0.002f * BS;
		for (aabb3f box : boxes) {
			box.MinEdge -= v3f(d, d, d);
			box.MaxEdge += v3f(d, d, d);
			selectionboxes->push_back(box);
		}
		hud->setSelectionPos(intToFloat(result.node_undersurface, BS),
			camera_offset);
		hud->setSelectionRotationRadians(v3f());
		hud->setSelectedFaceNormal(result.intersection_normal);
	}

	// Update selection mesh light level and vertex colors
	if (!selectionboxes->empty()) {
		v3f pf = hud->getSelectionPos();
		v3s16 p = floatToInt(pf, BS);

		// Get selection mesh light level
		MapNode n = map.getNode(p);
		u16 node_light = getInteriorLight(n, -1, nodedef);
		u16 light_level = node_light;

		for (const v3s16 &dir : g_6dirs) {
			n = map.getNode(p + dir);
			node_light = getInteriorLight(n, -1, nodedef);
			if (node_light > light_level)
				light_level = node_light;
		}

		u32 daynight_ratio = client->getEnv().getDayNightRatio();
		video::SColor c;
		final_color_blend(&c, light_level, daynight_ratio);

		// Modify final color a bit with time
		u32 timer = client->getEnv().getFrameTime() % 5000;
		float timerf = (float) (core::PI * ((timer / 2500.0) - 0.5));
		float sin_r = 0.08f * std::sin(timerf);
		float sin_g = 0.08f * std::sin(timerf + core::PI * 0.5f);
		float sin_b = 0.08f * std::sin(timerf + core::PI);
		c.setRed(core::clamp(core::round32(c.getRed() * (0.8 + sin_r)), 0, 255));
		c.setGreen(core::clamp(core::round32(c.getGreen() * (0.8 + sin_g)), 0, 255));
		c.setBlue(core::clamp(core::round32(c.getBlue() * (0.8 + sin_b)), 0, 255));

		// Set mesh final color
		hud->setSelectionMeshColor(c);
	}
	return result;
}


void Game::handlePointingAtNothing(const ItemStack &playerItem)
{
	infostream << "Attempted to place item while pointing at nothing" << std::endl;
	PointedThing fauxPointed;
	fauxPointed.type = POINTEDTHING_NOTHING;
	client->interact(INTERACT_ACTIVATE, fauxPointed);
}


void Game::handlePointingAtNode(const PointedThing &pointed,
	const ItemStack &selected_item, const ItemStack &hand_item, f32 dtime)
{
	v3s16 nodepos = pointed.node_undersurface;
	v3s16 neighborpos = pointed.node_abovesurface;

	/*
		Check information text of node
	*/

	ClientMap &map = client->getEnv().getClientMap();

	if (runData.nodig_delay_timer <= 0.0 && isKeyDown(KeyType::DIG)
			&& !runData.digging_blocked
			&& client->checkPrivilege("interact")) {
		handleDigging(pointed, nodepos, selected_item, hand_item, dtime);
	}

	// This should be done after digging handling
	NodeMetadata *meta = map.getNodeMetadata(nodepos);

	if (meta) {
		m_game_ui->setInfoText(unescape_translate(utf8_to_wide(
			meta->getString("infotext"))));
	} else {
		MapNode n = map.getNode(nodepos);

		if (nodedef_manager->get(n).name == "unknown") {
			m_game_ui->setInfoText(L"Unknown node");
		}
	}

	if ((wasKeyPressed(KeyType::PLACE) ||
			runData.repeat_place_timer >= m_repeat_place_time) &&
			client->checkPrivilege("interact")) {
		runData.repeat_place_timer = 0;
		infostream << "Place button pressed while looking at ground" << std::endl;

		// Placing animation (always shown for feedback)
		camera->setDigging(1);

		soundmaker->m_player_rightpunch_sound = SoundSpec();

		// If the wielded item has node placement prediction,
		// make that happen
		// And also set the sound and send the interact
		// But first check for meta formspec and rightclickable
		auto &def = selected_item.getDefinition(itemdef_manager);
		bool placed = nodePlacement(def, selected_item, nodepos, neighborpos,
			pointed, meta);

		if (placed && client->modsLoaded())
			client->getScript()->on_placenode(pointed, def);
	}
}

bool Game::nodePlacement(const ItemDefinition &selected_def,
	const ItemStack &selected_item, const v3s16 &nodepos, const v3s16 &neighborpos,
	const PointedThing &pointed, const NodeMetadata *meta)
{
	const auto &prediction = selected_def.node_placement_prediction;

	const NodeDefManager *nodedef = client->ndef();
	ClientMap &map = client->getEnv().getClientMap();
	MapNode node;
	bool is_valid_position;

	node = map.getNode(nodepos, &is_valid_position);
	if (!is_valid_position) {
		soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
		return false;
	}

	// formspec in meta
	if (meta && !meta->getString("formspec").empty() && !input->isRandom()
			&& !isKeyDown(KeyType::SNEAK)) {
		// on_rightclick callbacks are called anyway
		if (nodedef_manager->get(map.getNode(nodepos)).rightclickable)
			client->interact(INTERACT_PLACE, pointed);

		m_game_formspec.showNodeFormspec(meta->getString("formspec"), nodepos);
		return false;
	}

	// on_rightclick callback
	if (prediction.empty() || (nodedef->get(node).rightclickable &&
			!isKeyDown(KeyType::SNEAK))) {
		// Report to server
		client->interact(INTERACT_PLACE, pointed);
		return false;
	}

	verbosestream << "Node placement prediction for "
		<< selected_def.name << " is " << prediction << std::endl;
	v3s16 p = neighborpos;

	// Place inside node itself if buildable_to
	MapNode n_under = map.getNode(nodepos, &is_valid_position);
	if (is_valid_position) {
		if (nodedef->get(n_under).buildable_to) {
			p = nodepos;
		} else {
			node = map.getNode(p, &is_valid_position);
			if (is_valid_position && !nodedef->get(node).buildable_to) {
				soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
				// Report to server
				client->interact(INTERACT_PLACE, pointed);
				return false;
			}
		}
	}

	// Find id of predicted node
	content_t id;
	bool found = nodedef->getId(prediction, id);

	if (!found) {
		errorstream << "Node placement prediction failed for "
			<< selected_def.name << " (places " << prediction
			<< ") - Name not known" << std::endl;
		// Handle this as if prediction was empty
		// Report to server
		client->interact(INTERACT_PLACE, pointed);
		return false;
	}

	const ContentFeatures &predicted_f = nodedef->get(id);

	// Compare core.item_place_node() for what the server does with param2
	MapNode predicted_node(id, 0, 0);

	const auto place_param2 = selected_def.place_param2;

	if (place_param2) {
		predicted_node.setParam2(*place_param2);
	} else if (predicted_f.param_type_2 == CPT2_WALLMOUNTED ||
			predicted_f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
		v3s16 dir = nodepos - neighborpos;

		if (abs(dir.Y) > MYMAX(abs(dir.X), abs(dir.Z))) {
			// If you change this code, also change builtin/game/item.lua
			u8 predicted_param2 = dir.Y < 0 ? 1 : 0;
			if (selected_def.wallmounted_rotate_vertical) {
				bool rotate90 = false;
				v3f ppos = client->getEnv().getLocalPlayer()->getPosition() / BS;
				v3f pdir = v3f::from(neighborpos) - ppos;
				switch (predicted_f.drawtype) {
					case NDT_TORCHLIKE: {
						rotate90 = !((pdir.X < 0 && pdir.Z > 0) ||
								(pdir.X > 0 && pdir.Z < 0));
						if (dir.Y > 0) {
							rotate90 = !rotate90;
						}
						break;
					};
					case NDT_SIGNLIKE: {
						rotate90 = std::abs(pdir.X) < std::abs(pdir.Z);
						break;
					}
					default: {
						rotate90 = std::abs(pdir.X) > std::abs(pdir.Z);
						break;
					}
				}
				if (rotate90) {
					predicted_param2 += 6;
				}
			}
			predicted_node.setParam2(predicted_param2);
		} else if (abs(dir.X) > abs(dir.Z)) {
			predicted_node.setParam2(dir.X < 0 ? 3 : 2);
		} else {
			predicted_node.setParam2(dir.Z < 0 ? 5 : 4);
		}
	} else if (predicted_f.param_type_2 == CPT2_FACEDIR ||
			predicted_f.param_type_2 == CPT2_COLORED_FACEDIR ||
			predicted_f.param_type_2 == CPT2_4DIR ||
			predicted_f.param_type_2 == CPT2_COLORED_4DIR) {
		v3s16 dir = nodepos - floatToInt(client->getEnv().getLocalPlayer()->getPosition(), BS);

		if (abs(dir.X) > abs(dir.Z)) {
			predicted_node.setParam2(dir.X < 0 ? 3 : 1);
		} else {
			predicted_node.setParam2(dir.Z < 0 ? 2 : 0);
		}
	}

	// Check attachment if node is in group attached_node
	int an = itemgroup_get(predicted_f.groups, "attached_node");
	if (an != 0) {
		v3s16 pp;

		if (an == 3) {
			pp = p + v3s16(0, -1, 0);
		} else if (an == 4) {
			pp = p + v3s16(0, 1, 0);
		} else if (an == 2) {
			if (predicted_f.param_type_2 == CPT2_FACEDIR ||
					predicted_f.param_type_2 == CPT2_COLORED_FACEDIR ||
					predicted_f.param_type_2 == CPT2_4DIR ||
					predicted_f.param_type_2 == CPT2_COLORED_4DIR) {
				pp = p + facedir_dirs[predicted_node.getFaceDir(nodedef)];
			} else {
				pp = p;
			}
		} else if (predicted_f.param_type_2 == CPT2_WALLMOUNTED ||
				predicted_f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
			pp = p + predicted_node.getWallMountedDir(nodedef);
		} else {
			pp = p + v3s16(0, -1, 0);
		}

		if (!nodedef->get(map.getNode(pp)).walkable) {
			soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
			// Report to server
			client->interact(INTERACT_PLACE, pointed);
			return false;
		}
	}

	// Apply color
	if (!place_param2 && (predicted_f.param_type_2 == CPT2_COLOR
			|| predicted_f.param_type_2 == CPT2_COLORED_FACEDIR
			|| predicted_f.param_type_2 == CPT2_COLORED_4DIR
			|| predicted_f.param_type_2 == CPT2_COLORED_WALLMOUNTED)) {
		const auto &indexstr = selected_item.metadata.
			getString("palette_index", 0);
		if (!indexstr.empty()) {
			s32 index = mystoi(indexstr);
			if (predicted_f.param_type_2 == CPT2_COLOR) {
				predicted_node.setParam2(index);
			} else if (predicted_f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
				// param2 = pure palette index + other
				predicted_node.setParam2((index & 0xf8) | (predicted_node.getParam2() & 0x07));
			} else if (predicted_f.param_type_2 == CPT2_COLORED_FACEDIR) {
				// param2 = pure palette index + other
				predicted_node.setParam2((index & 0xe0) | (predicted_node.getParam2() & 0x1f));
			} else if (predicted_f.param_type_2 == CPT2_COLORED_4DIR) {
				// param2 = pure palette index + other
				predicted_node.setParam2((index & 0xfc) | (predicted_node.getParam2() & 0x03));
			}
		}
	}

	// Add node to client map
	try {
		LocalPlayer *player = client->getEnv().getLocalPlayer();

		// Don't place node when player would be inside new node
		// NOTE: This is to be eventually implemented by a mod as client-side Lua
		if (!predicted_f.walkable ||
				g_settings->getBool("enable_build_where_you_stand") ||
				(client->checkPrivilege("noclip") && g_settings->getBool("noclip")) ||
				(predicted_f.walkable &&
					neighborpos != player->getStandingNodePos() + v3s16(0, 1, 0) &&
					neighborpos != player->getStandingNodePos() + v3s16(0, 2, 0))) {
			// This triggers the required mesh update too
			client->addNode(p, predicted_node);
			// Report to server
			client->interact(INTERACT_PLACE, pointed);
			// A node is predicted, also play a sound
			soundmaker->m_player_rightpunch_sound = selected_def.sound_place;
			return true;
		} else {
			soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
			return false;
		}
	} catch (const InvalidPositionException &e) {
		errorstream << "Node placement prediction failed for "
			<< selected_def.name << " (places "
			<< prediction << ") - Position not loaded" << std::endl;
		soundmaker->m_player_rightpunch_sound = selected_def.sound_place_failed;
		return false;
	}
}

void Game::handlePointingAtObject(const PointedThing &pointed, const ItemStack &tool_item,
		const ItemStack &hand_item, const v3f &player_position, bool show_debug)
{
	std::wstring infotext = unescape_translate(
		utf8_to_wide(runData.selected_object->infoText()));

	if (show_debug) {
		if (!infotext.empty()) {
			infotext += L"\n";
		}
		infotext += utf8_to_wide(runData.selected_object->debugInfoText());
	}

	m_game_ui->setInfoText(infotext);

	if (isKeyDown(KeyType::DIG)) {
		bool do_punch = false;
		bool do_punch_damage = false;

		if (runData.object_hit_delay_timer <= 0.0) {
			do_punch = true;
			do_punch_damage = true;
			runData.object_hit_delay_timer = object_hit_delay;
		}

		if (wasKeyPressed(KeyType::DIG))
			do_punch = true;

		if (do_punch) {
			infostream << "Punched object" << std::endl;
			runData.punching = true;
			runData.nodig_delay_timer = std::max(0.15f, m_repeat_dig_time);
		}

		if (do_punch_damage) {
			// Report direct punch
			v3f objpos = runData.selected_object->getPosition();
			v3f dir = (objpos - player_position).normalize();

			bool disable_send = runData.selected_object->directReportPunch(
					dir, &tool_item, &hand_item, runData.time_from_last_punch);
			runData.time_from_last_punch = 0;

			if (!disable_send)
				client->interact(INTERACT_START_DIGGING, pointed);
		}
	} else if (wasKeyDown(KeyType::PLACE)) {
		infostream << "Pressed place button while pointing at object" << std::endl;
		client->interact(INTERACT_PLACE, pointed);  // place
	}
}


void Game::handleDigging(const PointedThing &pointed, const v3s16 &nodepos,
		const ItemStack &selected_item, const ItemStack &hand_item, f32 dtime)
{
	// See also: serverpackethandle.cpp, action == 2
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	ClientMap &map = client->getEnv().getClientMap();
	MapNode n = map.getNode(nodepos);
	const auto &features = nodedef_manager->get(n);
	const ItemStack &tool_item = selected_item.name.empty() ? hand_item : selected_item;

	// NOTE: Similar piece of code exists on the server side for
	// cheat detection.
	// Get digging parameters
	DigParams params = getDigParams(features.groups,
			&tool_item.getToolCapabilities(itemdef_manager, &hand_item),
			tool_item.wear);

	// If can't dig, try hand
	if (!params.diggable) {
		params = getDigParams(features.groups,
				&hand_item.getToolCapabilities(itemdef_manager));
	}

	if (!params.diggable) {
		// I guess nobody will wait for this long
		runData.dig_time_complete = 10000000.0;
	} else {
		runData.dig_time_complete = params.time;

		client->getParticleManager()->addNodeParticle(client,
				player, nodepos, n, features);
	}

	if (!runData.digging) {
		infostream << "Started digging" << std::endl;
		runData.dig_instantly = runData.dig_time_complete == 0;
		if (client->modsLoaded() && client->getScript()->on_punchnode(nodepos, n))
			return;

		client->interact(INTERACT_START_DIGGING, pointed);
		runData.digging = true;
		runData.btn_down_for_dig = true;
	}

	if (!runData.dig_instantly) {
		runData.dig_index = (float)crack_animation_length
				* runData.dig_time
				/ runData.dig_time_complete;
	} else {
		// This is for e.g. torches
		runData.dig_index = crack_animation_length;
	}

	const auto &sound_dig = features.sound_dig;

	if (sound_dig.exists() && params.diggable) {
		if (sound_dig.name == "__group") {
			if (!params.main_group.empty()) {
				soundmaker->m_player_leftpunch_sound.gain = 0.5;
				soundmaker->m_player_leftpunch_sound.name =
						std::string("default_dig_") +
						params.main_group;
			}
		} else {
			soundmaker->m_player_leftpunch_sound = sound_dig;
		}
	}

	// Don't show cracks if not diggable
	if (runData.dig_time_complete >= 100000.0) {
	} else if (runData.dig_index < crack_animation_length) {
		client->setCrack(runData.dig_index, nodepos);
	} else {
		infostream << "Digging completed" << std::endl;
		client->setCrack(-1, v3s16(0, 0, 0));

		runData.dig_time = 0;
		runData.digging = false;
		// we successfully dug, now block it from repeating if we want to be safe
		if (g_settings->getBool("safe_dig_and_place"))
			runData.digging_blocked = true;

		runData.nodig_delay_timer =
				runData.dig_time_complete / (float)crack_animation_length;

		// We don't want a corresponding delay to very time consuming nodes
		// and nodes without digging time (e.g. torches) get a fixed delay.
		if (runData.nodig_delay_timer > 0.3f)
			runData.nodig_delay_timer = 0.3f;
		else if (runData.dig_instantly)
			runData.nodig_delay_timer = 0.15f;

		// Ensure that the delay between breaking nodes
		// (dig_time_complete + nodig_delay_timer) is at least the
		// value of the repeat_dig_time setting.
		runData.nodig_delay_timer = std::max(runData.nodig_delay_timer,
				m_repeat_dig_time - runData.dig_time_complete);

		if (client->modsLoaded() &&
				client->getScript()->on_dignode(nodepos, n)) {
			return;
		}

		if (features.node_dig_prediction == "air") {
			client->removeNode(nodepos);
		} else if (!features.node_dig_prediction.empty()) {
			content_t id;
			bool found = nodedef_manager->getId(features.node_dig_prediction, id);
			if (found)
				client->addNode(nodepos, id, true);
		}
		// implicit else: no prediction

		client->interact(INTERACT_DIGGING_COMPLETED, pointed);

		client->getParticleManager()->addDiggingParticles(client,
			player, nodepos, n, features);

		// Send event to trigger sound
		client->getEventManager()->put(new NodeDugEvent(nodepos, n));
	}

	if (runData.dig_time_complete < 100000.0) {
		runData.dig_time += dtime;
	} else {
		runData.dig_time = 0;
		client->setCrack(-1, nodepos);
	}

	camera->setDigging(0);  // Dig animation
}

void Game::updateFrame(ProfilerGraph *graph, RunStats *stats, f32 dtime,
		const CameraOrientation &cam)
{
	ZoneScoped;
	TimeTaker tt_update("Game::updateFrame()");
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	/*
		Frame time
	*/

	client->getEnv().updateFrameTime(m_is_paused);

	/*
		Fog range
	*/

	if (sky->getFogDistance() >= 0) {
		draw_control->wanted_range = MYMIN(draw_control->wanted_range, sky->getFogDistance());
	}
	if (draw_control->range_all && sky->getFogDistance() < 0) {
		runData.fog_range = FOG_RANGE_ALL;
	} else {
		runData.fog_range = draw_control->wanted_range * BS;
	}

	/*
		Calculate general brightness
	*/
	u32 daynight_ratio = client->getEnv().getDayNightRatio();
	float time_brightness = decode_light_f((float)daynight_ratio / 1000.0);
	float direct_brightness;
	bool sunlight_seen;

	// When in noclip mode force same sky brightness as above ground so you
	// can see properly
	if (draw_control->allow_noclip && m_cache_enable_free_move &&
		client->checkPrivilege("fly")) {
		direct_brightness = time_brightness;
		sunlight_seen = true;
	} else {
		float old_brightness = sky->getBrightness();
		direct_brightness = client->getEnv().getClientMap()
				.getBackgroundBrightness(MYMIN(runData.fog_range * 1.2, 60 * BS),
						daynight_ratio, (int)(old_brightness * 255.5), &sunlight_seen)
				/ 255.0;
	}

	float time_of_day_smooth = runData.time_of_day_smooth;
	float time_of_day = client->getEnv().getTimeOfDayF();

	static const float maxsm = 0.05f;
	static const float todsm = 0.05f;

	if (std::fabs(time_of_day - time_of_day_smooth) > maxsm &&
			std::fabs(time_of_day - time_of_day_smooth + 1.0) > maxsm &&
			std::fabs(time_of_day - time_of_day_smooth - 1.0) > maxsm)
		time_of_day_smooth = time_of_day;

	if (time_of_day_smooth > 0.8 && time_of_day < 0.2)
		time_of_day_smooth = time_of_day_smooth * (1.0 - todsm)
				+ (time_of_day + 1.0) * todsm;
	else
		time_of_day_smooth = time_of_day_smooth * (1.0 - todsm)
				+ time_of_day * todsm;

	runData.time_of_day_smooth = time_of_day_smooth;

	sky->update(time_of_day_smooth, time_brightness, direct_brightness,
			sunlight_seen, camera->getCameraMode(), player->getYaw(),
			player->getPitch());

	/*
		Update clouds
	*/
	updateClouds(dtime);

	/*
		Update particles
	*/
	client->getParticleManager()->step(dtime);

	/*
		Damage camera tilt
	*/
	if (player->hurt_tilt_timer > 0.0f) {
		player->hurt_tilt_timer -= dtime * 6.0f;

		if (player->hurt_tilt_timer < 0.0f)
			player->hurt_tilt_strength = 0.0f;
	}

	/*
		Update minimap pos and rotation
	*/
	if (mapper && m_game_ui->m_flags.show_hud) {
		mapper->setPos(floatToInt(player->getPosition(), BS));
		mapper->setAngle(player->getYaw());
	}

	/*
		Get chat messages from client
	*/

	updateChat(dtime);

	/*
		Inventory
	*/

	if (player->getWieldIndex() != runData.new_playeritem)
		client->setPlayerItem(runData.new_playeritem);

	if (client->updateWieldedItem()) {
		// Update wielded tool
		ItemStack selected_item, hand_item;
		ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);
		camera->wield(tool_item);
	}

	/*
		Update block draw list every 200ms or when camera direction has
		changed much
	*/
	runData.update_draw_list_timer += dtime;
	runData.touch_blocks_timer += dtime;

	constexpr float update_draw_list_delta = 0.2f;
	constexpr float touch_mapblock_delta = 4.0f;

	v3f camera_direction = camera->getDirection();

	// call only one of updateDrawList, touchMapBlocks, or updateShadow per frame
	// (the else-ifs below are intentional)
	if (runData.update_draw_list_timer >= update_draw_list_delta
			|| runData.update_draw_list_last_cam_dir.getDistanceFrom(camera_direction) > 0.2
			|| m_camera_offset_changed
			|| client->getEnv().getClientMap().needsUpdateDrawList()) {
		runData.update_draw_list_timer = 0;
		client->getEnv().getClientMap().updateDrawList();
		runData.update_draw_list_last_cam_dir = camera_direction;
	} else if (runData.touch_blocks_timer > touch_mapblock_delta) {
		client->getEnv().getClientMap().touchMapBlocks();
		runData.touch_blocks_timer = 0;
	} else if (RenderingEngine::get_shadow_renderer()) {
		updateShadows();
	}

	m_game_ui->update(*stats, client, draw_control, cam, runData.pointed_old,
			gui_chat_console.get(), dtime);

	m_game_formspec.update();

	/*
		==================== Drawing begins ====================
	*/
	if (device->isWindowVisible())
		drawScene(graph, stats);
	/*
		==================== End scene ====================
	*/

	// Damage flash is drawn in drawScene, but the timing update is done here to
	// keep dtime out of the drawing code.
	if (runData.damage_flash > 0.0f) {
		runData.damage_flash -= 384.0f * dtime;
	}

	g_profiler->avg("Game::updateFrame(): update frame [ms]", tt_update.stop(true));
}

void Game::updateClouds(float dtime)
{
	if (this->sky->getCloudsVisible()) {
		this->clouds->setVisible(true);
		this->clouds->step(dtime);
		// this->camera->getPosition is not enough for third-person camera.
		v3f camera_node_position = this->camera->getCameraNode()->getPosition();
		v3s16 camera_offset      = this->camera->getOffset();
		camera_node_position.X   = camera_node_position.X + camera_offset.X * BS;
		camera_node_position.Y   = camera_node_position.Y + camera_offset.Y * BS;
		camera_node_position.Z   = camera_node_position.Z + camera_offset.Z * BS;
		this->clouds->update(camera_node_position, this->sky->getCloudColor());
		if (this->clouds->isCameraInsideCloud() && this->fogEnabled()) {
			// If camera is inside cloud and fog is enabled, use cloud's colors as sky colors.
			video::SColor clouds_dark = this->clouds->getColor().getInterpolated(
					video::SColor(255, 0, 0, 0), 0.9);
			this->sky->overrideColors(clouds_dark, this->clouds->getColor());
			this->sky->setInClouds(true);
			this->runData.fog_range = std::fmin(this->runData.fog_range * 0.5f, 32.0f * BS);
			// Clouds are not drawn in this case.
			this->clouds->setVisible(false);
		}
	} else {
		this->clouds->setVisible(false);
	}
}

/* Log times and stuff for visualization */
inline void Game::updateProfilerGraphs(ProfilerGraph *graph)
{
	Profiler::GraphValues values;
	g_profiler->graphPop(values);
	graph->put(values);
}

/****************************************************************************
 * Shadows
 *****************************************************************************/
void Game::updateShadows()
{
	ShadowRenderer *shadow = RenderingEngine::get_shadow_renderer();
	if (!shadow)
		return;

	float in_timeofday = std::fmod(runData.time_of_day_smooth, 1.0f);

	float timeoftheday = getWickedTimeOfDay(in_timeofday);
	bool is_day = timeoftheday > 0.25 && timeoftheday < 0.75;
	bool is_shadow_visible = is_day ? sky->getSunVisible() : sky->getMoonVisible();
	const auto &lighting = client->getEnv().getLocalPlayer()->getLighting();
	shadow->setShadowIntensity(is_shadow_visible ? lighting.shadow_intensity : 0.0f);
	shadow->setShadowTint(lighting.shadow_tint);

	timeoftheday = std::fmod(timeoftheday + 0.75f, 0.5f) + 0.25f;
	const float offset_constant = 10000.0f;

	v3f light = is_day ? sky->getSunDirection() : sky->getMoonDirection();

	v3f sun_pos = light * offset_constant;
	shadow->getDirectionalLight().setDirection(sun_pos);
	shadow->setTimeOfDay(in_timeofday);

	shadow->getDirectionalLight().updateFrustum(camera, client);
}

void Game::drawScene(ProfilerGraph *graph, RunStats *stats)
{
	ZoneScoped;

	const video::SColor fog_color = this->sky->getFogColor();
	const video::SColor sky_color = this->sky->getSkyColor();

	/*
		Fog
	*/
	if (this->fogEnabled()) {
		this->driver->setFog(
				fog_color,
				video::EFT_FOG_LINEAR,
				this->runData.fog_range * this->sky->getFogStart(),
				this->runData.fog_range * 1.0f,
				0.f, // unused
				false, // pixel fog
				true // range fog
		);
	} else {
		this->driver->setFog(
				fog_color,
				video::EFT_FOG_LINEAR,
				FOG_RANGE_ALL,
				FOG_RANGE_ALL + 100 * BS,
				0.f, // unused
				false, // pixel fog
				false // range fog
		);
	}

	/*
		Drawing
	*/
	TimeTaker tt_draw("Draw scene", nullptr, PRECISION_MICRO);
	this->driver->beginScene(true, true, sky_color);

	const LocalPlayer *player = this->client->getEnv().getLocalPlayer();
	bool draw_wield_tool = (this->m_game_ui->m_flags.show_hud &&
			(player->hud_flags & HUD_FLAG_WIELDITEM_VISIBLE) &&
			(this->camera->getCameraMode() == CAMERA_MODE_FIRST));
	bool draw_crosshair = (
			(player->hud_flags & HUD_FLAG_CROSSHAIR_VISIBLE) &&
			(this->camera->getCameraMode() != CAMERA_MODE_THIRD_FRONT));

	if (isTouchShootlineUsed())
		draw_crosshair = false;

	this->m_rendering_engine->draw_scene(sky_color, this->m_game_ui->m_flags.show_hud,
			draw_wield_tool, draw_crosshair);

	/*
		Profiler graph
	*/
	v2u32 screensize = this->driver->getScreenSize();

	if (this->m_game_ui->m_flags.show_profiler_graph)
		graph->draw(10, screensize.Y - 10, driver, g_fontengine->getFont());

	/*
		Damage flash
	*/
	if (this->runData.damage_flash > 0.0f) {
		video::SColor color(this->runData.damage_flash, 180, 0, 0);
		this->driver->draw2DRectangle(color,
					core::rect<s32>(0, 0, screensize.X, screensize.Y),
					NULL);
	}

	this->driver->endScene();

	stats->drawtime = tt_draw.stop(true);
	g_profiler->graphAdd("Draw scene [us]", stats->drawtime);

}

/****************************************************************************
 Misc
 ****************************************************************************/

void Game::showOverlayMessage(const char *msg, float dtime, int percent, float *indef_pos)
{
	m_rendering_engine->draw_load_screen(wstrgettext(msg), guienv, texture_src,
			dtime, percent, indef_pos);
}

void Game::settingChangedCallback(const std::string &setting_name, void *data)
{
	((Game *)data)->readSettings();
}

void Game::readSettings()
{
	LogLevel chat_log_level = Logger::stringToLevel(g_settings->get("chat_log_level"));
	if (chat_log_level == LL_MAX) {
		warningstream << "Supplied unrecognized chat_log_level; showing none." << std::endl;
		chat_log_level = LL_NONE;
	}
	m_chat_log_buf.setLogLevel(chat_log_level);

	m_cache_doubletap_jump               = g_settings->getBool("doubletap_jump");
	m_cache_toggle_sneak_key             = g_settings->getBool("toggle_sneak_key");
	m_cache_toggle_aux1_key              = g_settings->getBool("toggle_aux1_key");
	m_cache_enable_joysticks             = g_settings->getBool("enable_joysticks");
	m_cache_enable_fog                   = g_settings->getBool("enable_fog");
	m_cache_mouse_sensitivity            = g_settings->getFloat("mouse_sensitivity", 0.001f, 10.0f);
	m_cache_joystick_frustum_sensitivity = std::max(g_settings->getFloat("joystick_frustum_sensitivity"), 0.001f);
	m_repeat_place_time                  = g_settings->getFloat("repeat_place_time", 0.16f, 2.0f);
	m_repeat_dig_time                    = g_settings->getFloat("repeat_dig_time", 0.0f, 2.0f);

	m_cache_enable_noclip                = g_settings->getBool("noclip");
	m_cache_enable_free_move             = g_settings->getBool("free_move");

	m_cache_cam_smoothing = 0;
	if (g_settings->getBool("cinematic"))
		m_cache_cam_smoothing = 1 - g_settings->getFloat("cinematic_camera_smoothing");
	else
		m_cache_cam_smoothing = 1 - g_settings->getFloat("camera_smoothing");

	m_cache_cam_smoothing = rangelim(m_cache_cam_smoothing, 0.01f, 1.0f);
	m_cache_mouse_sensitivity = rangelim(m_cache_mouse_sensitivity, 0.001, 100.0);

	m_invert_mouse = g_settings->getBool("invert_mouse");
	m_enable_hotbar_mouse_wheel = g_settings->getBool("enable_hotbar_mouse_wheel");
	m_invert_hotbar_mouse_wheel = g_settings->getBool("invert_hotbar_mouse_wheel");

	m_does_lost_focus_pause_game = g_settings->getBool("pause_on_lost_focus");
}

/****************************************************************************/
/****************************************************************************
 extern function for launching the game
 ****************************************************************************/
/****************************************************************************/

void the_game(volatile std::sig_atomic_t *kill,
		InputHandler *input,
		RenderingEngine *rendering_engine,
		const GameStartData &start_data,
		std::string &error_message,
		ChatBackend &chat_backend,
		bool *reconnect_requested) // Used for local game
{
	Game game;

	try {

		if (game.startup(kill, input, rendering_engine, start_data,
				error_message, reconnect_requested, &chat_backend)) {
			game.run();
		}

	} catch (SerializationError &e) {
		const std::string ver_err = fmtgettext("The server is probably running a different version of %s.", PROJECT_NAME_C);
		error_message = strgettext("A serialization error occurred:") +"\n"
				+ e.what() + "\n\n" + ver_err;
		errorstream << error_message << std::endl;
	} catch (ServerError &e) {
		error_message = e.what();
		errorstream << "ServerError: " << error_message << std::endl;
	} catch (ModError &e) {
		// DO NOT TRANSLATE the `ModError`, it's used by `ui.lua`
		error_message = std::string("ModError: ") + e.what() +
				strgettext("\nCheck debug.txt for details.");
		errorstream << error_message << std::endl;
	} catch (con::PeerNotFoundException &e) {
		error_message = gettext("Connection error (timed out?)");
		errorstream << error_message << std::endl;
	} catch (ShaderException &e) {
		error_message = e.what();
		errorstream << error_message << std::endl;
	}

	game.shutdown();
}
