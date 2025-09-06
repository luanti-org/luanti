// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_particles.h"
#include "common/c_types.h"
#include "lua_api/l_object.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_particleparams.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "server.h"
#include "particles.h"

void LuaParticleParams::readTexValue(lua_State* L, ServerParticleTexture& tex)
{
	StackUnroller unroll(L);

	tex.animated = false;
	if (lua_isstring(L, -1)) {
		tex.string = lua_tostring(L, -1);
		return;
	}

	luaL_checktype(L, -1, LUA_TTABLE);
	lua_getfield(L, -1, "name");
	tex.string = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, -1, "animation");
	if (! lua_isnil(L, -1)) {
		tex.animated = true;
		tex.animation = read_animation_definition(L, -1);
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "blend");
	LuaParticleParams::readLuaValue(L, tex.blendmode);
	lua_pop(L, 1);

	LuaParticleParams::readTweenTable(L, "alpha", tex.alpha);
	LuaParticleParams::readTweenTable(L, "scale", tex.scale);

}

// add_particle({...})
int ModApiParticles::l_add_particle(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// Get parameters
	ParticleParameters p;
	std::string playername;

	if (lua_gettop(L) > 1) // deprecated
	{
		log_deprecated(L, "Deprecated add_particle call with "
			"individual parameters instead of definition");
		p.pos = check_v3f(L, 1);
		p.vel = check_v3f(L, 2);
		p.acc = check_v3f(L, 3);
		p.expirationtime = luaL_checknumber(L, 4);
		p.size = luaL_checknumber(L, 5);
		p.collisiondetection = readParam<bool>(L, 6);
		p.texture.string = luaL_checkstring(L, 7);
		if (lua_gettop(L) == 8) // only spawn for a single player
			playername = luaL_checkstring(L, 8);
	}
	else if (lua_istable(L, 1))
	{
		lua_getfield(L, 1, "pos");
		if (lua_istable(L, -1))
			p.pos = check_v3f(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "vel");
		if (lua_istable(L, -1)) {
			p.vel = check_v3f(L, -1);
			log_deprecated(L, "The use of vel is deprecated. "
				"Use velocity instead");
		}
		lua_pop(L, 1);

		lua_getfield(L, 1, "velocity");
		if (lua_istable(L, -1))
			p.vel = check_v3f(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "acc");
		if (lua_istable(L, -1)) {
			p.acc = check_v3f(L, -1);
			log_deprecated(L, "The use of acc is deprecated. "
				"Use acceleration instead");
		}
		lua_pop(L, 1);

		lua_getfield(L, 1, "acceleration");
		if (lua_istable(L, -1))
			p.acc = check_v3f(L, -1);
		lua_pop(L, 1);

		p.expirationtime = getfloatfield_default(L, 1, "expirationtime",
			p.expirationtime);
		p.size = getfloatfield_default(L, 1, "size", p.size);
		p.collisiondetection = getboolfield_default(L, 1,
			"collisiondetection", p.collisiondetection);
		p.collision_removal = getboolfield_default(L, 1,
			"collision_removal", p.collision_removal);
		p.object_collision = getboolfield_default(L, 1,
			"object_collision", p.object_collision);
		p.vertical = getboolfield_default(L, 1, "vertical", p.vertical);

		lua_getfield(L, 1, "animation");
		p.animation = read_animation_definition(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "texture");
		if (!lua_isnil(L, -1)) {
			LuaParticleParams::readTexValue(L, p.texture);
		}
		lua_pop(L, 1);

		p.glow = getintfield_default(L, 1, "glow", p.glow);

		lua_getfield(L, 1, "node");
		if (lua_istable(L, -1))
			p.node = readnode(L, -1);
		lua_pop(L, 1);

		p.node_tile = getintfield_default(L, 1, "node_tile", p.node_tile);

		playername = getstringfield_default(L, 1, "playername", "");

		lua_getfield(L, 1, "drag");
		if (lua_istable(L, -1))
			p.drag = check_v3f(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "jitter");
		LuaParticleParams::readLuaValue(L, p.jitter);
		lua_pop(L, 1);

		lua_getfield(L, 1, "bounce");
		LuaParticleParams::readLuaValue(L, p.bounce);
		lua_pop(L, 1);
	}

	getServer(L)->spawnParticle(playername, p);
	return 1;
}

