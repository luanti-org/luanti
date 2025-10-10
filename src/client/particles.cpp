// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "particles.h"
#include <cmath>
#include <array>
#include "client.h"
#include "collision.h"
#include "client/content_cao.h"
#include "client/clientevent.h"
#include "client/renderingengine.h"
#include "client/texturesource.h"
#include "util/numeric.h"
#include "light.h"
#include "localplayer.h"
#include "environment.h"
#include "clientmap.h"
#include "mapnode.h"
#include "nodedef.h"
#include "client.h"
#include "settings.h"
#include "profiler.h"

#include "CMeshBuffer.h"

using BlendMode = ParticleParamTypes::BlendMode;

ClientParticleTexture::ClientParticleTexture(const ServerParticleTexture& p, ITextureSource *tsrc)
{
	tex = p;
	// note: getTextureForMesh not needed here because we don't use texture filtering
	ref = tsrc->getTexture(p.string);

	// Try to show another texture to indicate a code issue.
	if (!ref)
		ref = tsrc->getTexture("no_texture.png");
}

/*
	Particle
*/

Particle::Particle(
		const ParticleParameters &p,
		const ClientParticleTexRef &texture,
		v2f texpos,
		v2f texsize,
		video::SColor color,
		ParticleSpawner *parent,
		std::unique_ptr<ClientParticleTexture> owned_texture
	) :
		m_expiration(p.expirationtime),

		m_base_color(color),

		m_texture(texture),
		m_texpos(texpos),
		m_texsize(texsize),
		m_pos(p.pos),
		m_velocity(p.vel),
		m_acceleration(p.acc),
		m_p(p),

		m_parent(parent),
		m_owned_texture(std::move(owned_texture))
{
}

Particle::~Particle()
{
	if (m_buffer)
		m_buffer->release(m_index);
}

bool Particle::attachToBuffer(ParticleBuffer *buffer)
{
	auto index_opt = buffer->allocate();
	if (index_opt.has_value()) {
		m_index = index_opt.value();
		m_buffer = buffer;
		return true;
	}
	return false;
}

void Particle::step(float dtime, ClientEnvironment *env)
{
	m_time += dtime;

	// apply drag (not handled by collisionMoveSimple) and brownian motion
	v3f av = vecAbsolute(m_velocity);
	av -= av * (m_p.drag * dtime);
	m_velocity = av*vecSign(m_velocity) + v3f(m_p.jitter.pickWithin())*dtime;

	if (m_p.collisiondetection) {
		aabb3f box(v3f(-m_p.size / 2.0f), v3f(m_p.size / 2.0f));
		v3f p_pos = m_pos * BS;
		v3f p_velocity = m_velocity * BS;
		collisionMoveResult r = collisionMoveSimple(env, env->getGameDef(),
			box, 0.0f, dtime, &p_pos, &p_velocity, m_acceleration * BS, nullptr,
			m_p.object_collision);

		f32 bounciness = m_p.bounce.pickWithin();
		if (r.collides && (m_p.collision_removal || bounciness > 0)) {
			if (m_p.collision_removal) {
				// force expiration of the particle
				m_expiration = -1.0f;
			} else if (bounciness > 0) {
				/* cheap way to get a decent bounce effect is to only invert the
				 * largest component of the velocity vector, so e.g. you don't
				 * have a rock immediately bounce back in your face when you try
				 * to skip it across the water (as would happen if we simply
				 * downscaled and negated the velocity vector). this means
				 * bounciness will work properly for cubic objects, but meshes
				 * with diagonal angles and entities will not yield the correct
				 * visual. this is probably unavoidable */
				if (av.Y > av.X && av.Y > av.Z) {
					m_velocity.Y = -(m_velocity.Y * bounciness);
				} else if (av.X > av.Y && av.X > av.Z) {
					m_velocity.X = -(m_velocity.X * bounciness);
				} else if (av.Z > av.Y && av.Z > av.X) {
					m_velocity.Z = -(m_velocity.Z * bounciness);
				} else { // well now we're in a bit of a pickle
					m_velocity = -(m_velocity * bounciness);
				}
			}
		} else {
			m_velocity = p_velocity / BS;
		}
		m_pos = p_pos / BS;
	} else {
		// apply velocity and acceleration to position
		m_pos += (m_velocity + m_acceleration * 0.5f * dtime) * dtime;
		// apply acceleration to velocity
		m_velocity += m_acceleration * dtime;
	}

	if (m_p.animation.type != TAT_NONE) {
		m_animation_time += dtime;
		int frame_length_i = 0;
		m_p.animation.determineParams(
				m_texture.ref->getSize(),
				NULL, &frame_length_i, NULL);
		float frame_length = frame_length_i / 1000.0;
		while (m_animation_time > frame_length) {
			m_animation_frame++;
			m_animation_time -= frame_length;
		}
	}

	// animate particle alpha in accordance with settings
	float alpha = 1.f;
	if (m_texture.tex != nullptr)
		alpha = m_texture.tex -> alpha.blend(m_time / (m_expiration+0.1f));

	// Update lighting
	auto col = updateLight(env);
	col.setAlpha(255 * alpha);

	// Update model
	updateVertices(env, col);
}

