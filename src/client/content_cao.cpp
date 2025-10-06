// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "content_cao.h"
#include <IBillboardSceneNode.h>
#include <ICameraSceneNode.h>
#include <IMeshManipulator.h>
#include <IAnimatedMeshSceneNode.h>
#include <ISceneNode.h>
#include "client/client.h"
#include "client/renderingengine.h"
#include "client/sound.h"
#include "client/texturesource.h"
#include "client/mapblock_mesh.h"
#include "client/content_mapblock.h"
#include "client/meshgen/collector.h"
#include "util/basic_macros.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include "camera.h" // CameraModes
#include "collision.h"
#include "content_cso.h"
#include "clientobject.h"
#include "environment.h"
#include "itemdef.h"
#include "localplayer.h"
#include "map.h"
#include "mesh.h"
#include "nodedef.h"
#include "settings.h"
#include "tool.h"
#include "wieldmesh.h"
#include <algorithm>
#include <cmath>
#include "client/shader.h"
#include "client/minimap.h"
#include <quaternion.h>
#include <SMesh.h>
#include <IMeshBuffer.h>
#include <CMeshBuffer.h>

struct ToolCapabilities;

std::unordered_map<u16, ClientActiveObject::Factory> ClientActiveObject::m_types;

template<typename T>
void SmoothTranslator<T>::init(T current)
{
	val_old = current;
	val_current = current;
	val_target = current;
	anim_time = 0;
	anim_time_counter = 0;
	aim_is_end = true;
}

template<typename T>
void SmoothTranslator<T>::update(T new_target, bool is_end_position, float update_interval)
{
	aim_is_end = is_end_position;
	val_old = val_current;
	val_target = new_target;
	if (update_interval > 0) {
		anim_time = update_interval;
	} else {
		if (anim_time < 0.001 || anim_time > 1.0)
			anim_time = anim_time_counter;
		else
			anim_time = anim_time * 0.9 + anim_time_counter * 0.1;
	}
	anim_time_counter = 0;
}

template<typename T>
void SmoothTranslator<T>::translate(f32 dtime)
{
	anim_time_counter = anim_time_counter + dtime;
	T val_diff = val_target - val_old;
	f32 moveratio = 1.0;
	if (anim_time > 0.001)
		moveratio = anim_time_counter / anim_time;
	f32 move_end = aim_is_end ? 1.0 : 1.5;

	// Move a bit less than should, to avoid oscillation
	moveratio = std::min(moveratio * 0.8f, move_end);
	val_current = val_old + val_diff * moveratio;
}

void SmoothTranslatorWrapped::translate(f32 dtime)
{
	anim_time_counter = anim_time_counter + dtime;
	f32 val_diff = std::abs(val_target - val_old);
	if (val_diff > 180.f)
		val_diff = 360.f - val_diff;

	f32 moveratio = 1.0;
	if (anim_time > 0.001)
		moveratio = anim_time_counter / anim_time;
	f32 move_end = aim_is_end ? 1.0 : 1.5;

	// Move a bit less than should, to avoid oscillation
	moveratio = std::min(moveratio * 0.8f, move_end);
	wrappedApproachShortest(val_current, val_target,
		val_diff * moveratio, 360.f);
}

void SmoothTranslatorWrappedv3f::translate(f32 dtime)
{
	anim_time_counter = anim_time_counter + dtime;

	v3f val_diff_v3f;
	val_diff_v3f.X = std::abs(val_target.X - val_old.X);
	val_diff_v3f.Y = std::abs(val_target.Y - val_old.Y);
	val_diff_v3f.Z = std::abs(val_target.Z - val_old.Z);

	if (val_diff_v3f.X > 180.f)
		val_diff_v3f.X = 360.f - val_diff_v3f.X;

	if (val_diff_v3f.Y > 180.f)
		val_diff_v3f.Y = 360.f - val_diff_v3f.Y;

	if (val_diff_v3f.Z > 180.f)
		val_diff_v3f.Z = 360.f - val_diff_v3f.Z;

	f32 moveratio = 1.0;
	if (anim_time > 0.001)
		moveratio = anim_time_counter / anim_time;
	f32 move_end = aim_is_end ? 1.0 : 1.5;

	// Move a bit less than should, to avoid oscillation
	moveratio = std::min(moveratio * 0.8f, move_end);
	wrappedApproachShortest(val_current.X, val_target.X,
		val_diff_v3f.X * moveratio, 360.f);

	wrappedApproachShortest(val_current.Y, val_target.Y,
		val_diff_v3f.Y * moveratio, 360.f);

	wrappedApproachShortest(val_current.Z, val_target.Z,
		val_diff_v3f.Z * moveratio, 360.f);
}

/*
	Other stuff
*/

static bool setMaterialTextureAndFilters(video::SMaterial &material,
	const std::string &texturestring, ITextureSource *tsrc)
{
	bool use_trilinear_filter = g_settings->getBool("trilinear_filter");
	bool use_bilinear_filter = g_settings->getBool("bilinear_filter");
	bool use_anisotropic_filter = g_settings->getBool("anisotropic_filter");

	video::ITexture *texture = tsrc->getTextureForMesh(texturestring);
	if (!texture)
		return false;

	material.setTexture(0, texture);

	// don't filter low-res textures, makes them look blurry
	const core::dimension2d<u32> &size = texture->getOriginalSize();
	if (std::min(size.Width, size.Height) < TEXTURE_FILTER_MIN_SIZE)
		use_trilinear_filter = use_bilinear_filter = false;

	material.forEachTexture([=] (auto &tex) {
		setMaterialFilters(tex, use_bilinear_filter, use_trilinear_filter,
				use_anisotropic_filter);
	});
	return true;
}

static void setBillboardTextureMatrix(scene::IBillboardSceneNode *bill,
		float txs, float tys, int col, int row)
{
	video::SMaterial& material = bill->getMaterial(0);
	core::matrix4& matrix = material.getTextureMatrix(0);
	matrix.setTextureTranslate(txs*col, tys*row);
	matrix.setTextureScale(txs, tys);
}

static bool logOnce(const std::ostringstream &from, std::ostream &log_to)
{
	thread_local std::vector<u64> logged;

	std::string message = from.str();
	u64 hash = murmur_hash_64_ua(message.data(), message.length(), 0xBADBABE);

	if (std::find(logged.begin(), logged.end(), hash) != logged.end())
		return false;
	logged.push_back(hash);
	log_to << message << std::endl;
	return true;
}

static void setColorParam(scene::ISceneNode *node, video::SColor color)
{
	for (u32 i = 0; i < node->getMaterialCount(); ++i)
		node->getMaterial(i).ColorParam = color;
}

static scene::SMesh *generateNodeMesh(Client *client, MapNode n,
	std::vector<MeshAnimationInfo> &animation)
{
	auto *ndef = client->ndef();
	auto *shdsrc = client->getShaderSource();

	MeshCollector collector(v3f(0), v3f());
	{
		MeshMakeData mmd(ndef, 1, MeshGrid{1});
		n.setParam1(0xff);
		mmd.fillSingleNode(n);
		MapblockMeshGenerator(&mmd, &collector).generate();
	}

	auto mesh = make_irr<scene::SMesh>();
	animation.clear();
	for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
		for (PreMeshBuffer &p : collector.prebuffers[layer]) {
			// reset the pre-computed light data stored in the vertex color,
			// since we do that ourselves via updateLight().
			for (auto &v : p.vertices)
				v.Color.set(0xFFFFFFFF);
			// but still apply the tile color
			p.applyTileColor();

			if (p.layer.material_flags & MATERIAL_FLAG_ANIMATION) {
				const FrameSpec &frame = (*p.layer.frames)[0];
				p.layer.texture = frame.texture;

				animation.emplace_back(MeshAnimationInfo{mesh->getMeshBufferCount(), 0, p.layer});
			}

			auto buf = make_irr<scene::SMeshBuffer>();
			buf->append(&p.vertices[0], p.vertices.size(),
					&p.indices[0], p.indices.size());

			// Set up material
			auto &mat = buf->Material;
			u32 shader_id = shdsrc->getShader("object_shader", p.layer.material_type, NDT_NORMAL);
			mat.MaterialType = shdsrc->getShaderInfo(shader_id).material;
			p.layer.applyMaterialOptions(mat, layer);

			mesh->addMeshBuffer(buf.get());
		}
	}
	mesh->recalculateBoundingBox();
	return mesh.release();
}