// add_particlespawner({...})
int ModApiParticles::l_add_particlespawner(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// Get parameters
	ParticleSpawnerParameters p;
	ServerActiveObject *attached = NULL;
	std::string playername, not_playername;

	using namespace ParticleParamTypes;
	if (lua_gettop(L) > 1) //deprecated
	{
		log_deprecated(L, "Deprecated add_particlespawner call with "
			"individual parameters instead of definition");
		p.amount = luaL_checknumber(L, 1);
		p.time = luaL_checknumber(L, 2);
		auto minpos = check_v3f(L, 3);
		auto maxpos = check_v3f(L, 4);
		auto minvel = check_v3f(L, 5);
		auto maxvel = check_v3f(L, 6);
		auto minacc = check_v3f(L, 7);
		auto maxacc = check_v3f(L, 8);
		auto minexptime = luaL_checknumber(L, 9);
		auto maxexptime = luaL_checknumber(L, 10);
		auto minsize = luaL_checknumber(L, 11);
		auto maxsize = luaL_checknumber(L, 12);
		p.pos = v3fRange(minpos, maxpos);
		p.vel = v3fRange(minvel, maxvel);
		p.acc = v3fRange(minacc, maxacc);
		p.exptime = f32Range(minexptime, maxexptime);
		p.size = f32Range(minsize, maxsize);

		p.collisiondetection = readParam<bool>(L, 13);
		p.texture.string = luaL_checkstring(L, 14);
		if (lua_gettop(L) == 15) // only spawn for a single player
			playername = luaL_checkstring(L, 15);
	}
	else if (lua_istable(L, 1))
	{
		p.amount = getintfield_default(L, 1, "amount", p.amount);
		p.time = getfloatfield_default(L, 1, "time", p.time);

		// set default values
		p.exptime = 1;
		p.size = 1;

		// read spawner parameters from the table
		LuaParticleParams::readTweenTable(L, "pos", p.pos);
		LuaParticleParams::readTweenTable(L, "vel", p.vel);
		LuaParticleParams::readTweenTable(L, "acc", p.acc);
		LuaParticleParams::readTweenTable(L, "size", p.size);
		LuaParticleParams::readTweenTable(L, "exptime", p.exptime);
		LuaParticleParams::readTweenTable(L, "drag", p.drag);
		LuaParticleParams::readTweenTable(L, "jitter", p.jitter);
		LuaParticleParams::readTweenTable(L, "bounce", p.bounce);
		lua_getfield(L, 1, "attract");
		if (!lua_isnil(L, -1)) {
			luaL_checktype(L, -1, LUA_TTABLE);
			lua_getfield(L, -1, "kind");
			LuaParticleParams::readLuaValue(L, p.attractor_kind);
			lua_pop(L,1);

			lua_getfield(L, -1, "die_on_contact");
			if (!lua_isnil(L, -1))
				p.attractor_kill = readParam<bool>(L, -1);
			lua_pop(L,1);

			if (p.attractor_kind != AttractorKind::none) {
				LuaParticleParams::readTweenTable(L, "strength", p.attract);
				LuaParticleParams::readTweenTable(L, "origin", p.attractor_origin);
				p.attractor_attachment = LuaParticleParams::readAttachmentID(L, "origin_attached");
				if (p.attractor_kind != AttractorKind::point) {
					LuaParticleParams::readTweenTable(L, "direction", p.attractor_direction);
					p.attractor_direction_attachment = LuaParticleParams::readAttachmentID(L, "direction_attached");
				}
			}
		} else {
			p.attractor_kind = AttractorKind::none;
		}
		lua_pop(L,1);
		LuaParticleParams::readTweenTable(L, "radius", p.radius);

		p.collisiondetection = getboolfield_default(L, 1,
			"collisiondetection", p.collisiondetection);
		p.collision_removal = getboolfield_default(L, 1,
			"collision_removal", p.collision_removal);
		p.object_collision = getboolfield_default(L, 1,
			"object_collision", p.object_collision);

		lua_getfield(L, 1, "animation");
		p.animation = read_animation_definition(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "attached");
		if (!lua_isnil(L, -1)) {
			ObjectRef *ref = checkObject<ObjectRef>(L, -1);
			lua_pop(L, 1);
			attached = ObjectRef::getobject(ref);
		}

		lua_getfield(L, 1, "texture");
		if (!lua_isnil(L, -1)) {
			LuaParticleParams::readTexValue(L, p.texture);
		}
		lua_pop(L, 1);

		p.vertical = getboolfield_default(L, 1, "vertical", p.vertical);
		p.glow = getintfield_default(L, 1, "glow", p.glow);

		lua_getfield(L, 1, "texpool");
		if (lua_istable(L, -1)) {
			size_t tl = lua_objlen(L, -1);
			p.texpool.reserve(tl);
			for (size_t i = 0; i < tl; ++i) {
				lua_pushinteger(L, i+1), lua_gettable(L, -2);
				p.texpool.emplace_back();
				LuaParticleParams::readTexValue(L, p.texpool.back());
				lua_pop(L,1);
			}
		}
		lua_pop(L, 1);

		lua_getfield(L, 1, "node");
		if (lua_istable(L, -1))
			p.node = readnode(L, -1);
		lua_pop(L, 1);

		p.node_tile = getintfield_default(L, 1, "node_tile", p.node_tile);

		// meta parameters
		playername = getstringfield_default(L, 1, "playername", "");
		not_playername = getstringfield_default(L, 1, "exclude_player", "");
	}

	if (p.time < 0)
		throw LuaError("particle spawner 'time' must be >= 0");

	u32 id = getServer(L)->addParticleSpawner(p, attached, playername, not_playername);
	lua_pushnumber(L, id);

	return 1;
}

// delete_particlespawner(id, player)
// player (string) is optional
int ModApiParticles::l_delete_particlespawner(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// Get parameters
	u32 id = luaL_checknumber(L, 1);
	std::string playername;
	if (lua_gettop(L) == 2) {
		playername = luaL_checkstring(L, 2);
	}

	getServer(L)->deleteParticleSpawner(playername, id);
	return 1;
}

void ModApiParticles::Initialize(lua_State *L, int top)
{
	API_FCT(add_particle);
	API_FCT(add_particlespawner);
	API_FCT(delete_particlespawner);
}

