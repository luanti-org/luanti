// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti contributors

#include "interact.h"
#include "soundmaker.h"

#include "camera.h"
#include "client.h"
#include "clientenvironment.h"
#include "clientmap.h"
#include "content_cao.h"
#include "game_formspec.h"
#include "gameui.h"
#include "gui/touchcontrols.h"
#include "hud.h"
#include "inputhandler.h"
#include "keys.h"
#include "gettext.h"
#include "itemdef.h"
#include "localplayer.h"
#include "log.h"
#include "mapnode.h"
#include "network/networkprotocol.h"
#include "nodedef.h"
#include "nodemetadata.h"
#include "particles.h"
#include "raycast.h"
#include "scripting_client.h"
#include "settings.h"
#include "tool.h"
#include "util/directiontables.h"
#include "util/pointedthing.h"

#include <ISceneNode.h>

#include "mapblock_mesh.h"

// External globals
extern TouchControls *g_touchcontrols;

// GameRunData structure (defined in game.cpp)
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

	float jump_timer_up;
	float jump_timer_down;
	float jump_timer_down_before;

	float damage_flash;
	float update_draw_list_timer;
	float touch_blocks_timer;

	f32 fog_range;

	v3f update_draw_list_last_cam_dir;

	float time_of_day_smooth;
};

static constexpr float object_hit_delay = 0.2;

namespace interact {
	// Helper function to check if touch shootline should be used
	static inline bool isTouchShootlineUsed(Camera *camera) {
		return g_touchcontrols && g_touchcontrols->isShootlineAvailable() &&
		       camera->getCameraMode() == CAMERA_MODE_FIRST;
	}

	void processPlayerInteraction(
		f32 dtime,
		Client *client,
		Camera *camera,
		GameUI *game_ui,
		Hud *hud,
		SoundMaker *soundmaker,
		IWritableItemDefManager *itemdef_manager,
		InputHandler *input,
		GameRunData &runData,
		f32 repeat_place_time,
		f32 repeat_dig_time,
		int crack_animation_length,
		GameFormSpec *game_formspec) {
		LocalPlayer *player = client->getEnv().getLocalPlayer();

		const v3f camera_direction = camera->getDirection();
		const v3s16 camera_offset = camera->getOffset();

		/*
		    Calculate what block is the crosshair pointing to
		*/

		ItemStack selected_item, hand_item;
		const ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);

		const ItemDefinition &selected_def = tool_item.getDefinition(itemdef_manager);
		f32 d = getToolRange(tool_item, hand_item, itemdef_manager);

		line3d<f32> shootline;

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

		if (isTouchShootlineUsed(camera)) {
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
		                                          camera_offset,
		                                          client,
		                                          hud);

		// Update selected_object based on what we're pointing at
		if (pointed.type == POINTEDTHING_OBJECT) {
			runData.selected_object = client->getEnv().getActiveObject(pointed.object_id);
		} else {
			runData.selected_object = nullptr;
		}

		if (pointed != runData.pointed_old)
			infostream << "Pointing at " << pointed.dump() << std::endl;

		if (g_touchcontrols) {
			auto mode = selected_def.touch_interaction.getMode(selected_def, pointed.type);
			g_touchcontrols->applyContextControls(mode);
			// applyContextControls may change dig/place input.
			// Update again so that TOSERVER_INTERACT packets have the correct controls set.
			player->control.dig = input->isKeyDown(KeyType::DIG);
			player->control.place = input->isKeyDown(KeyType::PLACE);
		}

		// Note that updating the selection mesh every frame is not particularly efficient,
		// but the halo rendering code is already inefficient so there's no point in optimizing it here
		hud->updateSelectionMesh(camera_offset);

		// Allow digging again if button is not pressed
		if (runData.digging_blocked && !input->isKeyDown(KeyType::DIG))
			runData.digging_blocked = false;