/*
	GenericCAO
*/

GenericCAO::GenericCAO(Client *client, ClientEnvironment *env):
		ClientActiveObject(0, client, env)
{
	if (!client) {
		ClientActiveObject::registerType(getType(), create);
	} else {
		m_client = client;
	}
}

bool GenericCAO::getCollisionBox(aabb3f *toset) const
{
	if (m_prop.physical)
	{
		//update collision box
		toset->MinEdge = m_prop.collisionbox.MinEdge * BS;
		toset->MaxEdge = m_prop.collisionbox.MaxEdge * BS;

		toset->MinEdge += m_position;
		toset->MaxEdge += m_position;

		return true;
	}

	return false;
}

bool GenericCAO::collideWithObjects() const
{
	return m_prop.collideWithObjects;
}

void GenericCAO::initialize(const std::string &data)
{
	processInitData(data);
}

void GenericCAO::processInitData(const std::string &data)
{
	std::istringstream is(data, std::ios::binary);
	const u8 version = readU8(is);

	if (version < 1) {
		errorstream << "GenericCAO: Unsupported init data version"
				<< std::endl;
		return;
	}

	// PROTOCOL_VERSION >= 37
	m_name = deSerializeString16(is);
	m_is_player = readU8(is);
	m_id = readU16(is);
	m_position = readV3F32(is);
	m_rotation = readV3F32(is);
	m_hp = readU16(is);

	if (m_is_player) {
		// Check if it's the current player
		LocalPlayer *player = m_env->getLocalPlayer();
		if (player && player->getName() == m_name) {
			m_is_local_player = true;
			m_is_visible = false;
			player->setCAO(this);
		}
	}

	const u8 num_messages = readU8(is);
	for (u8 i = 0; i < num_messages; i++) {
		std::string message = deSerializeString32(is);
		processMessage(message);
	}

	m_rotation = wrapDegrees_0_360_v3f(m_rotation);
	pos_translator.init(m_position);
	rot_translator.init(m_rotation);
	updateNodePos();
}

GenericCAO::~GenericCAO()
{
	removeFromScene(true);
}

bool GenericCAO::getSelectionBox(aabb3f *toset) const
{
	if (!m_prop.is_visible || !m_is_visible || m_is_local_player) {
		return false;
	}
	*toset = m_selection_box;
	return true;
}

void GenericCAO::updateParentChain() const
{
	if (!m_matrixnode)
		return;
	// Update the entire chain of nodes to ensure absolute position is correct
	std::vector<scene::ISceneNode *> chain;
	for (scene::ISceneNode *node = m_matrixnode; node; node = node->getParent())
		chain.push_back(node);
	for (auto it = chain.rbegin(); it != chain.rend(); ++it)
		(*it)->updateAbsolutePosition();
}

const v3f GenericCAO::getPosition() const
{
	if (!getParent())
		return pos_translator.val_current;

	// Calculate real position in world based on MatrixNode
	if (m_matrixnode) {
		// FIXME work around #16221 which is caused by the camera position and thus
		// offset not being in sync with the player (parent) CAO position.
		// A better solution might restrict this update to the local player only
		// or keep player and camera position in sync.
		GenericCAO::updateParentChain();
		v3s16 camera_offset = m_env->getCameraOffset();
		return m_matrixnode->getAbsolutePosition() +
				intToFloat(camera_offset, BS);
	}

	return m_position;
}

bool GenericCAO::isImmortal() const
{
	return itemgroup_get(getGroups(), "immortal");
}

scene::ISceneNode *GenericCAO::getSceneNode() const
{
	if (m_meshnode) {
		return m_meshnode;
	}

	if (m_animated_meshnode) {
		return m_animated_meshnode;
	}

	if (m_wield_meshnode) {
		return m_wield_meshnode;
	}

	if (m_spritenode) {
		return m_spritenode;
	}
	return NULL;
}

scene::IAnimatedMeshSceneNode *GenericCAO::getAnimatedMeshSceneNode() const
{
	return m_animated_meshnode;
}

void GenericCAO::setChildrenVisible(bool toset)
{
	for (object_t cao_id : m_attachment_child_ids) {
		GenericCAO *obj = m_env->getGenericCAO(cao_id);
		if (obj) {
			// Check if the entity is forced to appear in first person.
			obj->setVisible(obj->m_force_visible ? true : toset);
		}
	}
}

void GenericCAO::setAttachment(object_t parent_id, const std::string &bone,
		v3f position, v3f rotation, bool force_visible)
{
	// Do checks to avoid circular references
	// See similar check in `UnitSAO::setAttachment` (but with different types).
	{
		auto *obj = m_env->getActiveObject(parent_id);
		if (obj == this) {
			assert(false);
			return;
		}
		bool problem = false;
		if (obj) {
			// The chain of wanted parent must not refer or contain "this"
			for (obj = obj->getParent(); obj; obj = obj->getParent()) {
				if (obj == this) {
					problem = true;
					break;
				}
			}
		}
		if (problem) {
			warningstream << "Network or mod bug: "
				<< "Attempted to attach object " << m_id << " to parent "
				<< parent_id << " but former is an (in)direct parent of latter." << std::endl;
			return;
		}
	}

	const auto old_parent = m_attachment_parent_id;
	m_attachment_parent_id = parent_id;
	m_attachment_bone = bone;
	m_attachment_position = position;
	m_attachment_rotation = rotation;
	m_force_visible = force_visible;

	ClientActiveObject *parent = m_env->getActiveObject(parent_id);

	if (parent_id != old_parent) {
		if (auto *o = m_env->getActiveObject(old_parent))
			o->removeAttachmentChild(m_id);
		if (parent)
			parent->addAttachmentChild(m_id);
	}
	updateAttachments();

	// Forcibly show attachments if required by set_attach
	if (m_force_visible) {
		m_is_visible = true;
	} else if (!m_is_local_player) {
		// Objects attached to the local player should be hidden in first person
		m_is_visible = !m_attached_to_local ||
			m_client->getCamera()->getCameraMode() != CAMERA_MODE_FIRST;
		m_force_visible = false;
	} else {
		// Local players need to have this set,
		// otherwise first person attachments fail.
		m_is_visible = true;
	}
}

void GenericCAO::getAttachment(object_t *parent_id, std::string *bone, v3f *position,
	v3f *rotation, bool *force_visible) const
{
	*parent_id = m_attachment_parent_id;
	*bone = m_attachment_bone;
	*position = m_attachment_position;
	*rotation = m_attachment_rotation;
	*force_visible = m_force_visible;
}

void GenericCAO::clearChildAttachments()
{
	// Cannot use for-loop here: setAttachment() modifies 'm_attachment_child_ids'!
	while (!m_attachment_child_ids.empty()) {
		const auto child_id = *m_attachment_child_ids.begin();

		if (auto *child = m_env->getActiveObject(child_id))
			child->clearParentAttachment();
		else
			removeAttachmentChild(child_id);
	}
}

void GenericCAO::addAttachmentChild(object_t child_id)
{
	m_attachment_child_ids.insert(child_id);
}

void GenericCAO::removeAttachmentChild(object_t child_id)
{
	m_attachment_child_ids.erase(child_id);
}

ClientActiveObject* GenericCAO::getParent() const
{
	return m_attachment_parent_id ? m_env->getActiveObject(m_attachment_parent_id) :
			nullptr;
}