video::SColor Particle::updateLight(ClientEnvironment *env)
{
	u8 light = 0;
	bool pos_ok;

	v3s16 p = v3s16(
		floor(m_pos.X+0.5),
		floor(m_pos.Y+0.5),
		floor(m_pos.Z+0.5)
	);
	MapNode n = env->getClientMap().getNode(p, &pos_ok);
	if (pos_ok)
		light = n.getLightBlend(env->getDayNightRatio(),
				env->getGameDef()->ndef()->getLightingFlags(n));
	else
		light = blend_light(env->getDayNightRatio(), LIGHT_SUN, 0);

	u8 m_light = decode_light(light + m_p.glow);
	return video::SColor(255,
		m_light * m_base_color.getRed() / 255,
		m_light * m_base_color.getGreen() / 255,
		m_light * m_base_color.getBlue() / 255);
}

void Particle::updateVertices(ClientEnvironment *env, video::SColor color)
{
	f32 tx0, tx1, ty0, ty1;
	v2f scale;

	if (!m_buffer)
		return;

	video::S3DVertex *vertices = m_buffer->getVertices(m_index);

	if (m_texture.tex != nullptr)
		scale = m_texture.tex -> scale.blend(m_time / (m_expiration+0.1f));
	else
		scale = v2f(1.f, 1.f);

	if (m_p.animation.type != TAT_NONE) {
		const v2u32 texsize = m_texture.ref->getSize();
		v2f texcoord, framesize_f;
		v2u32 framesize;
		texcoord = m_p.animation.getTextureCoords(texsize, m_animation_frame);
		m_p.animation.determineParams(texsize, NULL, NULL, &framesize);
		framesize_f = v2f::from(framesize) / v2f::from(texsize);

		tx0 = m_texpos.X + texcoord.X;
		tx1 = m_texpos.X + texcoord.X + framesize_f.X * m_texsize.X;
		ty0 = m_texpos.Y + texcoord.Y;
		ty1 = m_texpos.Y + texcoord.Y + framesize_f.Y * m_texsize.Y;
	} else {
		tx0 = m_texpos.X;
		tx1 = m_texpos.X + m_texsize.X;
		ty0 = m_texpos.Y;
		ty1 = m_texpos.Y + m_texsize.Y;
	}

	auto half = m_p.size * .5f,
	     hx   = half * scale.X,
	     hy   = half * scale.Y;
	vertices[0] = video::S3DVertex(-hx, -hy,
		0, 0, 0, 0, color, tx0, ty1);
	vertices[1] = video::S3DVertex(hx, -hy,
		0, 0, 0, 0, color, tx1, ty1);
	vertices[2] = video::S3DVertex(hx, hy,
		0, 0, 0, 0, color, tx1, ty0);
	vertices[3] = video::S3DVertex(-hx, hy,
		0, 0, 0, 0, color, tx0, ty0);

	// Update position -- see #10398
	auto *player = env->getLocalPlayer();
	v3s16 camera_offset = env->getCameraOffset();

	for (u16 i = 0; i < 4; i++) {
		video::S3DVertex &vertex = vertices[i];
		if (m_p.vertical) {
			v3f ppos = player->getPosition() / BS;
			vertex.Pos.rotateXZBy(std::atan2(ppos.Z - m_pos.Z, ppos.X - m_pos.X) /
				core::DEGTORAD + 90);
		} else {
			vertex.Pos.rotateYZBy(player->getPitch());
			vertex.Pos.rotateXZBy(player->getYaw());
		}
		vertex.Pos += m_pos * BS - intToFloat(camera_offset, BS);
	}
}

/*
	ParticleSpawner
*/