		/*
		    Stop digging when
		    - releasing dig button
		    - pointing away from node
		*/
		if (runData.digging) {
			if (input->wasKeyReleased(KeyType::DIG)) {
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
		} else if (runData.dig_instantly && input->wasKeyReleased(KeyType::DIG)) {
			// Remove e.g. torches faster when clicking instead of holding dig button
			runData.nodig_delay_timer = 0;
			runData.dig_instantly = false;
		}

		if (!runData.digging && runData.btn_down_for_dig && !input->isKeyDown(KeyType::DIG))
			runData.btn_down_for_dig = false;

		runData.punching = false;

		soundmaker->m_player_leftpunch_sound = SoundSpec();
		soundmaker->m_player_leftpunch_sound2 = pointed.type != POINTEDTHING_NOTHING
			                                        ? selected_def.sound_use
			                                        : selected_def.sound_use_air;

		// Prepare for repeating, unless we're not supposed to
		if (input->isKeyDown(KeyType::PLACE) && !g_settings->getBool("safe_dig_and_place"))
			runData.repeat_place_timer += dtime;
		else
			runData.repeat_place_timer = 0;

		if (selected_def.usable && input->isKeyDown(KeyType::DIG)) {
			if (input->wasKeyPressed(KeyType::DIG) && (!client->modsLoaded() ||
			                                           !client->getScript()->on_item_use(selected_item, pointed)))
				client->interact(INTERACT_USE, pointed);
		} else if (pointed.type == POINTEDTHING_NODE) {
			handlePointingAtNode(pointed, selected_item, hand_item, dtime,
			                     client, camera, game_ui, hud, soundmaker, itemdef_manager,
			                     client->getEnv().getClientMap().getNodeDefManager(), input, runData,
			                     repeat_place_time, repeat_dig_time, crack_animation_length, game_formspec);
		} else if (pointed.type == POINTEDTHING_OBJECT) {
			v3f player_position = player->getPosition();
			bool basic_debug_allowed = client->checkPrivilege("debug") || (player->hud_flags & HUD_FLAG_BASIC_DEBUG);
			handlePointingAtObject(pointed, tool_item, hand_item, player_position,
			                       game_ui->getFlags().show_basic_debug && basic_debug_allowed,
			                       client, game_ui, input, runData, repeat_dig_time);
		} else if (input->isKeyDown(KeyType::DIG)) {
			// When button is held down in air, show continuous animation
			runData.punching = true;
			// Run callback even though item is not usable
			if (input->wasKeyPressed(KeyType::DIG) && client->modsLoaded())
				client->getScript()->on_item_use(selected_item, pointed);
		} else if (input->wasKeyPressed(KeyType::PLACE)) {
			handlePointingAtNothing(selected_item, client);
		}

		runData.pointed_old = pointed;

		if (runData.punching || input->wasKeyPressed(KeyType::DIG))
			camera->setDigging(0); // dig animation

		input->clearWasKeyPressed();
		input->clearWasKeyReleased();
		// Ensure DIG & PLACE are marked as handled
		input->wasKeyDown(KeyType::DIG);
		input->wasKeyDown(KeyType::PLACE);

		input->joystick.clearWasKeyPressed(KeyType::DIG);
		input->joystick.clearWasKeyPressed(KeyType::PLACE);

		input->joystick.clearWasKeyReleased(KeyType::DIG);
		input->joystick.clearWasKeyReleased(KeyType::PLACE);
	}

