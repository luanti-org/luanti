// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "lua_api/l_base.h"
#include "irrlichttypes.h"
#include <lua.h>

class ServerActiveObject;
class LuaEntitySAO;
class PlayerSAO;
class RemotePlayer;

/*
	ObjectRef
*/

class ObjectRef : public ModApiBase {
public:
	ObjectRef(ServerActiveObject *object);

	~ObjectRef() = default;

	// Creates an ObjectRef and leaves it on top of stack
	// NOTE: do not call this, use `ScriptApiBase::objectrefGetOrCreate()`!
	static void create(lua_State *L, ServerActiveObject *object);

	// Clear the pointer in the ObjectRef (at -1).
	// Throws an fatal error if the object pointer wasn't `expect`.
	static void set_null(lua_State *L, void *expect);

	static void Register(lua_State *L);

	static ServerActiveObject* getobject(ObjectRef *ref);

	static const char className[];
private:
	ServerActiveObject *m_object = nullptr;
	static luaL_Reg methods[];


	static LuaEntitySAO* getluaobject(ObjectRef *ref);

	static PlayerSAO* getplayersao(ObjectRef *ref);

	static RemotePlayer *getplayer(ObjectRef *ref);

	// Exported functions

	// __tostring metamethod
	static int mt_tostring(lua_State *L);

	// garbage collector
	static int gc_object(lua_State *L);

	// remove(self)
	static int l_remove(lua_State *L);

	// is_valid(self)
	static int l_is_valid(lua_State *L);

	// get_guid()
	static int l_get_guid(lua_State *L);

	// get_pos(self)
	static int l_get_pos(lua_State *L);

	// set_pos(self, pos)
	static int l_set_pos(lua_State *L);

	// add_pos(self, pos)
	static int l_add_pos(lua_State *L);

	// move_to(self, pos, continuous)
	static int l_move_to(lua_State *L);

	// punch(self, puncher, time_from_last_punch, tool_capabilities, dir)
	static int l_punch(lua_State *L);

	// right_click(self, clicker)
	static int l_right_click(lua_State *L);

	// set_hp(self, hp, reason)
	static int l_set_hp(lua_State *L);

	// get_hp(self)
	static int l_get_hp(lua_State *L);

	// get_inventory(self)
	static int l_get_inventory(lua_State *L);

	// get_wield_list(self)
	static int l_get_wield_list(lua_State *L);

	// get_wield_index(self)
	static int l_get_wield_index(lua_State *L);

	// get_wielded_item(self)
	static int l_get_wielded_item(lua_State *L);

	// set_wielded_item(self, item)
	static int l_set_wielded_item(lua_State *L);

	// set_armor_groups(self, groups)
	static int l_set_armor_groups(lua_State *L);

	// get_armor_groups(self)
	static int l_get_armor_groups(lua_State *L);

	// set_physics_override(self, override_table)
	static int l_set_physics_override(lua_State *L);

	// get_physics_override(self)
	static int l_get_physics_override(lua_State *L);

	// set_animation(self, frame_range, frame_speed, frame_blend, frame_loop)
	static int l_set_animation(lua_State *L);

	// set_animation_frame_speed(self, frame_speed)
	static int l_set_animation_frame_speed(lua_State *L);

	// get_animation(self)
	static int l_get_animation(lua_State *L);

	// set_bone_position(self, bone, position, rotation)
	static int l_set_bone_position(lua_State *L);

	// get_bone_position(self, bone)
	static int l_get_bone_position(lua_State *L);

	// set_bone_override(self, bone)
	static int l_set_bone_override(lua_State *L);

	// get_bone_override(self, bone)
	static int l_get_bone_override(lua_State *L);

	// get_bone_override(self)
	static int l_get_bone_overrides(lua_State *L);

	// set_attach(self, parent, bone, position, rotation)
	static int l_set_attach(lua_State *L);

	// get_attach(self)
	static int l_get_attach(lua_State *L);

	// get_children(self)
	static int l_get_children(lua_State *L);