ParticleSpawner::ParticleSpawner(
		LocalPlayer *player,
		const ParticleSpawnerParameters &params,
		u16 attached_id,
		std::vector<ClientParticleTexture> &&texpool,
		ParticleManager *p_manager
	) :
		m_active(0),
		m_particlemanager(p_manager),
		m_time(0.0f),
		m_player(player),
		p(params),
		m_texpool(std::move(texpool)),
		m_attached_id(attached_id)
{
	m_spawntimes.reserve(p.amount + 1);
	for (u16 i = 0; i <= p.amount; i++) {
		float spawntime = myrand_float() * p.time;
		m_spawntimes.push_back(spawntime);
	}

	size_t max_particles = 0; // maximum number of particles likely to be visible at any given time
	assert(p.time >= 0);
	if (p.time != 0) {
		auto maxGenerations = p.time / std::min(p.exptime.start.min, p.exptime.end.min);
		max_particles = p.amount / maxGenerations;
	} else {
		auto longestLife = std::max(p.exptime.start.max, p.exptime.end.max);
		max_particles = p.amount * longestLife;
	}

	p_manager->reserveParticleSpace(max_particles * 1.2);
}

namespace {
	GenericCAO *findObjectByID(ClientEnvironment *env, u16 id) {
		if (id == 0)
			return nullptr;
		return env->getGenericCAO(id);
	}
}

