// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 red-001 <red-001@outlook.ie>

#include "lua_api/l_particles_local.h"
#include "common/c_content.h"
#include "common/c_converter.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_object.h"
#include "lua_api/l_particleparams.h"
#include "client/particles.h"
#include "client/client.h"
#include "client/clientevent.h"

int ModApiParticlesLocal::l_add_particle(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	// Get parameters
	ParticleParameters p;

	lua_getfield(L, 1, "pos");
	if (lua_istable(L, -1))
		p.pos = check_v3f(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "velocity");
	if (lua_istable(L, -1))
		p.vel = check_v3f(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "acceleration");
	if (lua_istable(L, -1))
		p.acc = check_v3f(L, -1);
	lua_pop(L, 1);

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
		LuaParticleParams::readTexValue(L,p.texture);
	}
	lua_pop(L, 1);
	p.glow = getintfield_default(L, 1, "glow", p.glow);

	lua_getfield(L, 1, "node");
	if (lua_istable(L, -1))
		p.node = readnode(L, -1);
	lua_pop(L, 1);

	p.node_tile = getintfield_default(L, 1, "node_tile", p.node_tile);

	ClientEvent *event = new ClientEvent();
	event->type           = CE_SPAWN_PARTICLE;
	event->spawn_particle = new ParticleParameters(p);
	getClient(L)->pushToEventQueue(event);

	return 0;
}

int ModApiParticlesLocal::l_add_particlespawner(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	// Get parameters
	ParticleSpawnerParameters p;
	p.amount = getintfield_default(L, 1, "amount", p.amount);
	p.time = getfloatfield_default(L, 1, "time", p.time);

	// set default values
	p.exptime = 1;
	p.size = 1;

	// read spawner parameters from the table
	using namespace ParticleParamTypes;
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

	u64 id = getClient(L)->getParticleManager()->generateSpawnerId();

	auto event = new ClientEvent();
	event->type                            = CE_ADD_PARTICLESPAWNER;
	event->add_particlespawner.p           = new ParticleSpawnerParameters(p);
	event->add_particlespawner.attached_id = 0;
	event->add_particlespawner.id          = id;

	getClient(L)->pushToEventQueue(event);
	lua_pushnumber(L, id);

	return 1;
}

int ModApiParticlesLocal::l_delete_particlespawner(lua_State *L)
{
	// Get parameters
	u32 id = luaL_checknumber(L, 1);

	ClientEvent *event = new ClientEvent();
	event->type                      = CE_DELETE_PARTICLESPAWNER;
	event->delete_particlespawner.id = id;

	getClient(L)->pushToEventQueue(event);
	return 0;
}

void ModApiParticlesLocal::Initialize(lua_State *L, int top)
{
	API_FCT(add_particle);
	API_FCT(add_particlespawner);
	API_FCT(delete_particlespawner);
}