void GenericCAO::removeFromScene(bool permanent)
{
	// Should be true when removing the object permanently
	// and false when refreshing (eg: updating visuals)
	if (m_env && permanent) {
		// The client does not know whether this object does re-appear to
		// a later time, thus do not clear child attachments.

		clearParentAttachment();
	}

	if (auto shadow = RenderingEngine::get_shadow_renderer())
		if (auto node = getSceneNode())
			shadow->removeNodeFromShadowList(node);

	if (m_meshnode) {
		m_meshnode->remove();
		m_meshnode->drop();
		m_meshnode = nullptr;
	} else if (m_animated_meshnode)	{
		m_animated_meshnode->remove();
		m_animated_meshnode->drop();
		m_animated_meshnode = nullptr;
	} else if (m_wield_meshnode) {
		m_wield_meshnode->remove();
		m_wield_meshnode->drop();
		m_wield_meshnode = nullptr;
	} else if (m_spritenode) {
		m_spritenode->remove();
		m_spritenode->drop();
		m_spritenode = nullptr;
	}

	m_meshnode_animation.clear();

	if (m_matrixnode) {
		m_matrixnode->remove();
		m_matrixnode->drop();
		m_matrixnode = nullptr;
	}

	if (m_nametag) {
		m_client->getCamera()->removeNametag(m_nametag);
		m_nametag = nullptr;
	}

	if (m_marker && m_client->getMinimap())
		m_client->getMinimap()->removeMarker(&m_marker);
}

void GenericCAO::addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr)
{
	m_smgr = smgr;

	if (getSceneNode() != NULL) {
		return;
	}

	m_visuals_expired = false;

	if (!m_prop.is_visible)
		return;

	infostream << "GenericCAO::addToScene(): " <<
		enum_to_string(es_ObjectVisual, m_prop.visual)<< std::endl;

	if (m_prop.visual != OBJECTVISUAL_NODE &&
			m_prop.visual != OBJECTVISUAL_WIELDITEM &&
			m_prop.visual != OBJECTVISUAL_ITEM)
	{
		IShaderSource *shader_source = m_client->getShaderSource();
		MaterialType material_type;

		if (m_prop.shaded && m_prop.glow == 0)
			material_type = (m_prop.use_texture_alpha) ?
				TILE_MATERIAL_ALPHA : TILE_MATERIAL_BASIC;
		else
			material_type = (m_prop.use_texture_alpha) ?
				TILE_MATERIAL_PLAIN_ALPHA : TILE_MATERIAL_PLAIN;

		u32 shader_id = shader_source->getShader("object_shader", material_type, NDT_NORMAL);
		m_material_type = shader_source->getShaderInfo(shader_id).material;
	} else {
		// Not used, so make sure it's not valid
		m_material_type = EMT_INVALID;
	}

	m_matrixnode = m_smgr->addDummyTransformationSceneNode();
	m_matrixnode->grab();

	auto setMaterial = [this] (video::SMaterial &mat) {
		if (m_material_type != EMT_INVALID)
			mat.MaterialType = m_material_type;
		mat.FogEnable = true;
		mat.forEachTexture([] (auto &tex) {
			tex.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
			tex.MagFilter = video::ETMAGF_NEAREST;
		});
	};

	auto setSceneNodeMaterials = [setMaterial] (scene::ISceneNode *node) {
		node->forEachMaterial(setMaterial);
	};

	switch(m_prop.visual) {
	case OBJECTVISUAL_UPRIGHT_SPRITE: {
		auto mesh = make_irr<scene::SMesh>();
		f32 dx = BS * m_prop.visual_size.X / 2;
		f32 dy = BS * m_prop.visual_size.Y / 2;
		video::SColor c(0xFFFFFFFF);

		video::S3DVertex vertices[4] = {
			video::S3DVertex(-dx, -dy, 0, 0,0,1, c, 1,1),
			video::S3DVertex( dx, -dy, 0, 0,0,1, c, 0,1),
			video::S3DVertex( dx,  dy, 0, 0,0,1, c, 0,0),
			video::S3DVertex(-dx,  dy, 0, 0,0,1, c, 1,0),
		};
		if (m_is_player) {
			// Move minimal Y position to 0 (feet position)
			for (auto &vertex : vertices)
				vertex.Pos.Y += dy;
		}
		const u16 indices[] = {0,1,2,2,3,0};

		for (int face : {0, 1}) {
			auto buf = make_irr<scene::SMeshBuffer>();
			// Front (0) or Back (1)
			if (face == 1) {
				for (auto &v : vertices)
					v.Normal *= -1;
				for (int i : {0, 2})
					std::swap(vertices[i].Pos, vertices[i+1].Pos);
			}
			buf->append(vertices, 4, indices, 6);

			// Set material
			setMaterial(buf->getMaterial());
			buf->getMaterial().ColorParam = c;

			// Add to mesh
			mesh->addMeshBuffer(buf.get());
		}

		mesh->recalculateBoundingBox();
		m_meshnode = m_smgr->addMeshSceneNode(mesh.get(), m_matrixnode);
		m_meshnode->grab();
		break;
	} case OBJECTVISUAL_CUBE: {
		scene::IMesh *mesh = createCubeMesh(v3f(BS,BS,BS));
		m_meshnode = m_smgr->addMeshSceneNode(mesh, m_matrixnode);
		m_meshnode->grab();
		mesh->drop();

		m_meshnode->setScale(m_prop.visual_size);

		setSceneNodeMaterials(m_meshnode);

		m_meshnode->forEachMaterial([this] (auto &mat) {
			mat.BackfaceCulling = m_prop.backface_culling;
		});
		break;
	} case OBJECTVISUAL_MESH: {
		scene::IAnimatedMesh *mesh = m_client->getMesh(m_prop.mesh, true);
		if (mesh) {
			if (!checkMeshNormals(mesh)) {
				infostream << "GenericCAO: recalculating normals for mesh "
					<< m_prop.mesh << std::endl;
				m_smgr->getMeshManipulator()->
						recalculateNormals(mesh, true, false);
			}

			m_animated_meshnode = m_smgr->addAnimatedMeshSceneNode(mesh, m_matrixnode);
			m_animated_meshnode->grab();
			mesh->drop(); // The scene node took hold of it
			m_animated_meshnode->setScale(m_prop.visual_size);

			// set vertex colors to ensure alpha is set
			setMeshColor(m_animated_meshnode->getMesh(), video::SColor(0xFFFFFFFF));

			setSceneNodeMaterials(m_animated_meshnode);

			m_animated_meshnode->forEachMaterial([this] (auto &mat) {
				mat.BackfaceCulling = m_prop.backface_culling;
			});

			m_animated_meshnode->setOnAnimateCallback([&](f32 dtime) {
				for (auto &it : m_bone_override) {
					auto* bone = m_animated_meshnode->getJointNode(it.first.c_str());
					if (!bone)
						continue;

					BoneOverride &props = it.second;
					props.dtime_passed += dtime;

					bone->setPosition(props.getPosition(bone->getPosition()));
					bone->setRotation(props.getRotationEulerDeg(bone->getRotation()));
					bone->setScale(props.getScale(bone->getScale()));
				}
			});
		} else
			errorstream<<"GenericCAO::addToScene(): Could not load mesh "<<m_prop.mesh<<std::endl;
		break;
	}
	case OBJECTVISUAL_WIELDITEM:
	case OBJECTVISUAL_ITEM: {
		ItemStack item;
		if (m_prop.wield_item.empty()) {
			// Old format, only textures are specified.
			infostream << "textures: " << m_prop.textures.size() << std::endl;
			if (!m_prop.textures.empty()) {
				infostream << "textures[0]: " << m_prop.textures[0]
					<< std::endl;
				IItemDefManager *idef = m_client->idef();
				item = ItemStack(m_prop.textures[0], 1, 0, idef);
			}
		} else {
			infostream << "serialized form: " << m_prop.wield_item << std::endl;
			item.deSerialize(m_prop.wield_item, m_client->idef());
		}
		m_wield_meshnode = new WieldMeshSceneNode(m_smgr, -1);
		m_wield_meshnode->setItem(item, m_client,
			(m_prop.visual == OBJECTVISUAL_WIELDITEM));

		m_wield_meshnode->setScale(m_prop.visual_size / 2.0f);
		break;
	} case OBJECTVISUAL_NODE: {
		auto *mesh = generateNodeMesh(m_client, m_prop.node, m_meshnode_animation);
		assert(mesh);

		m_meshnode = m_smgr->addMeshSceneNode(mesh, m_matrixnode);
		m_meshnode->setSharedMaterials(true);
		m_meshnode->grab();
		mesh->drop();

		m_meshnode->setScale(m_prop.visual_size);

		setSceneNodeMaterials(m_meshnode);
		break;
	} default:
		m_spritenode = m_smgr->addBillboardSceneNode(m_matrixnode);
		m_spritenode->grab();

		setSceneNodeMaterials(m_spritenode);

		m_spritenode->setSize(v2f(m_prop.visual_size.X,
				m_prop.visual_size.Y) * BS);
		setBillboardTextureMatrix(m_spritenode, 1, 1, 0, 0);

		// This also serves as fallback for unknown visual types
		if (m_prop.visual != OBJECTVISUAL_SPRITE) {
			m_spritenode->getMaterial(0).setTexture(0,
				tsrc->getTextureForMesh("unknown_object.png"));
		}
		break;
	}

	/* Set VBO hint */
	// wieldmesh sets its own hint, no need to handle it
	if (m_meshnode || m_animated_meshnode) {
		// sprite uses vertex animation
		if (m_meshnode && m_prop.visual != OBJECTVISUAL_UPRIGHT_SPRITE)
			m_meshnode->getMesh()->setHardwareMappingHint(scene::EHM_STATIC);

		if (m_animated_meshnode) {
			auto *mesh = m_animated_meshnode->getMesh();
			// skinning happens on the CPU
			if (m_animated_meshnode->getJointCount() > 0)
				mesh->setHardwareMappingHint(scene::EHM_STREAM, scene::EBT_VERTEX);
			else
				mesh->setHardwareMappingHint(scene::EHM_STATIC, scene::EBT_VERTEX);
			mesh->setHardwareMappingHint(scene::EHM_STATIC, scene::EBT_INDEX);
		}
	}

	/* don't update while punch texture modifier is active */
	if (m_reset_textures_timer < 0)
		updateTextures(m_current_texture_modifier);

	if (scene::ISceneNode *node = getSceneNode()) {
		node->setParent(m_matrixnode);

		if (auto shadow = RenderingEngine::get_shadow_renderer())
			shadow->addNodeToShadowList(node);
	}

	updateNametag();
	updateMarker();
	updateNodePos();
	updateAnimation();
	updateAttachments();
	setNodeLight(m_last_light);
	updateMeshCulling();

	if (m_animated_meshnode) {
		u32 mat_count = m_animated_meshnode->getMaterialCount();
		assert(mat_count == m_animated_meshnode->getMesh()->getMeshBufferCount());
		u32 max_tex_idx = 0;
		for (u32 i = 0; i < mat_count; ++i) {
			max_tex_idx = std::max(max_tex_idx,
					m_animated_meshnode->getMesh()->getTextureSlot(i));
		}
		if (mat_count == 0 || m_prop.textures.empty()) {
			// nothing
		} else if (max_tex_idx >= m_prop.textures.size()) {
			std::ostringstream oss;
			oss << "GenericCAO::addToScene(): Model "
				<< m_prop.mesh << " is missing " << (max_tex_idx + 1 - m_prop.textures.size())
				<< " more texture(s), this is deprecated.";
			logOnce(oss, warningstream);

			video::ITexture *last = m_animated_meshnode->getMaterial(0).TextureLayers[0].Texture;
			for (u32 i = 1; i < mat_count; i++) {
				auto &layer = m_animated_meshnode->getMaterial(i).TextureLayers[0];
				if (!layer.Texture)
					layer.Texture = last;
				last = layer.Texture;
			}
		}
	}
}