void ParticleSpawner::spawnParticle(ClientEnvironment *env, float radius,
	const core::matrix4 *attached_absolute_pos_rot_matrix)
{
	float fac = 0;
	if (p.time != 0) { // ensure safety from divide-by-zeroes
		fac = m_time / (p.time+0.1f);
	}

	auto r_pos    = p.pos.blend(fac);
	auto r_vel    = p.vel.blend(fac);
	auto r_acc    = p.acc.blend(fac);
	auto r_drag   = p.drag.blend(fac);
	auto r_radius = p.radius.blend(fac);
	auto r_jitter = p.jitter.blend(fac);
	auto r_bounce = p.bounce.blend(fac);
	v3f  attractor_origin    = p.attractor_origin.blend(fac);
	v3f  attractor_direction = p.attractor_direction.blend(fac);
	auto attractor_obj           = findObjectByID(env, p.attractor_attachment);
	auto attractor_direction_obj = findObjectByID(env, p.attractor_direction_attachment);

	auto r_exp     = p.exptime.blend(fac);
	auto r_size    = p.size.blend(fac);
	auto r_attract = p.attract.blend(fac);
	auto attract   = r_attract.pickWithin();

	v3f ppos = m_player->getPosition() / BS;
	v3f pos = r_pos.pickWithin();
	v3f sphere_radius = r_radius.pickWithin();

	// Need to apply this first or the following check
	// will be wrong for attached spawners
	if (attached_absolute_pos_rot_matrix) {
		pos *= BS;
		attached_absolute_pos_rot_matrix->transformVect(pos);
		pos /= BS;
		v3s16 camera_offset = m_particlemanager->m_env->getCameraOffset();
		pos.X += camera_offset.X;
		pos.Y += camera_offset.Y;
		pos.Z += camera_offset.Z;
	}

	if (pos.getDistanceFromSQ(ppos) > radius*radius)
		return;

	// Parameters for the single particle we're about to spawn
	ParticleParameters pp;
	pp.pos = pos;

	pp.vel = r_vel.pickWithin();
	pp.acc = r_acc.pickWithin();
	pp.drag = r_drag.pickWithin();
	pp.jitter = r_jitter;
	pp.bounce = r_bounce;

	if (attached_absolute_pos_rot_matrix) {
		// Apply attachment rotation
		pp.vel = attached_absolute_pos_rot_matrix->rotateAndScaleVect(pp.vel);
		pp.acc = attached_absolute_pos_rot_matrix->rotateAndScaleVect(pp.acc);
	}

	if (attractor_obj)
		attractor_origin += attractor_obj->getPosition() / BS;
	if (attractor_direction_obj) {
		auto *attractor_absolute_pos_rot_matrix = attractor_direction_obj->getAbsolutePosRotMatrix();
		if (attractor_absolute_pos_rot_matrix) {
			attractor_direction = attractor_absolute_pos_rot_matrix
					->rotateAndScaleVect(attractor_direction);
		}
	}

	pp.expirationtime = r_exp.pickWithin();

	if (sphere_radius != v3f()) {
		f32 l = sphere_radius.getLength();
		v3f mag = sphere_radius;
		mag.normalize();

		v3f ofs = v3f(l,0,0);
		ofs.rotateXZBy(myrand_range(0.f,360.f));
		ofs.rotateYZBy(myrand_range(0.f,360.f));
		ofs.rotateXYBy(myrand_range(0.f,360.f));

		pp.pos += ofs * mag;
	}

	if (p.attractor_kind != ParticleParamTypes::AttractorKind::none && attract != 0) {
		v3f dir;
		f32 dist = 0; /* =0 necessary to silence warning */
		switch (p.attractor_kind) {
			case ParticleParamTypes::AttractorKind::none:
				break;

			case ParticleParamTypes::AttractorKind::point: {
				dist = pp.pos.getDistanceFrom(attractor_origin);
				dir = pp.pos - attractor_origin;
				dir.normalize();
				break;
			}

			case ParticleParamTypes::AttractorKind::line: {
				// <https://github.com/luanti-org/luanti/issues/11505#issuecomment-915612700>
				const auto& lorigin = attractor_origin;
				v3f ldir = attractor_direction;
				ldir.normalize();
				auto origin_to_point = pp.pos - lorigin;
				auto scalar_projection = origin_to_point.dotProduct(ldir);
				auto point_on_line = lorigin + (ldir * scalar_projection);

				dist = pp.pos.getDistanceFrom(point_on_line);
				dir = (point_on_line - pp.pos);
				dir.normalize();
				dir *= -1; // flip it around so strength=1 attracts, not repulses
				break;
			}

			case ParticleParamTypes::AttractorKind::plane: {
				// <https://github.com/luanti-org/luanti/issues/11505#issuecomment-915612700>
				const v3f& porigin = attractor_origin;
				v3f normal = attractor_direction;
				normal.normalize();
				v3f point_to_origin = porigin - pp.pos;
				f32 factor = normal.dotProduct(point_to_origin);
				if (numericAbsolute(factor) == 0.0f) {
					dir = normal;
				} else {
					factor = numericSign(factor);
					dir = normal * factor;
				}
				dist = numericAbsolute(normal.dotProduct(pp.pos - porigin));
				dir *= -1; // flip it around so strength=1 attracts, not repulses
				break;
			}
		}

		f32 speedTowards = numericAbsolute(attract) * dist;
		v3f avel = dir * speedTowards;
		if (attract > 0 && speedTowards > 0) {
			avel *= -1;
			if (p.attractor_kill) {
				// make sure the particle dies after crossing the attractor threshold
				f32 timeToCenter = dist / speedTowards;
				if (timeToCenter < pp.expirationtime)
					pp.expirationtime = timeToCenter;
			}
		}
		pp.vel += avel;
	}

	p.copyCommon(pp);

	ClientParticleTexRef texture;
	v2f texpos, texsize;
	video::SColor color(0xFFFFFFFF);

	if (p.node.getContent() != CONTENT_IGNORE) {
		const ContentFeatures &f =
			m_particlemanager->m_env->getGameDef()->ndef()->get(p.node);
		if (!ParticleManager::getNodeParticleParams(p.node, f, pp, &texture.ref,
				texpos, texsize, &color, p.node_tile))
			return;
	} else {
		if (m_texpool.size() == 0)
			return;
		texture = ClientParticleTexRef(m_texpool[myrand_range(0, m_texpool.size() - 1)]);
		texpos = v2f(0.0f, 0.0f);
		texsize = v2f(1.0f, 1.0f);
		if (texture.tex->animated)
			pp.animation = texture.tex->animation;
	}

	// Same guard as in `CE_SPAWN_PARTICLE`
	if (!texture.ref)
		return;

	// synchronize animation length with particle life if desired
	if (pp.animation.type != TAT_NONE) {
		// FIXME: this should be moved into a TileAnimationParams class method
		if (pp.animation.type == TAT_VERTICAL_FRAMES &&
			pp.animation.vertical_frames.length < 0) {
			auto& a = pp.animation.vertical_frames;
			// we add a tiny extra value to prevent the first frame
			// from flickering back on just before the particle dies
			a.length = (pp.expirationtime / -a.length) + 0.1;
		} else if (pp.animation.type == TAT_SHEET_2D &&
				   pp.animation.sheet_2d.frame_length < 0) {
			auto& a = pp.animation.sheet_2d;
			auto frames = a.frames_w * a.frames_h;
			auto runtime = (pp.expirationtime / -a.frame_length) + 0.1;
			pp.animation.sheet_2d.frame_length = frames / runtime;
		}
	}

	// Allow keeping default random size
	if (p.size.start.max > 0.0f || p.size.end.max > 0.0f)
		pp.size = r_size.pickWithin();

	++m_active;
	m_particlemanager->addParticle(std::make_unique<Particle>(
			pp,
			texture,
			texpos,
			texsize,
			color,
			this
		));
}