	// set_detach(self)
	static int l_set_detach(lua_State *L);

	// set_properties(self, properties)
	static int l_set_properties(lua_State *L);

	// get_properties(self)
	static int l_get_properties(lua_State *L);

	// set_observers(self, observers)
	static int l_set_observers(lua_State *L);

	// get_observers(self)
	static int l_get_observers(lua_State *L);

	// get_effective_observers(self)
	static int l_get_effective_observers(lua_State *L);

	// is_player(self)
	static int l_is_player(lua_State *L);

	/* LuaEntitySAO-only */

	// set_velocity(self, velocity)
	static int l_set_velocity(lua_State *L);

	// add_velocity(self, velocity)
	static int l_add_velocity(lua_State *L);

	// get_velocity(self)
	static int l_get_velocity(lua_State *L);

	// set_acceleration(self, acceleration)
	static int l_set_acceleration(lua_State *L);

	// get_acceleration(self)
	static int l_get_acceleration(lua_State *L);

	// set_rotation(self, rotation)
	static int l_set_rotation(lua_State *L);

	// get_rotation(self)
	static int l_get_rotation(lua_State *L);

	// set_yaw(self, yaw)
	static int l_set_yaw(lua_State *L);

	// get_yaw(self)
	static int l_get_yaw(lua_State *L);

	// set_texture_mod(self, mod)
	static int l_set_texture_mod(lua_State *L);

	// l_get_texture_mod(self)
	static int l_get_texture_mod(lua_State *L);

	// set_sprite(self, start_frame, num_frames, framelength, select_x_by_camera)
	static int l_set_sprite(lua_State *L);

	// DEPRECATED
	// get_entity_name(self)
	static int l_get_entity_name(lua_State *L);

	// get_luaentity(self)
	static int l_get_luaentity(lua_State *L);

	/* Player-only */

	// get_player_name(self)
	static int l_get_player_name(lua_State *L);

	// get_fov(self)
	static int l_get_fov(lua_State *L);

	// get_look_dir(self)
	static int l_get_look_dir(lua_State *L);

	// DEPRECATED
	// get_look_pitch(self)
	static int l_get_look_pitch(lua_State *L);

	// DEPRECATED
	// get_look_yaw(self)
	static int l_get_look_yaw(lua_State *L);

	// get_look_pitch2(self)
	static int l_get_look_vertical(lua_State *L);

	// get_look_yaw2(self)
	static int l_get_look_horizontal(lua_State *L);

	// set_fov(self, degrees, is_multiplier, transition_time)
	static int l_set_fov(lua_State *L);

	// set_look_vertical(self, radians)
	static int l_set_look_vertical(lua_State *L);

	// set_look_horizontal(self, radians)
	static int l_set_look_horizontal(lua_State *L);

	// DEPRECATED
	// set_look_pitch(self, radians)
	static int l_set_look_pitch(lua_State *L);

	// DEPRECATED
	// set_look_yaw(self, radians)
	static int l_set_look_yaw(lua_State *L);

	// set_breath(self, breath)
	static int l_set_breath(lua_State *L);

	// get_breath(self, breath)
	static int l_get_breath(lua_State *L);

	// DEPRECATED
	// set_attribute(self, attribute, value)
	static int l_set_attribute(lua_State *L);

	// DEPRECATED
	// get_attribute(self, attribute)
	static int l_get_attribute(lua_State *L);

	// get_meta(self)
	static int l_get_meta(lua_State *L);

	// set_inventory_formspec(self, formspec)
	static int l_set_inventory_formspec(lua_State *L);

	// get_inventory_formspec(self)
	static int l_get_inventory_formspec(lua_State *L);

	// set_formspec_prepend(self, formspec)
	static int l_set_formspec_prepend(lua_State *L);

	// get_formspec_prepend(self)
	static int l_get_formspec_prepend(lua_State *L);

	// get_player_control(self)
	static int l_get_player_control(lua_State *L);