void GenericCAO::updateLight(u32 day_night_ratio)
{
	if (m_prop.glow < 0)
		return;

	u16 light_at_pos = 0;
	u8 light_at_pos_intensity = 0;
	bool pos_ok = false;

	v3s16 pos[3];
	u16 npos = getLightPosition(pos);
	for (u16 i = 0; i < npos; i++) {
		bool this_ok;
		MapNode n = m_env->getMap().getNode(pos[i], &this_ok);
		if (this_ok) {
			// Get light level at the position plus the entity glow
			u16 this_light = getInteriorLight(n, m_prop.glow, m_client->ndef());
			u8 this_light_intensity = MYMAX(this_light & 0xFF, this_light >> 8);
			if (this_light_intensity > light_at_pos_intensity) {
				light_at_pos = this_light;
				light_at_pos_intensity = this_light_intensity;
			}
			pos_ok = true;
		}
	}
	if (!pos_ok)
		light_at_pos = LIGHT_SUN;

	video::SColor light;

	// Encode light into color, adding a small boost
	// based on the entity glow.
	light = encode_light(light_at_pos, m_prop.glow);

	if (light != m_last_light) {
		m_last_light = light;
		setNodeLight(light);
	}
}

void GenericCAO::setNodeLight(const video::SColor &light_color)
{
	if (m_prop.visual == OBJECTVISUAL_WIELDITEM || m_prop.visual == OBJECTVISUAL_ITEM) {
		if (m_wield_meshnode)
			m_wield_meshnode->setLightColorAndAnimation(light_color,
					m_client->getAnimationTime());
		return;
	}

	{
		auto *node = getSceneNode();
		if (!node)
			return;
		setColorParam(node, light_color);
	}
}

u16 GenericCAO::getLightPosition(v3s16 *pos)
{
	const auto &box = m_prop.collisionbox;
	pos[0] = floatToInt(m_position + box.MinEdge * BS, BS);
	pos[1] = floatToInt(m_position + box.MaxEdge * BS, BS);

	// Skip center pos if it falls into the same node as Min or MaxEdge
	if ((box.MaxEdge - box.MinEdge).getLengthSQ() < 3.0f)
		return 2;
	pos[2] = floatToInt(m_position + box.getCenter() * BS, BS);
	return 3;
}

void GenericCAO::updateMarker()
{
	if (!m_client->getMinimap())
		return;

	if (!m_prop.show_on_minimap) {
		if (m_marker)
			m_client->getMinimap()->removeMarker(&m_marker);
		return;
	}

	if (m_marker)
		return;

	scene::ISceneNode *node = getSceneNode();
	if (!node)
		return;
	m_marker = m_client->getMinimap()->addMarker(node);
}

void GenericCAO::updateNametag()
{
	if (m_is_local_player) // No nametag for local player
		return;

	if (m_prop.nametag.empty() || m_prop.nametag_color.getAlpha() == 0) {
		// Delete nametag
		if (m_nametag) {
			m_client->getCamera()->removeNametag(m_nametag);
			m_nametag = nullptr;
		}
		return;
	}

	scene::ISceneNode *node = getSceneNode();
	if (!node)
		return;

	v3f pos;
	pos.Y = m_prop.selectionbox.MaxEdge.Y + 0.3f;
	// Add or update nametag
	Nametag tmp{node, m_prop.nametag, m_prop.nametag_color,
			m_prop.nametag_bgcolor, m_prop.nametag_fontsize, pos,
			m_prop.nametag_scale_z};
	if (!m_nametag) {
		m_nametag = m_client->getCamera()->addNametag(tmp);
		assert(m_nametag);
	} else {
		*m_nametag = tmp;
	}
}

void GenericCAO::updateNodePos()
{
	if (getParent() != NULL)
		return;

	scene::ISceneNode *node = getSceneNode();

	if (node) {
		assert(m_matrixnode);
		v3s16 camera_offset = m_env->getCameraOffset();
		v3f pos = pos_translator.val_current -
				intToFloat(camera_offset, BS);
		getPosRotMatrix().setTranslation(pos);
		if (node != m_spritenode) { // rotate if not a sprite
			v3f rot = m_is_local_player ? -m_rotation : -rot_translator.val_current;
			setPitchYawRoll(getPosRotMatrix(), rot);
		}
	}
}