void ParticleSpawner::step(float dtime, ClientEnvironment *env)
{
	m_time += dtime;

	static thread_local const float radius =
			g_settings->getS16("max_block_send_distance") * MAP_BLOCKSIZE;

	bool unloaded = false;
	const core::matrix4 *attached_absolute_pos_rot_matrix = nullptr;
	if (m_attached_id) {
		if (GenericCAO *attached = env->getGenericCAO(m_attached_id)) {
			attached_absolute_pos_rot_matrix = attached->getAbsolutePosRotMatrix();
		} else {
			unloaded = true;
		}
	}

	if (p.time != 0) {
		// Spawner exists for a predefined timespan
		for (auto i = m_spawntimes.begin(); i != m_spawntimes.end(); ) {
			if ((*i) <= m_time && p.amount > 0) {
				--p.amount;

				// Pretend to, but don't actually spawn a particle if it is
				// attached to an unloaded object or distant from player.
				if (!unloaded)
					spawnParticle(env, radius, attached_absolute_pos_rot_matrix);

				i = m_spawntimes.erase(i);
			} else {
				++i;
			}
		}
	} else {
		// Spawner exists for an infinity timespan, spawn on a per-second base

		// Skip this step if attached to an unloaded object
		if (unloaded)
			return;

		for (int i = 0; i <= p.amount; i++) {
			if (myrand_float() < dtime)
				spawnParticle(env, radius, attached_absolute_pos_rot_matrix);
		}
	}
}

/*
	ParticleBuffer
*/

ParticleBuffer::ParticleBuffer(ClientEnvironment *env, const video::SMaterial &material)
	: scene::ISceneNode(
			env->getGameDef()->getSceneManager()->getRootSceneNode(),
			env->getGameDef()->getSceneManager()),
	m_mesh_buffer(make_irr<scene::SMeshBuffer>())
{
	m_mesh_buffer->getMaterial() = material;
}

static constexpr u16 quad_indices[] = { 0, 1, 2, 2, 3, 0 };

std::optional<u16> ParticleBuffer::allocate()
{
	u16 index;

	m_usage_timer = 0;

	if (!m_free_list.empty()) {
		index = m_free_list.back();
		m_free_list.pop_back();
		auto *vertices = static_cast<video::S3DVertex*>(m_mesh_buffer->getVertices());
		u16 *indices = m_mesh_buffer->getIndices();
		// reset vertices, because it is only written in Particle::step()
		for (u16 i = 0; i < 4; i++)
			vertices[4 * index + i] = video::S3DVertex();
		for (u16 i = 0; i < 6; i++)
			indices[6 * index + i] = 4 * index + quad_indices[i];
		return index;
	}

	if (m_count >= MAX_PARTICLES_PER_BUFFER)
		return std::nullopt;

	// append new vertices
	// note: Our buffer never gets smaller, but ParticleManager will delete
	//       us after a while.
	std::array<video::S3DVertex, 4> vertices {};
	m_mesh_buffer->append(&vertices.front(), 4, quad_indices, 6);
	index = m_count++;
	return index;
}

void ParticleBuffer::release(u16 index)
{
	assert(index < m_count);
	u16 *indices = m_mesh_buffer->getIndices();
	for (u16 i = 0; i < 6; i++)
		indices[6 * index + i] = 0;
	m_free_list.push_back(index);
}

video::S3DVertex *ParticleBuffer::getVertices(u16 index)
{
	if (index >= m_count)
		return nullptr;
	m_bounding_box_dirty = true;
	return &(static_cast<video::S3DVertex *>(m_mesh_buffer->getVertices())[4 * index]);
}

void ParticleBuffer::OnRegisterSceneNode()
{
	if (IsVisible) {
		SceneManager->registerNodeForRendering(this,
				m_mesh_buffer->getMaterial().MaterialType == video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF
				? scene::ESNRP_SOLID : scene::ESNRP_TRANSPARENT_EFFECT);
	}
	scene::ISceneNode::OnRegisterSceneNode();
}

const core::aabbox3df &ParticleBuffer::getBoundingBox() const
{
	if (!m_bounding_box_dirty)
		return m_mesh_buffer->BoundingBox;

	core::aabbox3df box{{0, 0, 0}};
	bool first = true;
	for (u16 i = 0; i < m_count; i++) {
		// check if this index is used
		static_assert(quad_indices[1] != 0);
		if (m_mesh_buffer->getIndices()[6 * i + 1] == 0)
			continue;

		for (u16 j = 0; j < 4; j++) {
			const auto pos = m_mesh_buffer->getPosition(i * 4 + j);
			if (first)
				box.reset(pos);
			else
				box.addInternalPoint(pos);
			first = false;
		}
	}

	m_mesh_buffer->BoundingBox = box;
	m_bounding_box_dirty = false;
	return m_mesh_buffer->BoundingBox;
}

