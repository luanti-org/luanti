// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti contributors

#pragma once

#include "line3d.h"
#include "util/pointedthing.h"
#include <optional>

// Forward declarations
class Client;
class Camera;
class GameUI;
class Hud;
class SoundMaker;
class ISoundManager;
class IWritableItemDefManager;
class NodeDefManager;
class NodeMetadata;
class InputHandler;
class GameFormSpec;

struct GameRunData;
struct ItemStack;
struct ItemDefinition;
struct Pointabilities;

/**
 * Namespace for player interaction functions
 * These functions handle digging, placing, punching, and using items
 */
namespace interact {

/**
 * Main player interaction processing function
 * Handles all player interactions with the world (digging, placing, punching, using)
 */
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
	GameFormSpec *game_formspec
);

/**
 * Updates what the player is currently pointing at
 * Returns the PointedThing representing what is being pointed at
 */
PointedThing updatePointedThing(
	const core::line3d<f32> &shootline,
	bool liquids_pointable,
	const std::optional<Pointabilities> &pointabilities,
	bool look_for_object,
	const v3s16 &camera_offset,
	Client *client,
	Hud *hud
);

/**
 * Handles activation when pointing at nothing (air)
 */
void handlePointingAtNothing(
	const ItemStack &playerItem,
	Client *client
);

/**
 * Handles interaction when pointing at a node
 */
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
	GameFormSpec *game_formspec
);

/**
 * Handles interaction when pointing at an object
 */
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
	f32 repeat_dig_time
);

/**
 * Handles digging mechanics with timing and prediction
 */
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
	int crack_animation_length
);

/**
 * Handles node placement with client-side prediction
 * Returns true if a node was placed
 */
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
	GameFormSpec *game_formspec
);

} // namespace interact
