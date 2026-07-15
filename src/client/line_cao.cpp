// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#include "line_cao.h"
#include "client/client.h"
#include "client/clientenvironment.h"
#include "constants.h"
#include "light.h"
#include "map.h"
#include "nodedef.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include <ISceneManager.h>
#include <ISceneNode.h>
#include <IVideoDriver.h>
#include <CMeshBuffer.h>
#include <irr_ptr.h>
#include <sstream>

/*
	Custom scene node wrapping a single rebuildable SMeshBuffer. Kept
	private to line_cao.cpp - LineCAO owns and rebuilds it, nothing else
	touches it. Modeled on Clouds (src/client/clouds.h/.cpp)
	TODO: Maybe extract and make this general between line and clouds and particles
*/
class LineCAO::SceneNode : public scene::ISceneNode
{
public:
	explicit SceneNode(scene::ISceneManager *mgr) :
			scene::ISceneNode(mgr->getRootSceneNode(), mgr, -1)
	{
		m_meshbuffer.reset(new scene::SMeshBuffer());
		m_meshbuffer->setHardwareMappingHint(scene::EHM_DYNAMIC);
		setAutomaticCulling(scene::EAC_OFF);
	}

	void OnRegisterSceneNode() override
	{
		if (IsVisible) {
			SceneManager->registerNodeForRendering(this,
					m_transparent ? scene::ESNRP_TRANSPARENT : scene::ESNRP_SOLID);
		}
		ISceneNode::OnRegisterSceneNode();
	}

	void render() override
	{
		if (m_meshbuffer->getVertexCount() == 0)
			return;
		video::IVideoDriver *driver = SceneManager->getVideoDriver();
		driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
		driver->setMaterial(m_material);
		driver->drawMeshBuffer(m_meshbuffer.get());
	}

	const aabb3f &getBoundingBox() const override { return m_box; }
	u32 getMaterialCount() const override { return 1; }
	video::SMaterial &getMaterial(u32) override { return m_material; }

	scene::SMeshBuffer *getBuffer() { return m_meshbuffer.get(); }
	video::SMaterial &material() { return m_material; }
	void setTransparent(bool transparent) { m_transparent = transparent; }
	void updateBoundingBox() { m_box = m_meshbuffer->getBoundingBox(); }

private:
	irr_ptr<scene::SMeshBuffer> m_meshbuffer;
	video::SMaterial m_material;
	aabb3f m_box{{0.0f, 0.0f, 0.0f}};
	bool m_transparent = false;
};

static video::SColor multiplyColor(video::SColor c, video::SColor light)
{
	return video::SColor(c.getAlpha(),
			c.getRed() * light.getRed() / 255,
			c.getGreen() * light.getGreen() / 255,
			c.getBlue() * light.getBlue() / 255);
}

// Vertex positions are emitted relative to `origin`rather than in absolute
// world coordinates, to work with the client's camera-offset. `light_colors`
// is only used when `props.lit` is set.
static void appendLineStrip(scene::SMeshBuffer &buf, const std::vector<v3f> &points,
		const LineCAO::Properties &props, v3f origin,
		const std::vector<video::SColor> &light_colors)
{
	auto &vertices = buf.Vertices->Data;
	auto &indices = buf.Indices->Data;

	for (size_t i = 0; i < points.size(); i++) {
		video::SColor c = i < props.colors.size() ? props.colors[i] : props.color;
		if (props.lit && i < light_colors.size())
			c = multiplyColor(c, light_colors[i]);
		vertices.emplace_back(points[i] - origin, v3f(0, 1, 0), c, v2f(0, 0));
		indices.push_back((u16)i);
	}
}

LineCAO::LineCAO(Client *client, ClientEnvironment *env) :
		ClientActiveObject(0, client, env)
{
	// Runs during global static initialization for the file-scope
	// proto_LineCAO below (client == nullptr)
	if (!client)
		ClientActiveObject::registerType(getType(), create);
}

LineCAO::~LineCAO()
{
	removeFromScene(true);
}

std::unique_ptr<ClientActiveObject> LineCAO::create(Client *client, ClientEnvironment *env)
{
	return std::make_unique<LineCAO>(client, env);
}

void LineCAO::addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr)
{
	if (m_scenenode)
		return;
	m_scenenode = new SceneNode(smgr);
	rebuildMesh();
}

void LineCAO::removeFromScene(bool permanent)
{
	if (!m_scenenode)
		return;
	m_scenenode->remove();
	m_scenenode = nullptr;
}

scene::ISceneNode *LineCAO::getSceneNode() const
{
	return m_scenenode;
}

void LineCAO::step(float dtime, ClientEnvironment *env)
{
	updateNodePosition();
}

std::vector<video::SColor> LineCAO::computeLightColors(u32 day_night_ratio) const
{
	// Blend day/night here on the CPU, since no shader exists for lines.
	// same approach that Particle::updateLight() uses.
	const NodeDefManager *ndef = m_client->ndef();

	std::vector<video::SColor> light_colors(m_points.size());
	for (size_t i = 0; i < m_points.size(); i++) {
		bool pos_ok = false;
		v3s16 pos = floatToInt(m_points[i], BS);
		MapNode n = m_env->getMap().getNode(pos, &pos_ok);
		u8 light = pos_ok ? n.getLightBlend(day_night_ratio, ndef->getLightingFlags(n)) :
				blend_light(day_night_ratio, LIGHT_SUN, 0);
		u8 brightness = decode_light(light);
		light_colors[i] = video::SColor(255, brightness, brightness, brightness);
	}
	return light_colors;
}