void ParticleBuffer::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();

	if (isEmpty())
		return;

	driver->setTransform(video::ETS_WORLD, core::matrix4());
	driver->setMaterial(m_mesh_buffer->getMaterial());
	driver->drawMeshBuffer(m_mesh_buffer.get());
}

/*
	ParticleManager
*/

ParticleManager::ParticleManager(ClientEnvironment *env) :
	m_env(env)
{}

ParticleManager::~ParticleManager()
{
	clearAll();
}

void ParticleManager::step(float dtime)
{
	stepParticles(dtime);
	stepSpawners(dtime);
	stepBuffers(dtime);
}

void ParticleManager::stepSpawners(float dtime)
{
	MutexAutoLock lock(m_spawner_list_lock);

	for (size_t i = 0; i < m_dying_particle_spawners.size();) {
		// the particlespawner owns the textures, so we need to make
		// sure there are no active particles before we free it
		if (!m_dying_particle_spawners[i]->hasActive()) {
			m_dying_particle_spawners[i] = std::move(m_dying_particle_spawners.back());
			m_dying_particle_spawners.pop_back();
		} else {
			++i;
		}
	}

	for (auto it = m_particle_spawners.begin(); it != m_particle_spawners.end();) {
		auto &ps = it->second;
		if (ps->getExpired()) {
			// same as above
			if (ps->hasActive())
				m_dying_particle_spawners.push_back(std::move(ps));
			it = m_particle_spawners.erase(it);
		} else {
			ps->step(dtime, m_env);
			++it;
		}
	}
}

void ParticleManager::stepParticles(float dtime)
{
	MutexAutoLock lock(m_particle_list_lock);

	for (size_t i = 0; i < m_particles.size();) {
		Particle &p = *m_particles[i];
		if (p.isExpired()) {
			ParticleSpawner *parent = p.getParent();
			if (parent) {
				assert(parent->hasActive());
				parent->decrActive();
			}
			// delete
			m_particles[i] = std::move(m_particles.back());
			m_particles.pop_back();
		} else {
			p.step(dtime, m_env);
			++i;
		}
	}
}

void ParticleManager::stepBuffers(float dtime)
{
	constexpr float INTERVAL = 0.5f;
	if (!m_buffer_gc.step(dtime, INTERVAL))
		return;

	MutexAutoLock lock(m_particle_list_lock);

	// remove buffers that have been unused for 5 seconds
	size_t alloc = 0;
	for (size_t i = 0; i < m_particle_buffers.size(); ) {
		auto &buf = m_particle_buffers[i];
		buf->m_usage_timer += INTERVAL;
		if (buf->isEmpty() && buf->m_usage_timer > 5.0f) {
			// delete and swap with last
			buf->remove();
			buf = std::move(m_particle_buffers.back());
			m_particle_buffers.pop_back();
		} else {
			i++;
			alloc += buf->m_count;
		}
	}

	g_profiler->avg("ParticleManager: particle buffer count [#]", m_particle_buffers.size());
	if (!m_particle_buffers.empty())
		g_profiler->avg("ParticleManager: buffer allocated size [#]", alloc);
}

void ParticleManager::clearAll()
{
	MutexAutoLock lock(m_spawner_list_lock);
	MutexAutoLock lock2(m_particle_list_lock);

	m_particle_spawners.clear();
	m_dying_particle_spawners.clear();

	m_particles.clear();

	// have to remove from scene first because it keeps a reference
	for (auto &it : m_particle_buffers)
		it->remove();
	m_particle_buffers.clear();
}

