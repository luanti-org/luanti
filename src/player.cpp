// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "player.h"

#include <cmath>
#include "threading/mutex_auto_lock.h"
#include "util/numeric.h"
#include "hud.h"
#include "constants.h"
#include "gamedef.h"
#include "settings.h"
#include "log.h"
#include "porting.h"  // strlcpy
#include <tuple>

const struct EnumString es_CameraMode[] = {
	{CAMERA_MODE_ANY, "any"},
	{CAMERA_MODE_FIRST, "first"},
	{CAMERA_MODE_THIRD, "third"},
	{CAMERA_MODE_THIRD_FRONT, "third_front"},
	{0, nullptr}
};

bool is_valid_player_name(std::string_view name)
{
	return !name.empty() && name.size() <= PLAYERNAME_SIZE && string_allowed(name, PLAYERNAME_ALLOWED_CHARS);
}

Player::Player(const std::string &name, IItemDefManager *idef):
	inventory(idef)
{
	m_name = name;

	inventory.clear();
	inventory.addList("main", PLAYER_INVENTORY_SIZE);
	InventoryList *craft = inventory.addList("craft", 9);
	craft->setWidth(3);
	inventory.addList("craftpreview", 1);
	inventory.addList("craftresult", 1);
	inventory.setModified(false);

	// Can be redefined via Lua
	inventory_formspec = "size[8,7.5]"
		//"image[1,0.6;1,2;player.png]"
		"list[current_player;main;0,3.5;8,4;]"
		"list[current_player;craft;3,0;3,3;]"
		"listring[]"
		"list[current_player;craftpreview;7,1;1,1;]";

	// Initialize movement settings at default values, so movement can work
	// if the server fails to send them
	movement_acceleration_default   = 3    * BS;
	movement_acceleration_air       = 2    * BS;
	movement_acceleration_fast      = 10   * BS;
	movement_speed_walk             = 4    * BS;
	movement_speed_crouch           = 1.35 * BS;
	movement_speed_fast             = 20   * BS;
	movement_speed_climb            = 2    * BS;
	movement_speed_jump             = 6.5  * BS;
	movement_liquid_fluidity        = 1    * BS;
	movement_liquid_fluidity_smooth = 0.5  * BS;
	movement_liquid_sink            = 10   * BS;
	movement_gravity                = 9.81 * BS;
	local_animation_speed           = 0.0;

	hud_flags =
		HUD_FLAG_HOTBAR_VISIBLE    | HUD_FLAG_HEALTHBAR_VISIBLE |
		HUD_FLAG_CROSSHAIR_VISIBLE | HUD_FLAG_WIELDITEM_VISIBLE |
		HUD_FLAG_BREATHBAR_VISIBLE | HUD_FLAG_MINIMAP_VISIBLE   |
		HUD_FLAG_MINIMAP_RADAR_VISIBLE | HUD_FLAG_BASIC_DEBUG   |
		HUD_FLAG_CHAT_VISIBLE;

	hud_hotbar_itemcount = HUD_HOTBAR_ITEMCOUNT_DEFAULT;
}

Player::~Player()
{
	clearHud();
}

void Player::setWieldIndex(u16 index)
{
	const InventoryList *mlist = inventory.getList("main");
	m_wield_index = MYMIN(index, mlist ? mlist->getSize() : 0);
}

u16 Player::getWieldIndex()
{
	return std::min(m_wield_index, getMaxHotbarItemcount());
}

ItemStack &Player::getWieldedItem(ItemStack *selected, ItemStack *hand) const
{
	assert(selected);

	const InventoryList *mlist = inventory.getList("main"); // TODO: Make this generic
	const InventoryList *hlist = inventory.getList("hand");

	if (mlist && m_wield_index < mlist->getSize())
		*selected = mlist->getItem(m_wield_index);

	if (hand && hlist)
		*hand = hlist->getItem(0);

	// Return effective tool item
	return (hand && selected->name.empty()) ? *hand : *selected;
}

u32 Player::addHud(HudElement *toadd)
{
	u32 id = getFreeHudID();

	if (id < hud.size())
		hud[id] = toadd;
	else
		hud.push_back(toadd);

	return id;
}

HudElement* Player::getHud(u32 id)
{
	if (id < hud.size())
		return hud[id];
	return nullptr;
}