void GenericCAO::step(float dtime, ClientEnvironment *env)
{
	// Handle model animations and update positions instantly to prevent lags
	if (m_is_local_player) {
		LocalPlayer *player = m_env->getLocalPlayer();
		m_position = player->getPosition();
		pos_translator.val_current = m_position;
		m_rotation.Y = wrapDegrees_0_360(player->getYaw());
		rot_translator.val_current = m_rotation;

		if (m_is_visible) {
			LocalPlayerAnimation old_anim = player->last_animation;
			float old_anim_speed = player->last_animation_speed;
			m_velocity = v3f(0,0,0);
			m_acceleration = v3f(0,0,0);
			const PlayerControl &controls = player->getPlayerControl();
			f32 new_speed = player->local_animation_speed;

			bool walking = false;
			if (controls.movement_speed > 0.001f) {
				new_speed *= controls.movement_speed;
				walking = true;
			}

			v2f new_anim(0,0);
			bool allow_update = false;

			// increase speed if using fast or flying fast
			if((g_settings->getBool("fast_move") &&
					m_client->checkLocalPrivilege("fast")) &&
					(controls.aux1 ||
					(!player->touching_ground &&
					g_settings->getBool("free_move") &&
					m_client->checkLocalPrivilege("fly"))))
					new_speed *= 1.5;
			// slowdown speed if sneaking
			if (controls.sneak && walking)
				new_speed /= 2;

			if (walking && (controls.dig || controls.place)) {
				new_anim = player->local_animations[3];
				player->last_animation = LocalPlayerAnimation::WD_ANIM;
			} else if (walking) {
				new_anim = player->local_animations[1];
				player->last_animation = LocalPlayerAnimation::WALK_ANIM;
			} else if (controls.dig || controls.place) {
				new_anim = player->local_animations[2];
				player->last_animation = LocalPlayerAnimation::DIG_ANIM;
			}

			// Apply animations if input detected and not attached
			// or set idle animation
			if ((new_anim.X + new_anim.Y) > 0 && !getParent()) {
				allow_update = true;
				m_animation_range = new_anim;
				m_animation_speed = new_speed;
				player->last_animation_speed = m_animation_speed;
			} else {
				player->last_animation = LocalPlayerAnimation::NO_ANIM;

				if (old_anim != LocalPlayerAnimation::NO_ANIM) {
					m_animation_range = player->local_animations[0];
					updateAnimation();
				}
			}

			// Update local player animations
			if ((player->last_animation != old_anim ||
					m_animation_speed != old_anim_speed) &&
					player->last_animation != LocalPlayerAnimation::NO_ANIM &&
					allow_update)
				updateAnimation();

		}
	}

	if (m_visuals_expired && m_smgr) {
		m_visuals_expired = false;

		// Attachments, part 1: All attached objects must be unparented first,
		// or Irrlicht causes a segmentation fault
		for (u16 cao_id : m_attachment_child_ids) {
			ClientActiveObject *obj = m_env->getActiveObject(cao_id);
			if (obj) {
				scene::ISceneNode *child_node = obj->getSceneNode();
				// The node's parent is always an IDummyTraformationSceneNode,
				// so we need to reparent that one instead.
				if (child_node)
					child_node->getParent()->setParent(m_smgr->getRootSceneNode());
			}
		}

		removeFromScene(false);
		addToScene(m_client->tsrc(), m_smgr);

		// Attachments, part 2: Now that the parent has been refreshed, put its attachments back
		for (u16 cao_id : m_attachment_child_ids) {
			ClientActiveObject *obj = m_env->getActiveObject(cao_id);
			if (obj)
				obj->updateAttachments();
		}
	}

	// Make sure m_is_visible is always applied
	scene::ISceneNode *node = getSceneNode();
	if (node)
		node->setVisible(m_is_visible);

	if(getParent() != NULL) // Attachments should be glued to their parent by Irrlicht
	{
		// Set these for later
		m_position = getPosition();
		m_velocity = v3f(0,0,0);
		m_acceleration = v3f(0,0,0);
		pos_translator.val_current = m_position;
		pos_translator.val_target = m_position;
	} else {
		rot_translator.translate(dtime);
		v3f lastpos = pos_translator.val_current;

		if(m_prop.physical)
		{
			aabb3f box = m_prop.collisionbox;
			box.MinEdge *= BS;
			box.MaxEdge *= BS;
			collisionMoveResult moveresult;
			v3f p_pos = m_position;
			v3f p_velocity = m_velocity;
			moveresult = collisionMoveSimple(env,env->getGameDef(),
					box, m_prop.stepheight, dtime,
					&p_pos, &p_velocity, m_acceleration,
					this, m_prop.collideWithObjects);
			// Apply results
			m_position = p_pos;
			m_velocity = p_velocity;

			bool is_end_position = moveresult.collides;
			pos_translator.update(m_position, is_end_position, dtime);
		} else {
			m_position += dtime * m_velocity + 0.5 * dtime * dtime * m_acceleration;
			m_velocity += dtime * m_acceleration;
			pos_translator.update(m_position, pos_translator.aim_is_end,
					pos_translator.anim_time);
		}
		pos_translator.translate(dtime);
		updateNodePos();

		float moved = lastpos.getDistanceFrom(pos_translator.val_current);
		m_step_distance_counter += moved;
		if (m_step_distance_counter > 1.5f * BS) {
			m_step_distance_counter = 0.0f;
			if (!m_is_local_player && m_prop.makes_footstep_sound) {
				const NodeDefManager *ndef = m_client->ndef();
				v3f foot_pos = getPosition() * (1.0f/BS)
						+ v3f(0.0f, m_prop.collisionbox.MinEdge.Y, 0.0f);
				v3s16 node_below_pos = floatToInt(foot_pos + v3f(0.0f, -0.5f, 0.0f),
						1.0f);
				MapNode n = m_env->getMap().getNode(node_below_pos);
				SoundSpec spec = ndef->get(n).sound_footstep;
				// Reduce footstep gain, as non-local-player footsteps are
				// somehow louder.
				spec.gain *= 0.6f;
				// The footstep-sound doesn't travel with the object. => vel=0
				m_client->sound()->playSoundAt(0, spec, foot_pos, v3f(0.0f));
			}
		}
	}

	m_anim_timer += dtime;
	if(m_anim_timer >= m_anim_framelength)
	{
		m_anim_timer -= m_anim_framelength;
		m_anim_frame++;
		if(m_anim_frame >= m_anim_num_frames)
			m_anim_frame = 0;
	}

	updateTextureAnim();

	if(m_reset_textures_timer >= 0)
	{
		m_reset_textures_timer -= dtime;
		if(m_reset_textures_timer <= 0) {
			m_reset_textures_timer = -1;
			updateTextures(m_previous_texture_modifier);
		}
	}

	if (node && std::abs(m_prop.automatic_rotate) > 0.001f) {
		// This is the child node's rotation. It is only used for automatic_rotate.
		v3f local_rot = node->getRotation();
		local_rot.Y = modulo360f(local_rot.Y - dtime * core::RADTODEG *
				m_prop.automatic_rotate);
		node->setRotation(local_rot);
	}

	if (!getParent() && m_prop.automatic_face_movement_dir &&
			(fabs(m_velocity.Z) > 0.001f || fabs(m_velocity.X) > 0.001f)) {
		float target_yaw = atan2(m_velocity.Z, m_velocity.X) * 180 / M_PI
				+ m_prop.automatic_face_movement_dir_offset;
		float max_rotation_per_sec =
				m_prop.automatic_face_movement_max_rotation_per_sec;

		if (max_rotation_per_sec > 0) {
			wrappedApproachShortest(m_rotation.Y, target_yaw,
				dtime * max_rotation_per_sec, 360.f);
		} else {
			// Negative values of max_rotation_per_sec mean disabled.
			m_rotation.Y = target_yaw;
		}

		rot_translator.val_current = m_rotation;
		updateNodePos();
	}
}

static void setMeshBufferTextureCoords(scene::IMeshBuffer *buf, const v2f *uv, u32 count)
{
	assert(buf->getVertexType() == video::EVT_STANDARD);
	assert(buf->getVertexCount() == count);
	auto *vertices = static_cast<video::S3DVertex *>(buf->getVertices());
	for (u32 i = 0; i < count; i++)
		vertices[i].TCoords = uv[i];
	buf->setDirty(scene::EBT_VERTEX);
}