void ParticleManager::handleParticleEvent(ClientEvent *event, Client *client,
	LocalPlayer *player)
{
	switch (event->type) {
		case CE_DELETE_PARTICLESPAWNER: {
			deleteParticleSpawner(event->delete_particlespawner.id);
			// no allocated memory in delete event
			break;
		}
		case CE_ADD_PARTICLESPAWNER: {
			deleteParticleSpawner(event->add_particlespawner.id);

			const ParticleSpawnerParameters &p = *event->add_particlespawner.p;

			// There can be multiple textures, e.g. for time-based animations
			// Look up all required textures in `ITextureSource` to retrieve an `ITexture`.
			std::vector<ClientParticleTexture> texpool;
			if (!p.texpool.empty()) {
				size_t txpsz = p.texpool.size();
				texpool.reserve(txpsz);
				for (size_t i = 0; i < txpsz; ++i) {
					texpool.emplace_back(p.texpool[i], client->tsrc());
				}
			} else {
				// no texpool in use, use fallback texture
				texpool.emplace_back(p.texture, client->tsrc());
			}

			addParticleSpawner(event->add_particlespawner.id,
					std::make_unique<ParticleSpawner>(
						player,
						p,
						event->add_particlespawner.attached_id,
						std::move(texpool),
						this)
					);

			delete event->add_particlespawner.p;
			break;
		}
		case CE_SPAWN_PARTICLE: {
			ParticleParameters &p = *event->spawn_particle;

			ClientParticleTexRef texture;
			std::unique_ptr<ClientParticleTexture> texstore;
			v2f texpos, texsize;
			video::SColor color(0xFFFFFFFF);

			f32 oldsize = p.size;

			if (p.node.getContent() != CONTENT_IGNORE) {
				const ContentFeatures &f = m_env->getGameDef()->ndef()->get(p.node);
				getNodeParticleParams(p.node, f, p, &texture.ref, texpos,
						texsize, &color, p.node_tile);
			} else {
				/* with no particlespawner to own the texture, we need
				 * to save it on the heap. it will be freed when the
				 * particle is destroyed */
				texstore = std::make_unique<ClientParticleTexture>(p.texture, client->tsrc());

				texture = ClientParticleTexRef(*texstore);
				texpos = v2f(0.0f, 0.0f);
				texsize = v2f(1.0f, 1.0f);
			}

			// Allow keeping default random size
			if (oldsize > 0.0f)
				p.size = oldsize;

			if (texture.ref) {
				addParticle(std::make_unique<Particle>(
						p, texture, texpos, texsize, color, nullptr,
						std::move(texstore)));
			}

			delete event->spawn_particle;
			break;
		}
		default: break;
	}
}

bool ParticleManager::getNodeParticleParams(const MapNode &n,
	const ContentFeatures &f, ParticleParameters &p, video::ITexture **texture,
	v2f &texpos, v2f &texsize, video::SColor *color, u8 tilenum)
{
	// No particles for "airlike" nodes
	if (f.drawtype == NDT_AIRLIKE)
		return false;

	// Texture
	u8 texid;
	if (tilenum > 0 && tilenum <= 6)
		texid = tilenum - 1;
	else
		texid = myrand_range(0,5);
	const TileLayer &tile = f.tiles[texid].layers[0];
	p.animation.type = TAT_NONE;

	// Only use first frame of animated texture
	if (tile.material_flags & MATERIAL_FLAG_ANIMATION)
		*texture = (*tile.frames)[0].texture;
	else
		*texture = tile.texture;

	float size = (myrand_range(0,8)) / 64.0f;
	p.size = BS * size;
	if (tile.scale)
		size /= tile.scale;
	texsize = v2f(size * 2.0f, size * 2.0f);
	texpos.X = (myrand_range(0,64)) / 64.0f - texsize.X;
	texpos.Y = (myrand_range(0,64)) / 64.0f - texsize.Y;

	if (tile.has_color)
		*color = tile.color;
	else
		n.getColor(f, color);

	return true;
}

// The final burst of particles when a node is finally dug, *not* particles
// spawned during the digging of a node.

void ParticleManager::addDiggingParticles(IGameDef *gamedef,
	LocalPlayer *player, v3s16 pos, const MapNode &n, const ContentFeatures &f)
{
	// No particles for "airlike" nodes
	if (f.drawtype == NDT_AIRLIKE)
		return;

	for (u16 j = 0; j < 16; j++) {
		addNodeParticle(gamedef, player, pos, n, f);
	}
}

// During the digging of a node particles are spawned individually by this
// function, called from Game::handleDigging() in game.cpp.

void ParticleManager::addNodeParticle(IGameDef *gamedef,
	LocalPlayer *player, v3s16 pos, const MapNode &n, const ContentFeatures &f)
{
	ParticleParameters p;
	video::ITexture *ref = nullptr;
	v2f texpos, texsize;
	video::SColor color;

	if (!getNodeParticleParams(n, f, p, &ref, texpos, texsize, &color))
		return;

	p.texture.blendmode = f.alpha == ALPHAMODE_BLEND
			? BlendMode::alpha : BlendMode::clip;

	p.expirationtime = myrand_range(0, 100) / 100.0f;

	// Physics
	p.vel = v3f(
		myrand_range(-1.5f,1.5f),
		myrand_range(0.f,3.f),
		myrand_range(-1.5f,1.5f)
	);
	p.acc = v3f(
		0.0f,
		-player->movement_gravity * player->physics_override.gravity / BS,
		0.0f
	);
	p.pos = v3f(
		(f32)pos.X + myrand_range(0.f, .5f) - .25f,
		(f32)pos.Y + myrand_range(0.f, .5f) - .25f,
		(f32)pos.Z + myrand_range(0.f, .5f) - .25f
	);

	addParticle(std::make_unique<Particle>(
		p,
		ClientParticleTexRef(ref),
		texpos,
		texsize,
		color));
}