HudElement* Player::removeHud(u32 id)
{
	HudElement* retval = nullptr;
	if (id < hud.size()) {
		retval = hud[id];
		hud[id] = nullptr;
	}
	// shrink list if possible
	while (!hud.empty() && hud.back() == nullptr)
		hud.pop_back();
	return retval;
}

void Player::clearHud()
{
	while (!hud.empty()) {
		delete hud.back();
		hud.pop_back();
	}
}

u16 Player::getMaxHotbarItemcount()
{
	InventoryList *mainlist = inventory.getList("main");
	return mainlist ? std::min(mainlist->getSize(), (u32) hud_hotbar_itemcount) : 0;
}

void PlayerControl::setMovementFromKeys()
{
	bool a_up = direction_keys & (1 << 0),
		a_down = direction_keys & (1 << 1),
		a_left = direction_keys & (1 << 2),
		a_right = direction_keys & (1 << 3);

	if (a_up || a_down || a_left || a_right)  {
		// if contradictory keys pressed, stay still
		if (a_up && a_down && a_left && a_right)
			movement_speed = 0.0f;
		else if (a_up && a_down && !a_left && !a_right)
			movement_speed = 0.0f;
		else if (!a_up && !a_down && a_left && a_right)
			movement_speed = 0.0f;
		else
			// If there is a keyboard event, assume maximum speed
			movement_speed = 1.0f;
	}

	// Check keyboard for input
	float x = 0, y = 0;
	if (a_up)
		y += 1;
	if (a_down)
		y -= 1;
	if (a_left)
		x -= 1;
	if (a_right)
		x += 1;

	if (x != 0 || y != 0)
		// If there is a keyboard event, it takes priority
		movement_direction = std::atan2(x, y);
}

u32 PlayerControl::getKeysPressed() const
{
	u32 keypress_bits =
		( (u32)(jump  & 1) << 4) |
		( (u32)(aux1  & 1) << 5) |
		( (u32)(sneak & 1) << 6) |
		( (u32)(dig   & 1) << 7) |
		( (u32)(place & 1) << 8) |
		( (u32)(zoom  & 1) << 9)
	;

	// If any direction keys are pressed pass those through
	if (direction_keys != 0)
	{
		keypress_bits |= direction_keys;
	}
	// Otherwise set direction keys based on joystick movement (for mod compatibility)
	else if (isMoving())
	{
		float abs_d;

		// (absolute value indicates forward / backward)
		abs_d = std::abs(movement_direction);
		if (abs_d < 3.0f / 8.0f * M_PI)
			keypress_bits |= (u32)1; // Forward
		if (abs_d > 5.0f / 8.0f * M_PI)
			keypress_bits |= (u32)1 << 1; // Backward

		// rotate entire coordinate system by 90 degree
		abs_d = movement_direction + M_PI_2;
		if (abs_d >= M_PI)
			abs_d -= 2 * M_PI;
		abs_d = std::abs(abs_d);
		// (value now indicates left / right)
		if (abs_d < 3.0f / 8.0f * M_PI)
			keypress_bits |= (u32)1 << 2; // Left
		if (abs_d > 5.0f / 8.0f * M_PI)
			keypress_bits |= (u32)1 << 3; // Right
	}

	return keypress_bits;
}


void PlayerControl::unpackKeysPressed(u32 keypress_bits)
{
	direction_keys = keypress_bits & 0xf;
	jump  = keypress_bits & (1 << 4);
	aux1  = keypress_bits & (1 << 5);
	sneak = keypress_bits & (1 << 6);
	dig   = keypress_bits & (1 << 7);
	place = keypress_bits & (1 << 8);
	zoom  = keypress_bits & (1 << 9);
}

v2f PlayerControl::getMovement() const
{
	return v2f(std::sin(movement_direction), std::cos(movement_direction)) * movement_speed;
}

static auto tie(const PlayerPhysicsOverride &o)
{
	// Make sure to add new members to this list!
	return std::tie(
	o.speed, o.jump, o.gravity, o.sneak, o.sneak_glitch, o.new_move, o.speed_climb,
	o.speed_crouch, o.liquid_fluidity, o.liquid_fluidity_smooth, o.liquid_sink,
	o.acceleration_default, o.acceleration_air, o.speed_fast, o.acceleration_fast,
	o.speed_walk
	);
}

bool PlayerPhysicsOverride::operator==(const PlayerPhysicsOverride &other) const
{
	return tie(*this) == tie(other);
}
