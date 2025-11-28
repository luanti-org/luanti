// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "game.h"

#include <AnimatedMeshSceneNode.h>
#include <optional>
#include <vector>
#include "camera.h"
#include "client.h"
#include "client/clientevent.h"
#include "client/game_formspec.h"
#include "client/renderingengine.h"
#include "clientdynamicinfo.h"
#include "clouds.h"
#include "gui/touchcontrols.h"
#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include "log_internal.h"
#include "sky.h"
#include "util/pointedthing.h"

/* DO NOT INCLUDE THIS FROM OUTSIDE GAME.CPP */

class Game;
class GameUI;
class SoundMaker;
class Server;
class NodeMetadata;
class ProfilerGraph;
class EventManager;
class GUIChatConsole;
class QuicktuneShortcutter;

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

struct ClientEventHandler
{
	void (Game::*handler)(ClientEvent *, CameraOrientation *);
};

using PausedNodesList = std::vector<std::pair<irr_ptr<scene::AnimatedMeshSceneNode>, float>>;

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

	Client *getClient() { return client; }

	// Pre-calculated value
	int crack_animation_length;

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
	void handleClientEvent_OverrideDayNightRatio(ClientEvent *event,
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
	std::unique_ptr<SoundMaker> soundmaker;

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
	bool isTouchShootlineUsed() const;
#ifdef __ANDROID__
	bool m_android_chat_open;
#endif

	float m_shutdown_progress = 0.0f;
};