void ParticleManager::reserveParticleSpace(size_t max_estimate)
{
	MutexAutoLock lock(m_particle_list_lock);

	m_particles.reserve(m_particles.size() + max_estimate);
}

static void setBlendMode(video::SMaterial &material, BlendMode blendmode)
{
	video::E_BLEND_FACTOR bfsrc, bfdst;
	video::E_BLEND_OPERATION blendop;
	switch (blendmode) {
		case BlendMode::add:
			bfsrc = video::EBF_SRC_ALPHA;
			bfdst = video::EBF_DST_ALPHA;
			blendop = video::EBO_ADD;
		break;

		case BlendMode::sub:
			bfsrc = video::EBF_SRC_ALPHA;
			bfdst = video::EBF_DST_ALPHA;
			blendop = video::EBO_REVSUBTRACT;
		break;

		case BlendMode::screen:
			bfsrc = video::EBF_ONE;
			bfdst = video::EBF_ONE_MINUS_SRC_COLOR;
			blendop = video::EBO_ADD;
		break;

		default: // includes BlendMode::alpha
			bfsrc = video::EBF_SRC_ALPHA;
			bfdst = video::EBF_ONE_MINUS_SRC_ALPHA;
			blendop = video::EBO_ADD;
		break;
	}

	material.MaterialTypeParam = video::pack_textureBlendFunc(
			bfsrc, bfdst,
			video::EMFN_MODULATE_1X,
			video::EAS_TEXTURE | video::EAS_VERTEX_COLOR);
	material.BlendOperation = blendop;
}

video::SMaterial ParticleManager::getMaterialForParticle(const Particle *particle)
{
	const ClientParticleTexRef &texture = particle->getTextureRef();

	video::SMaterial material;

	// Texture
	material.BackfaceCulling = false;
	material.FogEnable = true;
	material.forEachTexture([] (auto &tex) {
		tex.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
		tex.MagFilter = video::ETMAGF_NEAREST;
	});

	const auto blendmode = particle->getBlendMode();
	if (blendmode == BlendMode::clip) {
		material.ZWriteEnable = video::EZW_ON;
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		material.MaterialTypeParam = 0.5f;
	} else {
		// We don't have working transparency sorting. Disable Z-Write for
		// correct results for clipped-alpha at least.
		material.ZWriteEnable = video::EZW_OFF;
		material.MaterialType = video::EMT_ONETEXTURE_BLEND;
		setBlendMode(material, blendmode);
	}
	material.setTexture(0, texture.ref);

	return material;
}

bool ParticleManager::addParticle(std::unique_ptr<Particle> toadd)
{
	MutexAutoLock lock(m_particle_list_lock);

	auto material = getMaterialForParticle(toadd.get());

	ParticleBuffer *found = nullptr;
	// simple shortcut when multiple particles of the same type get added
	if (!m_particles.empty()) {
		auto &last = m_particles.back();
		if (last->getBuffer() && last->getBuffer()->getMaterial(0) == material)
			found = last->getBuffer();
	}
	// search fitting buffer
	if (!found) {
		for (auto &buffer : m_particle_buffers) {
			if (buffer->getMaterial(0) == material) {
				found = buffer.get();
				break;
			}
		}
	}
	// or create a new one
	if (!found) {
		auto tmp = make_irr<ParticleBuffer>(m_env, material);
		found = tmp.get();
		m_particle_buffers.push_back(std::move(tmp));
	}

	if (!toadd->attachToBuffer(found)) {
		infostream << "ParticleManager: buffer full, dropping particle" << std::endl;
		return false;
	}
	m_particles.push_back(std::move(toadd));
	return true;
}

void ParticleManager::addParticleSpawner(u64 id, std::unique_ptr<ParticleSpawner> toadd)
{
	MutexAutoLock lock(m_spawner_list_lock);

	auto &slot = m_particle_spawners[id];
	if (slot) {
		// do not kill spawners here. children are still alive
		errorstream << "ParticleManager: Failed to add spawner with id " << id
				<< ". Id already in use." << std::endl;
		return;
	}
	slot = std::move(toadd);
}

void ParticleManager::deleteParticleSpawner(u64 id)
{
	MutexAutoLock lock(m_spawner_list_lock);

	auto it = m_particle_spawners.find(id);
	if (it != m_particle_spawners.end()) {
		m_dying_particle_spawners.push_back(std::move(it->second));
		m_particle_spawners.erase(it);
	}
}