void GenericCAO::updateTextureAnim()
{
	if(m_spritenode)
	{
		scene::ICameraSceneNode* camera =
				m_spritenode->getSceneManager()->getActiveCamera();
		if(!camera)
			return;
		v3f cam_to_entity = m_spritenode->getAbsolutePosition()
				- camera->getAbsolutePosition();
		cam_to_entity.normalize();

		int row = m_tx_basepos.Y;
		int col = m_tx_basepos.X;

		// Yawpitch goes rightwards
		if (m_tx_select_horiz_by_yawpitch) {
			if (cam_to_entity.Y > 0.75)
				col += 5;
			else if (cam_to_entity.Y < -0.75)
				col += 4;
			else {
				float mob_dir =
						atan2(cam_to_entity.Z, cam_to_entity.X) / M_PI * 180.;
				float dir = mob_dir - m_rotation.Y;
				dir = wrapDegrees_180(dir);
				if (std::fabs(wrapDegrees_180(dir - 0)) <= 45.1f)
					col += 2;
				else if(std::fabs(wrapDegrees_180(dir - 90)) <= 45.1f)
					col += 3;
				else if(std::fabs(wrapDegrees_180(dir - 180)) <= 45.1f)
					col += 0;
				else if(std::fabs(wrapDegrees_180(dir + 90)) <= 45.1f)
					col += 1;
				else
					col += 4;
			}
		}

		// Animation goes downwards
		row += m_anim_frame;

		float txs = m_tx_size.X;
		float tys = m_tx_size.Y;
		setBillboardTextureMatrix(m_spritenode, txs, tys, col, row);
	}

	else if (m_meshnode) {
		if (m_prop.visual == OBJECTVISUAL_UPRIGHT_SPRITE) {
			int row = m_tx_basepos.Y;
			int col = m_tx_basepos.X;

			// Animation goes downwards
			row += m_anim_frame;

			const auto &tx = m_tx_size;
			v2f t[4] = { // cf. vertices in GenericCAO::addToScene()
				tx * v2f(col+1, row+1),
				tx * v2f(col, row+1),
				tx * v2f(col, row),
				tx * v2f(col+1, row),
			};
			auto mesh = m_meshnode->getMesh();
			setMeshBufferTextureCoords(mesh->getMeshBuffer(0), t, 4);
			setMeshBufferTextureCoords(mesh->getMeshBuffer(1), t, 4);
		} else if (m_prop.visual == OBJECTVISUAL_NODE) {
			// same calculation as MapBlockMesh::animate() with a global timer
			const float time = m_client->getAnimationTime();
			for (auto &it : m_meshnode_animation) {
				const TileLayer &tile = it.tile;
				int frameno = (int)(time * 1000 / tile.animation_frame_length_ms)
					% tile.animation_frame_count;

				if (frameno == it.frame)
					continue;
				it.frame = frameno;

				auto *buf = m_meshnode->getMesh()->getMeshBuffer(it.i);

				const FrameSpec &frame = (*tile.frames)[frameno];
				buf->getMaterial().setTexture(0, frame.texture);
			}
		}
	}
}

// Do not pass by reference, see header.
void GenericCAO::updateTextures(std::string mod)
{
	ITextureSource *tsrc = m_client->tsrc();

	m_previous_texture_modifier = m_current_texture_modifier;
	m_current_texture_modifier = mod;

	if (m_spritenode) {
		if (m_prop.visual == OBJECTVISUAL_SPRITE) {
			std::string texturestring = "no_texture.png";
			if (!m_prop.textures.empty())
				texturestring = m_prop.textures[0];
			texturestring += mod;

			video::SMaterial &material = m_spritenode->getMaterial(0);
			material.MaterialType = m_material_type;
			setMaterialTextureAndFilters(material, texturestring, tsrc);
		}
	}

	else if (m_animated_meshnode) {
		if (m_prop.visual == OBJECTVISUAL_MESH) {
			for (u32 i = 0; i < m_animated_meshnode->getMaterialCount(); ++i) {
				const auto texture_idx = m_animated_meshnode->getMesh()->getTextureSlot(i);
				if (texture_idx >= m_prop.textures.size())
					continue;
				std::string texturestring = m_prop.textures[texture_idx];
				if (texturestring.empty())
					continue; // Empty texture string means don't modify that material
				texturestring += mod;

				// Set material flags and texture
				video::SMaterial &material = m_animated_meshnode->getMaterial(i);
				material.MaterialType = m_material_type;
				material.BackfaceCulling = m_prop.backface_culling;
				setMaterialTextureAndFilters(material, texturestring, tsrc);
			}
		}
	}

	else if (m_meshnode) {
		if(m_prop.visual == OBJECTVISUAL_CUBE)
		{
			for (u32 i = 0; i < 6; ++i)
			{
				std::string texturestring = "no_texture.png";
				if(m_prop.textures.size() > i)
					texturestring = m_prop.textures[i];
				texturestring += mod;

				// Set material flags and texture
				video::SMaterial &material = m_meshnode->getMaterial(i);
				material.MaterialType = m_material_type;
				setMaterialTextureAndFilters(material, texturestring, tsrc);
			}
		} else if (m_prop.visual == OBJECTVISUAL_UPRIGHT_SPRITE) {
			scene::IMesh *mesh = m_meshnode->getMesh();
			{
				std::string tname = "no_texture.png";
				if (!m_prop.textures.empty())
					tname = m_prop.textures[0];
				tname += mod;

				auto &material = m_meshnode->getMaterial(0);
				setMaterialTextureAndFilters(material, tname, tsrc);
			}
			{
				std::string tname = "no_texture.png";
				if (m_prop.textures.size() >= 2)
					tname = m_prop.textures[1];
				else if (!m_prop.textures.empty())
					tname = m_prop.textures[0];
				tname += mod;

				auto &material = m_meshnode->getMaterial(1);
				setMaterialTextureAndFilters(material, tname, tsrc);
			}
			// Set mesh color (only if lighting is disabled)
			if (m_prop.glow < 0)
				setMeshColor(mesh, {255, 255, 255, 255});
		}
	}
	// Prevent showing the player after changing texture
	if (m_is_local_player)
		updateMeshCulling();
}

void GenericCAO::updateAnimation()
{
	if (!m_animated_meshnode)
		return;

	// Note: This sets the current frame as well, (re)starting the animation.
	m_animated_meshnode->setFrameLoop(m_animation_range.X, m_animation_range.Y);
	if (m_animated_meshnode->getAnimationSpeed() != m_animation_speed)
		m_animated_meshnode->setAnimationSpeed(m_animation_speed);
	m_animated_meshnode->setTransitionTime(m_animation_blend);
	if (m_animated_meshnode->getLoopMode() != m_animation_loop)
		m_animated_meshnode->setLoopMode(m_animation_loop);
}

void GenericCAO::updateAnimationSpeed()
{
	if (!m_animated_meshnode)
		return;

	m_animated_meshnode->setAnimationSpeed(m_animation_speed);
}

void GenericCAO::updateAttachments()
{
	ClientActiveObject *parent = getParent();

	m_attached_to_local = parent && parent->isLocalPlayer();

	/*
	Following cases exist:
		m_attachment_parent_id == 0 && !parent
			This object is not attached
		m_attachment_parent_id != 0 && parent
			This object is attached
		m_attachment_parent_id != 0 && !parent
			This object will be attached as soon the parent is known
		m_attachment_parent_id == 0 && parent
			Impossible case
	*/

	if (!parent) { // Detach or don't attach
		if (m_matrixnode) {
			v3s16 camera_offset = m_env->getCameraOffset();
			v3f old_pos = getPosition();

			m_matrixnode->setParent(m_smgr->getRootSceneNode());
			getPosRotMatrix().setTranslation(old_pos - intToFloat(camera_offset, BS));
			m_matrixnode->updateAbsolutePosition();
		}
	}
	else // Attach
	{
		parent->updateAttachments();
		scene::ISceneNode *parent_node = parent->getSceneNode();
		scene::IAnimatedMeshSceneNode *parent_animated_mesh_node =
				parent->getAnimatedMeshSceneNode();
		if (parent_animated_mesh_node && !m_attachment_bone.empty()) {
			parent_node = parent_animated_mesh_node->getJointNode(m_attachment_bone.c_str());
		}

		if (m_matrixnode && parent_node) {
			m_matrixnode->setParent(parent_node);
			parent_node->updateAbsolutePosition();
			getPosRotMatrix().setTranslation(m_attachment_position);
			//setPitchYawRoll(getPosRotMatrix(), m_attachment_rotation);
			// use Irrlicht eulers instead
			getPosRotMatrix().setRotationDegrees(m_attachment_rotation);
			m_matrixnode->updateAbsolutePosition();
		}
	}
}