	PointedThing updatePointedThing(
		const line3d<f32> &shootline,
		bool liquids_pointable,
		const std::optional<Pointabilities> &pointabilities,
		bool look_for_object,
		const v3s16 &camera_offset,
		Client *client,
		Hud *hud) {
		std::vector<aabb3f> *selectionboxes = hud->getSelectionBoxes();
		selectionboxes->clear();
		hud->setSelectedFaceNormal(v3f());
		static thread_local const bool show_entity_selectionbox = g_settings->getBool(
			"show_entity_selectionbox");

		ClientEnvironment &env = client->getEnv();
		ClientMap &map = env.getClientMap();
		const NodeDefManager *nodedef = map.getNodeDefManager();

		hud->pointing_at_object = false;

		RaycastState s(shootline, look_for_object, liquids_pointable, pointabilities);
		PointedThing result;
		env.continueRaycast(&s, &result);
		if (result.type == POINTEDTHING_OBJECT) {
			hud->pointing_at_object = true;

			ClientActiveObject *selected_object = client->getEnv().getActiveObject(result.object_id);
			aabb3f selection_box{{0.0f, 0.0f, 0.0f}};
			if (show_entity_selectionbox && selected_object && selected_object->doShowSelectionBox() &&
			    selected_object->getSelectionBox(&selection_box)) {
				v3f pos = selected_object->getPosition();
				selectionboxes->push_back(selection_box);
				hud->setSelectionPos(pos, camera_offset);
				GenericCAO *gcao = dynamic_cast<GenericCAO *>(selected_object);
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
			for (aabb3f box: boxes) {
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

			for (const v3s16 &dir: g_6dirs) {
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
			float timerf = (float) (PI * ((timer / 2500.0) - 0.5));
			float sin_r = 0.08f * std::sin(timerf);
			float sin_g = 0.08f * std::sin(timerf + PI * 0.5f);
			float sin_b = 0.08f * std::sin(timerf + PI);
			c.setRed(clamp(round32(c.getRed() * (0.8 + sin_r)), 0, 255));
			c.setGreen(clamp(round32(c.getGreen() * (0.8 + sin_g)), 0, 255));
			c.setBlue(clamp(round32(c.getBlue() * (0.8 + sin_b)), 0, 255));

			// Set mesh final color
			hud->setSelectionMeshColor(c);
		}
		return result;
	}

	void handlePointingAtNothing(
		const ItemStack &playerItem,
		Client *client) {
		infostream << "Attempted to place item while pointing at nothing" << std::endl;
		PointedThing fauxPointed;
		fauxPointed.type = POINTEDTHING_NOTHING;
		client->interact(INTERACT_ACTIVATE, fauxPointed);
	}

	void handlePointingAtNode(
		const PointedThing &pointed,
		const ItemStack &selected_item,
		const ItemStack &hand_item,
		f32 dtime,
		Client *client,
		Camera *camera,
		GameUI *game_ui,
		Hud *hud,
		SoundMaker *soundmaker,
		IWritableItemDefManager *itemdef_manager,
		const NodeDefManager *nodedef_manager,
		InputHandler *input,
		GameRunData &runData,
		f32 repeat_place_time,
		f32 repeat_dig_time,
		int crack_animation_length,
		GameFormSpec *game_formspec) {
		v3s16 nodepos = pointed.node_undersurface;
		v3s16 neighborpos = pointed.node_abovesurface;

		/*
		    Check information text of node
		*/

		ClientMap &map = client->getEnv().getClientMap();

		if (runData.nodig_delay_timer <= 0.0 && input->isKeyDown(KeyType::DIG)
		    && !runData.digging_blocked
		    && client->checkPrivilege("interact")) {
			handleDigging(pointed, nodepos, selected_item, hand_item, dtime,
			              client, camera, soundmaker, itemdef_manager, nodedef_manager,
			              runData, repeat_dig_time, crack_animation_length);
		}

		// This should be done after digging handling
		NodeMetadata *meta = map.getNodeMetadata(nodepos);

		if (meta) {
			game_ui->setInfoText(unescape_translate(utf8_to_wide(
				meta->getString("infotext"))));
		} else {
			MapNode n = map.getNode(nodepos);

			if (nodedef_manager->get(n).name == "unknown") {
				game_ui->setInfoText(L"Unknown node");
			}
		}

		if ((input->wasKeyPressed(KeyType::PLACE) ||
		     runData.repeat_place_timer >= repeat_place_time) &&
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
			                            pointed, meta, client, soundmaker, itemdef_manager, nodedef_manager,
			                            input, game_formspec);

			if (placed && client->modsLoaded())
				client->getScript()->on_placenode(pointed, def);
		}
	}

	void handlePointingAtObject(
		const PointedThing &pointed,
		const ItemStack &tool_item,
		const ItemStack &hand_item,
		const v3f &player_position,
		bool show_debug,
		Client *client,
		GameUI *game_ui,
		InputHandler *input,
		GameRunData &runData,
		f32 repeat_dig_time) {
		ClientActiveObject *selected_object = runData.selected_object;
		std::wstring infotext = unescape_translate(
			utf8_to_wide(selected_object->infoText()));

		if (show_debug) {
			if (!infotext.empty()) {
				infotext += L"\n";
			}
			infotext += utf8_to_wide(selected_object->debugInfoText());
		}

		game_ui->setInfoText(infotext);

		if (input->isKeyDown(KeyType::DIG)) {
			bool do_punch = false;
			bool do_punch_damage = false;

			if (runData.object_hit_delay_timer <= 0.0) {
				do_punch = true;
				do_punch_damage = true;
				runData.object_hit_delay_timer = object_hit_delay;
			}

			if (input->wasKeyPressed(KeyType::DIG))
				do_punch = true;

			if (do_punch) {
				infostream << "Punched object" << std::endl;
				runData.punching = true;
				runData.nodig_delay_timer = std::max(0.15f, repeat_dig_time);
			}

			if (do_punch_damage) {
				// Report direct punch
				v3f objpos = selected_object->getPosition();
				v3f dir = (objpos - player_position).normalize();

				bool disable_send = selected_object->directReportPunch(
					dir, &tool_item, &hand_item, runData.time_from_last_punch);
				runData.time_from_last_punch = 0;

				if (!disable_send)
					client->interact(INTERACT_START_DIGGING, pointed);
			}
		} else if (input->wasKeyDown(KeyType::PLACE)) {
			infostream << "Pressed place button while pointing at object" << std::endl;
			client->interact(INTERACT_PLACE, pointed); // place
		}
	}

	void handleDigging(
		const PointedThing &pointed,
		const v3s16 &nodepos,
		const ItemStack &selected_item,
		const ItemStack &hand_item,
		f32 dtime,
		Client *client,
		Camera *camera,
		SoundMaker *soundmaker,
		IWritableItemDefManager *itemdef_manager,
		const NodeDefManager *nodedef_manager,
		GameRunData &runData,
		f32 repeat_dig_time,
		int crack_animation_length) {
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

			client->getParticleManager()->addNodeParticle(player, nodepos, n);
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
			runData.dig_index = (float) crack_animation_length
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
					runData.dig_time_complete / (float) crack_animation_length;

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
			                                     repeat_dig_time - runData.dig_time_complete);

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

			client->getParticleManager()->addDiggingParticles(player, nodepos, n);

			// Send event to trigger sound
			// Note: NodeDugEvent is defined in game.cpp, so we'll trigger it via event manager
			// client->getEventManager()->put(new NodeDugEvent(nodepos, n));
		}

		if (runData.dig_time_complete < 100000.0) {
			runData.dig_time += dtime;
		} else {
			runData.dig_time = 0;
			client->setCrack(-1, nodepos);
		}

		camera->setDigging(0); // Dig animation
	}

	bool nodePlacement(
		const ItemDefinition &selected_def,
		const ItemStack &selected_item,
		const v3s16 &nodepos,
		const v3s16 &neighborpos,
		const PointedThing &pointed,
		const NodeMetadata *meta,
		Client *client,
		SoundMaker *soundmaker,
		IWritableItemDefManager *itemdef_manager,
		const NodeDefManager *nodedef_manager,
		InputHandler *input,
		GameFormSpec *game_formspec) {
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
		    && !input->isKeyDown(KeyType::SNEAK)) {
			// on_rightclick callbacks are called anyway
			if (nodedef_manager->get(map.getNode(nodepos)).rightclickable)
				client->interact(INTERACT_PLACE, pointed);

			game_formspec->showNodeFormspec(meta->getString("formspec"), nodepos);
			return false;
		}

		// on_rightclick callback
		if (prediction.empty() || (nodedef->get(node).rightclickable &&
		                           !input->isKeyDown(KeyType::SNEAK))) {
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
} // namespace interact