	// get_player_control_bits(self)
	static int l_get_player_control_bits(lua_State *L);

	// hud_add(self, id, form)
	static int l_hud_add(lua_State *L);

	// hud_rm(self, id)
	static int l_hud_remove(lua_State *L);

	// hud_change(self, id, stat, data)
	static int l_hud_change(lua_State *L);

	// hud_get_next_id(self)
	static u32 hud_get_next_id(lua_State *L);

	// hud_get(self, id)
	static int l_hud_get(lua_State *L);

	// hud_get_all(self)
	static int l_hud_get_all(lua_State *L);

	// hud_set_flags(self, flags)
	static int l_hud_set_flags(lua_State *L);

	// hud_get_flags()
	static int l_hud_get_flags(lua_State *L);

	// hud_set_hotbar_itemcount(self, hotbar_itemcount)
	static int l_hud_set_hotbar_itemcount(lua_State *L);

	// hud_get_hotbar_itemcount(self)
	static int l_hud_get_hotbar_itemcount(lua_State *L);

	// hud_set_hotbar_image(self, name)
	static int l_hud_set_hotbar_image(lua_State *L);

	// hud_get_hotbar_image(self)
	static int l_hud_get_hotbar_image(lua_State *L);

	// hud_set_hotbar_selected_image(self, name)
	static int l_hud_set_hotbar_selected_image(lua_State *L);

	// hud_get_hotbar_selected_image(self)
	static int l_hud_get_hotbar_selected_image(lua_State *L);

	// set_sky(self, sky_parameters)
	static int l_set_sky(lua_State *L);

	// get_sky(self, as_table)
	static int l_get_sky(lua_State *L);

	// DEPRECATED
	// get_sky_color(self)
	static int l_get_sky_color(lua_State* L);

	// set_sun(self, sun_parameters)
	static int l_set_sun(lua_State *L);

	// get_sun(self)
	static int l_get_sun(lua_State *L);

	// set_moon(self, moon_parameters)
	static int l_set_moon(lua_State *L);

	// get_moon(self)
	static int l_get_moon(lua_State *L);

	// set_stars(self, star_parameters)
	static int l_set_stars(lua_State *L);

	// get_stars(self)
	static int l_get_stars(lua_State *L);

	// set_clouds(self, cloud_parameters)
	static int l_set_clouds(lua_State *L);

	// get_clouds(self)
	static int l_get_clouds(lua_State *L);

	// override_day_night_ratio(self, type)
	static int l_override_day_night_ratio(lua_State *L);

	// get_day_night_ratio(self)
	static int l_get_day_night_ratio(lua_State *L);

	// set_local_animation(self, idle, walk, dig, walk_while_dig, frame_speed)
	static int l_set_local_animation(lua_State *L);

	// get_local_animation(self)
	static int l_get_local_animation(lua_State *L);

	// set_eye_offset(self, firstperson, thirdperson, thirdperson_front)
	static int l_set_eye_offset(lua_State *L);

	// get_eye_offset(self)
	static int l_get_eye_offset(lua_State *L);

	// set_camera(self, {params})
	static int l_set_camera(lua_State *L);

	// get_camera(self)
	static int l_get_camera(lua_State *L);

	// set_nametag_attributes(self, attributes)
	static int l_set_nametag_attributes(lua_State *L);

	// get_nametag_attributes(self)
	static int l_get_nametag_attributes(lua_State *L);

	// send_mapblock(pos)
	static int l_send_mapblock(lua_State *L);

	// set_minimap_modes(self, modes, wanted_mode)
	static int l_set_minimap_modes(lua_State *L);

	// set_lighting(self, lighting)
	static int l_set_lighting(lua_State *L);

	// get_lighting(self)
	static int l_get_lighting(lua_State *L);

	// respawn(self)
	static int l_respawn(lua_State *L);

	// set_flags(self, flags)
	static int l_set_flags(lua_State *L);

	// get_flags(self)
	static int l_get_flags(lua_State *L);
};