bool GenericCAO::visualExpiryRequired(const ObjectProperties &new_) const
{
	const ObjectProperties &old = m_prop;
	/* Visuals do not need to be expired for:
	 * - nametag props: handled by updateNametag()
	 * - textures:      handled by updateTextures()
	 * - sprite props:  handled by updateTextureAnim()
	 * - glow:          handled by updateLight()
	 * - any other properties that do not change appearance
	 */

	bool uses_legacy_texture = new_.wield_item.empty() &&
		(new_.visual == OBJECTVISUAL_WIELDITEM || new_.visual == OBJECTVISUAL_ITEM);
	// Ordered to compare primitive types before std::vectors
	return old.backface_culling != new_.backface_culling ||
		old.is_visible != new_.is_visible ||
		old.shaded != new_.shaded ||
		old.use_texture_alpha != new_.use_texture_alpha ||
		old.node != new_.node ||
		old.mesh != new_.mesh ||
		old.visual != new_.visual ||
		old.visual_size != new_.visual_size ||
		old.wield_item != new_.wield_item ||
		old.colors != new_.colors ||
		(uses_legacy_texture && old.textures != new_.textures);
}

void GenericCAO::processMessage(const std::string &data)
{
	//infostream<<"GenericCAO: Got message"<<std::endl;
	std::istringstream is(data, std::ios::binary);
	// command
	u8 cmd = readU8(is);
	if (cmd == AO_CMD_SET_PROPERTIES) {
		ObjectProperties newprops;
		newprops.show_on_minimap = m_is_player; // default

		newprops.deSerialize(is);

		// Check what exactly changed
		bool expire_visuals = visualExpiryRequired(newprops);
		bool textures_changed = m_prop.textures != newprops.textures;

		// Apply changes
		m_prop = std::move(newprops);

		m_selection_box = m_prop.selectionbox;
		m_selection_box.MinEdge *= BS;
		m_selection_box.MaxEdge *= BS;

		m_tx_size.X = 1.0f / m_prop.spritediv.X;
		m_tx_size.Y = 1.0f / m_prop.spritediv.Y;

		if(!m_initial_tx_basepos_set){
			m_initial_tx_basepos_set = true;
			m_tx_basepos = m_prop.initial_sprite_basepos;
		}
		if (m_is_local_player) {
			LocalPlayer *player = m_env->getLocalPlayer();
			player->makes_footstep_sound = m_prop.makes_footstep_sound;
			aabb3f collision_box = m_prop.collisionbox;
			collision_box.MinEdge *= BS;
			collision_box.MaxEdge *= BS;
			player->setCollisionbox(collision_box);
			player->setEyeHeight(m_prop.eye_height);
			player->setZoomFOV(m_prop.zoom_fov);
		}

		if ((m_is_player && !m_is_local_player) && m_prop.nametag.empty())
			m_prop.nametag = m_name;
		if (m_is_local_player)
			m_prop.show_on_minimap = false;

		if (expire_visuals) {
			expireVisuals();
		} else {
			if (textures_changed) {
				// don't update while punch texture modifier is active
				if (m_reset_textures_timer < 0)
					updateTextures(m_current_texture_modifier);
			}
			updateNametag();
			updateMarker();
		}
	} else if (cmd == AO_CMD_UPDATE_POSITION) {
		// Not sent by the server if this object is an attachment.
		// We might however get here if the server notices the object being detached before the client.
		m_position = readV3F32(is);
		m_velocity = readV3F32(is);
		m_acceleration = readV3F32(is);
		m_rotation = readV3F32(is);

		m_rotation = wrapDegrees_0_360_v3f(m_rotation);
		bool do_interpolate = readU8(is);
		bool is_end_position = readU8(is);
		float update_interval = readF32(is);

		if(getParent() != NULL) // Just in case
			return;

		if(do_interpolate)
		{
			if(!m_prop.physical)
				pos_translator.update(m_position, is_end_position, update_interval);
		} else {
			pos_translator.init(m_position);
		}
		rot_translator.update(m_rotation, false, update_interval);
		updateNodePos();
	} else if (cmd == AO_CMD_SET_TEXTURE_MOD) {
		std::string mod = deSerializeString16(is);

		// immediately reset an engine issued texture modifier if a mod sends a different one
		if (m_reset_textures_timer > 0) {
			m_reset_textures_timer = -1;
			updateTextures(m_previous_texture_modifier);
		}
		updateTextures(mod);
	} else if (cmd == AO_CMD_SET_SPRITE) {
		v2s16 p = readV2S16(is);
		int num_frames = readU16(is);
		float framelength = readF32(is);
		bool select_horiz_by_yawpitch = readU8(is);

		m_tx_basepos = p;
		m_anim_num_frames = num_frames;
		m_anim_frame = 0;
		m_anim_framelength = framelength;
		m_tx_select_horiz_by_yawpitch = select_horiz_by_yawpitch;

		updateTextureAnim();
	} else if (cmd == AO_CMD_SET_PHYSICS_OVERRIDE) {
		float override_speed = readF32(is);
		float override_jump = readF32(is);
		float override_gravity = readF32(is);
		// MT 0.4.10 legacy: send inverted for detault `true` if the server sends nothing
		bool sneak = !readU8(is);
		bool sneak_glitch = !readU8(is);
		bool new_move = !readU8(is);

		// new overrides since 5.8.0
		float override_speed_climb = readF32(is);
		float override_speed_crouch = readF32(is);
		float override_liquid_fluidity = readF32(is);
		float override_liquid_fluidity_smooth = readF32(is);
		float override_liquid_sink = readF32(is);
		float override_acceleration_default = readF32(is);
		float override_acceleration_air = readF32(is);
		if (is.eof()) {
			override_speed_climb = 1.0f;
			override_speed_crouch = 1.0f;
			override_liquid_fluidity = 1.0f;
			override_liquid_fluidity_smooth = 1.0f;
			override_liquid_sink = 1.0f;
			override_acceleration_default = 1.0f;
			override_acceleration_air = 1.0f;
		}

		// new overrides since 5.9.0
		float override_speed_fast = readF32(is);
		float override_acceleration_fast = readF32(is);
		float override_speed_walk = readF32(is);
		if (is.eof()) {
			override_speed_fast = 1.0f;
			override_acceleration_fast = 1.0f;
			override_speed_walk = 1.0f;
		}

		if (m_is_local_player) {
			auto &phys = m_env->getLocalPlayer()->physics_override;
			phys.speed = override_speed;
			phys.jump = override_jump;
			phys.gravity = override_gravity;
			phys.sneak = sneak;
			phys.sneak_glitch = sneak_glitch;
			phys.new_move = new_move;
			phys.speed_climb = override_speed_climb;
			phys.speed_crouch = override_speed_crouch;
			phys.liquid_fluidity = override_liquid_fluidity;
			phys.liquid_fluidity_smooth = override_liquid_fluidity_smooth;
			phys.liquid_sink = override_liquid_sink;
			phys.acceleration_default = override_acceleration_default;
			phys.acceleration_air = override_acceleration_air;
			phys.speed_fast = override_speed_fast;
			phys.acceleration_fast = override_acceleration_fast;
			phys.speed_walk = override_speed_walk;
		}
	} else if (cmd == AO_CMD_SET_ANIMATION) {
		v2f range = readV2F32(is);
		if (!m_is_local_player) {
			m_animation_range = range;
			m_animation_speed = readF32(is);
			m_animation_blend = readF32(is);
			// these are sent inverted so we get true when the server sends nothing
			m_animation_loop = !readU8(is);
			updateAnimation();
		} else {
			LocalPlayer *player = m_env->getLocalPlayer();
			if(player->last_animation == LocalPlayerAnimation::NO_ANIM)
			{
				m_animation_range = range;
				m_animation_speed = readF32(is);
				m_animation_blend = readF32(is);
				// these are sent inverted so we get true when the server sends nothing
				m_animation_loop = !readU8(is);
			}
			// update animation only if local animations present
			// and received animation is unknown (except idle animation)
			bool is_known = false;
			for (int i = 1;i<4;i++)
			{
				if(m_animation_range.Y == player->local_animations[i].Y)
					is_known = true;
			}
			if(!is_known ||
					(player->local_animations[1].Y + player->local_animations[2].Y < 1))
			{
					updateAnimation();
			}
			// FIXME: ^ This code is trash. It's also broken.
		}
	} else if (cmd == AO_CMD_SET_ANIMATION_SPEED) {
		m_animation_speed = readF32(is);
		updateAnimationSpeed();
	} else if (cmd == AO_CMD_SET_BONE_POSITION) {
		std::string bone = deSerializeString16(is);
		auto it = m_bone_override.find(bone);
		BoneOverride props;
		if (it != m_bone_override.end()) {
			props = it->second;
			// Reset timer
			props.dtime_passed = 0;
			// Save previous values for interpolation
			props.position.previous = props.position.vector;
			props.rotation.previous = props.rotation.next;
			props.scale.previous = props.scale.vector;
		} else {
			// Disable interpolation
			props.position.interp_timer = 0.0f;
			props.rotation.interp_timer = 0.0f;
			props.scale.interp_timer = 0.0f;
		}
		// Read new values
		props.position.vector = readV3F32(is);
		props.rotation.next = core::quaternion(readV3F32(is) * core::DEGTORAD);
		props.scale.vector = readV3F32(is); // reads past end of string on older cmds
		if (is.eof()) {
			// Backwards compatibility
			props.scale.vector = v3f(1, 1, 1); // restore the scale which was not sent
			props.position.absolute = true;
			props.rotation.absolute = true;
		} else {
			props.position.interp_timer = readF32(is);
			props.rotation.interp_timer = readF32(is);
			props.scale.interp_timer = readF32(is);
			u8 absoluteFlag = readU8(is);
			props.position.absolute = (absoluteFlag & 1) > 0;
			props.rotation.absolute = (absoluteFlag & 2) > 0;
			props.scale.absolute = (absoluteFlag & 4) > 0;
		}
		if (props.isIdentity()) {
			m_bone_override.erase(bone);
		} else {
			m_bone_override[bone] = props;
		}
	} else if (cmd == AO_CMD_ATTACH_TO) {
		u16 parent_id = readS16(is);
		std::string bone = deSerializeString16(is);
		v3f position = readV3F32(is);
		v3f rotation = readV3F32(is);
		bool force_visible = readU8(is); // Returns false for EOF

		setAttachment(parent_id, bone, position, rotation, force_visible);
	} else if (cmd == AO_CMD_PUNCHED) {
		u16 result_hp = readU16(is);

		// Use this instead of the send damage to not interfere with prediction
		s32 damage = (s32)m_hp - (s32)result_hp;

		m_hp = result_hp;

		if (m_is_local_player)
			m_env->getLocalPlayer()->hp = m_hp;

		if (damage > 0)
		{
			if (m_hp == 0)
			{
				// TODO: Execute defined fast response
				// As there is no definition, make a smoke puff
				ClientSimpleObject *simple = createSmokePuff(
						m_smgr, m_env, m_position,
						v2f(m_prop.visual_size.X, m_prop.visual_size.Y) * BS);
				m_env->addSimpleObject(simple);
			} else if (m_reset_textures_timer < 0 && !m_prop.damage_texture_modifier.empty()) {
				m_reset_textures_timer = 0.05;
				if(damage >= 2)
					m_reset_textures_timer += 0.05 * damage;
				// Cap damage overlay to 1 second
				m_reset_textures_timer = std::min(m_reset_textures_timer, 1.0f);
				updateTextures(m_current_texture_modifier + m_prop.damage_texture_modifier);
			}
		}

		if (m_hp == 0) {
			// Same as 'Server::DiePlayer'
			clearParentAttachment();
			// Same as 'ObjectRef::l_remove'
			if (!m_is_player)
				clearChildAttachments();
		}
	} else if (cmd == AO_CMD_UPDATE_ARMOR_GROUPS) {
		m_armor_groups.clear();
		int armor_groups_size = readU16(is);
		for(int i=0; i<armor_groups_size; i++)
		{
			std::string name = deSerializeString16(is);
			int rating = readS16(is);
			m_armor_groups[name] = rating;
		}
	} else if (cmd == AO_CMD_SPAWN_INFANT) {
		u16 child_id = readU16(is);
		u8 type = readU8(is); // maybe this will be useful later
		(void)type;

		addAttachmentChild(child_id);
	} else if (cmd == AO_CMD_OBSOLETE1) {
		// Don't do anything and also don't log a warning
	} else {
		warningstream << FUNCTION_NAME
			<< ": unknown command or outdated client \""
			<< +cmd << "\"" << std::endl;
	}
}