void LineCAO::updateLight(u32 day_night_ratio)
{
	if (!m_properties.lit || m_points.empty())
		return;

	auto light_colors = computeLightColors(day_night_ratio);
	if (light_colors == m_light_colors)
		return;
	m_light_colors = std::move(light_colors);
	rebuildMesh();
}

void LineCAO::updateNodePosition()
{
	if (!m_scenenode || m_points.empty())
		return;
	v3s16 camera_offset = m_env->getCameraOffset();
	m_scenenode->setPosition(m_points[0] - intToFloat(camera_offset, BS));
}

void LineCAO::initialize(const std::string &data)
{
	std::istringstream is(data, std::ios::binary);
	u8 version = readU8(is);
	if (version != 1)
		return;
	deserializePoints(is);
	deserializeProperties(is);
}

void LineCAO::processMessage(const std::string &data)
{
	std::istringstream is(data, std::ios::binary);
	auto cmd = (LineCommand)readU8(is);
	switch (cmd) {
	case LineCommand::LINE_CMD_SET_POINTS:
		deserializePoints(is);
		break;
	case LineCommand::LINE_CMD_SET_PROPERTIES:
		deserializeProperties(is);
		break;
	default:
		return;
	}
	if (m_properties.lit)
		m_light_colors = computeLightColors(m_env->getDayNightRatio());
	rebuildMesh();
}

void LineCAO::deserializePoints(std::istream &is)
{
	u16 num_points = readU16(is);
	m_points.resize(num_points);
	for (v3f &p : m_points)
		p = readV3F32(is);
}

void LineCAO::deserializeProperties(std::istream &is)
{
	m_properties.color = readARGB8(is);
	u16 num_colors = readU16(is);
	m_properties.colors.resize(num_colors);
	for (video::SColor &c : m_properties.colors)
		c = readARGB8(is);
	u8 raw_alpha_mode = readU8(is);
	// Falls back to opaque for an out-of-range byte (corrupt/foreign data)
	// rather than casting blindly.
	m_properties.alpha_mode = raw_alpha_mode <= (u8)LineAlphaMode::LINE_ALPHA_ADD ?
			(LineAlphaMode)raw_alpha_mode : LineAlphaMode::LINE_ALPHA_OPAQUE;
	m_properties.lit = readU8(is);
}

void LineCAO::rebuildMesh()
{
	if (!m_scenenode)
		return;

	scene::SMeshBuffer *buf = m_scenenode->getBuffer();
	buf->Vertices->Data.clear();
	buf->Indices->Data.clear();

	v3f origin = m_points.empty() ? v3f() : m_points[0];

	if (m_points.size() >= 2) {
		buf->setPrimitiveType(scene::EPT_LINE_STRIP);
		appendLineStrip(*buf, m_points, m_properties, origin, m_light_colors);
	}

	buf->recalculateBoundingBox();
	buf->setDirty();
	m_scenenode->updateBoundingBox();
	updateNodePosition();

	updateMaterial();
}

void LineCAO::updateMaterial()
{
	if (!m_scenenode)
		return;

	video::SMaterial &material = m_scenenode->material();
	material.BackfaceCulling = false;
	material.FogEnable = true;

	bool transparent = false;

	switch (m_properties.alpha_mode) {
	case LineAlphaMode::LINE_ALPHA_BLEND:
		material.ZWriteEnable = video::EZW_OFF;
		material.MaterialType = video::EMT_ONETEXTURE_BLEND;
		material.MaterialTypeParam = video::pack_textureBlendFunc(
				video::EBF_SRC_ALPHA, video::EBF_ONE_MINUS_SRC_ALPHA,
				video::EMFN_MODULATE_1X, video::EAS_TEXTURE | video::EAS_VERTEX_COLOR);
		material.BlendOperation = video::EBO_ADD;
		transparent = true;
		break;
	case LineAlphaMode::LINE_ALPHA_ADD:
		material.ZWriteEnable = video::EZW_OFF;
		material.MaterialType = video::EMT_ONETEXTURE_BLEND;
		material.MaterialTypeParam = video::pack_textureBlendFunc(
				video::EBF_SRC_ALPHA, video::EBF_DST_ALPHA,
				video::EMFN_MODULATE_1X, video::EAS_TEXTURE | video::EAS_VERTEX_COLOR);
		material.BlendOperation = video::EBO_ADD;
		transparent = true;
		break;
	default: // OPAQUE
		material.ZWriteEnable = video::EZW_ON;
		material.MaterialType = video::EMT_SOLID;
		break;
	}

	m_scenenode->setTransparent(transparent);
}

// Prototype: registers ACTIVEOBJECT_TYPE_LINE's factory at static-init time.
static LineCAO proto_LineCAO(nullptr, nullptr);
