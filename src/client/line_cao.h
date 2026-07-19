// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#pragma once

#include "client/clientobject.h"
#include "line_common.h"
#include <SColor.h>
#include <vector>

/*
	Client-side counterpart of LineSAO (src/server/line_sao.h).
	It only renders whatever absolute point positions the server last sent,
	rebuilding its mesh when points or properties change via processMessage()
*/
class LineCAO : public ClientActiveObject
{
public:
	// Public so the free mesh-building functions in line_cao.cpp can take it by const-reference.
	struct Properties
	{
		video::SColor color = video::SColor(0xFFFFFFFF);
		std::vector<video::SColor> colors;
		LineAlphaMode alpha_mode = LineAlphaMode::LINE_ALPHA_OPAQUE;
		bool lit = true;
		float width = 0.1f; // Only used by shape == LINE_SHAPE_TUBE, in nodes
		LineShape shape = LineShape::LINE_SHAPE_LINE;
	};

	LineCAO(Client *client, ClientEnvironment *env);
	~LineCAO() override;

	ActiveObjectType getType() const override { return ACTIVEOBJECT_TYPE_LINE; }
	static std::unique_ptr<ClientActiveObject> create(Client *client, ClientEnvironment *env);

	void addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr) override;
	void removeFromScene(bool permanent) override;
	void initialize(const std::string &data) override;
	void processMessage(const std::string &data) override;
	scene::ISceneNode *getSceneNode() const override;

	// Only used to track the client's camera-offset recentering
	void step(float dtime, ClientEnvironment *env) override;
	// Only does anything when m_properties.lit is set - rebuilds the mesh
	// with per-point map light baked into vertex colors, same trigger
	// GenericCAO uses.
	void updateLight(u32 day_night_ratio) override;

	bool getCollisionBox(aabb3f *toset) const override { return false; }
	bool getSelectionBox(aabb3f *toset) const override { return false; }
	bool collideWithObjects() const override { return false; }

private:
	class SceneNode; // custom scene::ISceneNode, defined in line_cao.cpp

	void deserializePoints(std::istream &is);
	void deserializeProperties(std::istream &is);

	// Samples map light at each point's own position; empty vector semantics
	// mirror m_light_colors (only meaningful while m_properties.lit is set).
	std::vector<video::SColor> computeLightColors(u32 day_night_ratio) const;

	void rebuildMesh();
	void updateMaterial();
	// Positions the scene node at m_points[0], adjusted for the client's
	// current camera offset. Mesh vertices are built relative to
	// m_points[0], not in absolute world coordinates, same convention
	// GenericCAO::updateNodePos() uses.
	void updateNodePosition();

	SceneNode *m_scenenode = nullptr;

	std::vector<v3f> m_points;
	Properties m_properties;
	// One per point, sampled at that point's own position. Only populated
	// (and kept up to date by updateLight()) while m_properties.lit is set.
	std::vector<video::SColor> m_light_colors;
};