/* \pre punchitem != NULL
 */
bool GenericCAO::directReportPunch(v3f dir, const ItemStack *punchitem,
	const ItemStack *hand_item, float time_from_last_punch)
{
	assert(punchitem);	// pre-condition
	const ToolCapabilities *toolcap =
			&punchitem->getToolCapabilities(m_client->idef(), hand_item);
	PunchDamageResult result = getPunchDamage(
			m_armor_groups,
			toolcap,
			punchitem,
			time_from_last_punch,
			punchitem->wear);

	if(result.did_punch && result.damage != 0)
	{
		if(result.damage < m_hp)
		{
			m_hp -= result.damage;
		} else {
			m_hp = 0;
			// TODO: Execute defined fast response
			// As there is no definition, make a smoke puff
			ClientSimpleObject *simple = createSmokePuff(
					m_smgr, m_env, m_position,
					v2f(m_prop.visual_size.X, m_prop.visual_size.Y) * BS);
			m_env->addSimpleObject(simple);
		}
		if (m_reset_textures_timer < 0 && !m_prop.damage_texture_modifier.empty()) {
			m_reset_textures_timer = 0.05;
			if (result.damage >= 2)
				m_reset_textures_timer += 0.05 * result.damage;
			// Cap damage overlay to 1 second
			m_reset_textures_timer = std::min(m_reset_textures_timer, 1.0f);
			updateTextures(m_current_texture_modifier + m_prop.damage_texture_modifier);
		}
	}

	return false;
}

std::string GenericCAO::debugInfoText()
{
	std::ostringstream os(std::ios::binary);
	os<<"GenericCAO hp="<<m_hp<<"\n";
	os<<"armor={";
	for(ItemGroupList::const_iterator i = m_armor_groups.begin();
			i != m_armor_groups.end(); ++i)
	{
		os<<i->first<<"="<<i->second<<", ";
	}
	os<<"}";
	return os.str();
}

void GenericCAO::updateMeshCulling()
{
	if (!m_is_local_player)
		return;

	const bool hidden = m_client->getCamera()->getCameraMode() == CAMERA_MODE_FIRST;

	scene::ISceneNode *node = getSceneNode();

	if (!node)
		return;

	if (m_prop.visual ==  OBJECTVISUAL_UPRIGHT_SPRITE) {
		// upright sprite has no backface culling
		node->forEachMaterial([=] (auto &mat) {
			mat.FrontfaceCulling = hidden;
		});
		return;
	}

	if (hidden) {
		// Hide the mesh by culling both front and
		// back faces. Serious hackyness but it works for our
		// purposes. This also preserves the skeletal armature.
		node->forEachMaterial([] (auto &mat) {
			mat.BackfaceCulling = true;
			mat.FrontfaceCulling = true;
		});
	} else {
		// Restore mesh visibility.
		node->forEachMaterial([this] (auto &mat) {
			mat.BackfaceCulling = m_prop.backface_culling;
			mat.FrontfaceCulling = false;
		});
	}
}

// Prototype
static GenericCAO proto_GenericCAO(nullptr, nullptr);
