// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 cx384

#include "drawItemStack.h"

#include <string>
#include "settings.h"
#include "client/client.h"
#include "porting.h"
#include "inventory.h"
#include "client/mesh.h"
#include "client/wieldmesh.h"
#include "client/texturesource.h"
#include "client/guiscalingfilter.h"
#include "client/item_visuals_manager.h"

struct MeshTimeInfo {
	u64 time;
	scene::IMesh *mesh = nullptr;
};

void drawItemStack(
		video::IVideoDriver *driver,
		gui::IGUIFont *font,
		const ItemStack &item,
		const core::rect<s32> &rect,
		const core::rect<s32> *clip,
		Client *client,
		ItemRotationKind rotation_kind,
		const v3s16 &angle,
		const v3s16 &rotation_speed)
{
	static MeshTimeInfo rotation_time_infos[IT_ROT_NONE];

	if (item.empty()) {
		if (rotation_kind < IT_ROT_NONE && rotation_kind != IT_ROT_OTHER) {
			rotation_time_infos[rotation_kind].mesh = NULL;
		}
		return;
	}

	const bool enable_animations = g_settings->getBool("inventory_items_animations");

	auto *idef = client->idef();
	const ItemDefinition &def = item.getDefinition(idef);
	ItemVisualsManager* item_visuals = client->getItemVisualsManager();

	bool draw_overlay = false;

	const std::string inventory_image = item.getInventoryImage(idef);
	const std::string inventory_overlay = item.getInventoryOverlay(idef);

	bool has_mesh = false;
	ItemMesh *imesh;

	core::rect<s32> viewrect = rect;
	if (clip != nullptr)
		viewrect.clipAgainst(*clip);

	// Render as mesh if animated or no inventory image
	if ((enable_animations && rotation_kind < IT_ROT_NONE) || inventory_image.empty()) {
		imesh = item_visuals->getWieldMesh(item, client);
		has_mesh = imesh && imesh->mesh;
	}
	if (has_mesh) {
		scene::IMesh *mesh = imesh->mesh;
		driver->clearBuffers(video::ECBF_DEPTH);
		s32 delta = 0;
		if (rotation_kind < IT_ROT_NONE) {
			MeshTimeInfo &ti = rotation_time_infos[rotation_kind];
			if (mesh != ti.mesh && rotation_kind != IT_ROT_OTHER) {
				ti.mesh = mesh;
				ti.time = porting::getTimeMs();
			} else {
				delta = porting::getDeltaMs(ti.time, porting::getTimeMs()) % 100000;
			}
		}
		core::rect<s32> oldViewPort = driver->getViewPort();
		core::matrix4 oldProjMat = driver->getTransform(video::ETS_PROJECTION);
		core::matrix4 oldViewMat = driver->getTransform(video::ETS_VIEW);

		core::matrix4 ProjMatrix;
		ProjMatrix.buildProjectionMatrixOrthoLH(2.0f, 2.0f, -1.0f, 100.0f);

		core::matrix4 ViewMatrix;
		ViewMatrix.buildProjectionMatrixOrthoLH(
			2.0f * viewrect.getWidth() / rect.getWidth(),
			2.0f * viewrect.getHeight() / rect.getHeight(),
			-1.0f,
			100.0f);
		ViewMatrix.setTranslation(core::vector3df(
			1.0f * (rect.LowerRightCorner.X + rect.UpperLeftCorner.X -
					viewrect.LowerRightCorner.X - viewrect.UpperLeftCorner.X) /
					viewrect.getWidth(),
			1.0f * (viewrect.LowerRightCorner.Y + viewrect.UpperLeftCorner.Y -
					rect.LowerRightCorner.Y - rect.UpperLeftCorner.Y) /
					viewrect.getHeight(),
			0.0f));

		driver->setTransform(video::ETS_PROJECTION, ProjMatrix);
		driver->setTransform(video::ETS_VIEW, ViewMatrix);

		core::matrix4 matrix;
		matrix.makeIdentity();

		if (enable_animations) {
			float timer_f = (float) delta / 5000.f;
			matrix.setRotationDegrees(v3f(
				angle.X + rotation_speed.X * 3.60f * timer_f,
				angle.Y + rotation_speed.Y * 3.60f * timer_f,
				angle.Z + rotation_speed.Z * 3.60f * timer_f)
			);
		}

		driver->setTransform(video::ETS_WORLD, matrix);
		driver->setViewPort(viewrect);

		video::SColor basecolor = item_visuals->getItemstackColor(item, client);

		const u32 mc = mesh->getMeshBufferCount();
		if (mc > imesh->buffer_info.size())
			imesh->buffer_info.resize(mc);
		for (u32 j = 0; j < mc; ++j) {
			scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
			video::SColor c = basecolor;

			auto &p = imesh->buffer_info[j];
			p.applyOverride(c);

			// TODO: could be moved to a shader
			if (p.needColorize(c)) {
				buf->setDirty(scene::EBT_VERTEX);
				if (imesh->needs_shading)
					colorizeMeshBuffer(buf, &c);
				else
					setMeshBufferColor(buf, c);
			}

			video::SMaterial &material = buf->getMaterial();

			// Texture animation
			if (p.animation_info) {
				p.animation_info->updateTexture(material, client->getAnimationTime());
			}

			material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			driver->setMaterial(material);
			driver->drawMeshBuffer(buf);
		}

		driver->setTransform(video::ETS_VIEW, oldViewMat);
		driver->setTransform(video::ETS_PROJECTION, oldProjMat);
		driver->setViewPort(oldViewPort);

		draw_overlay = def.type == ITEM_NODE && inventory_image.empty();
	} else { // Otherwise just draw as 2D
		video::ITexture *texture = item_visuals->getInventoryTexture(item, client);
		video::SColor color;
		if (texture) {
			color = item_visuals->getItemstackColor(item, client);
		} else {
			color = video::SColor(255, 255, 255, 255);
			ITextureSource *tsrc = client->getTextureSource();
			texture = tsrc->getTexture("no_texture.png");
			if (!texture)
				return;
		}

		const video::SColor colors[] = { color, color, color, color };

		draw2DImageFilterScaled(driver, texture, rect,
			core::rect<s32>({0, 0}, core::dimension2di(texture->getOriginalSize())),
			clip, colors, true);

		draw_overlay = true;
	}

	// draw the inventory_overlay
	if (!inventory_overlay.empty() && draw_overlay) {
		ITextureSource *tsrc = client->getTextureSource();
		video::ITexture *overlay_texture = tsrc->getTexture(inventory_overlay);
		core::dimension2d<u32> dimens = overlay_texture->getOriginalSize();
		core::rect<s32> srcrect(0, 0, dimens.Width, dimens.Height);
		draw2DImageFilterScaled(driver, overlay_texture, rect, srcrect, clip, 0, true);
	}

	if (def.type == ITEM_TOOL && item.wear != 0) {
		// Draw a progressbar
		float barheight = static_cast<float>(rect.getHeight()) / 16;
		float barpad_x = static_cast<float>(rect.getWidth()) / 16;
		float barpad_y = static_cast<float>(rect.getHeight()) / 16;

		core::rect<s32> progressrect(
			rect.UpperLeftCorner.X + barpad_x,
			rect.LowerRightCorner.Y - barpad_y - barheight,
			rect.LowerRightCorner.X - barpad_x,
			rect.LowerRightCorner.Y - barpad_y);

		// Shrink progressrect by amount of tool damage
		float wear = item.wear / 65535.0f;
		int progressmid =
			wear * progressrect.UpperLeftCorner.X +
			(1 - wear) * progressrect.LowerRightCorner.X;

		// Compute progressbar color
		// default scheme:
		//   wear = 0.0: green
		//   wear = 0.5: yellow
		//   wear = 1.0: red

		video::SColor color;
		auto barParams = item.getWearBarParams(client->idef());
		if (barParams.has_value()) {
			f32 durabilityPercent = 1.0 - wear;
			color = barParams->getWearBarColor(durabilityPercent);
		} else {
			color = video::SColor(255, 255, 255, 255);
			int wear_i = MYMIN(std::floor(wear * 600), 511);
			wear_i = MYMIN(wear_i + 10, 511);

			if (wear_i <= 255)
				color.set(255, wear_i, 255, 0);
			else
				color.set(255, 255, 511 - wear_i, 0);
		}

		core::rect<s32> progressrect2 = progressrect;
		progressrect2.LowerRightCorner.X = progressmid;
		driver->draw2DRectangle(color, progressrect2, clip);

		color = video::SColor(255, 0, 0, 0);
		progressrect2 = progressrect;
		progressrect2.UpperLeftCorner.X = progressmid;
		driver->draw2DRectangle(color, progressrect2, clip);
	}

	const std::string &count_text = item.metadata.getString("count_meta");
	if (font != nullptr && (item.count >= 2 || !count_text.empty())) {
		// Get the item count as a string
		std::string text = count_text.empty() ? itos(item.count) : count_text;
		v2u32 dim = font->getDimension(utf8_to_wide(unescape_enriched(text)).c_str());
		v2s32 sdim(dim.X, dim.Y);

		core::rect<s32> rect2(
			rect.LowerRightCorner - sdim,
			rect.LowerRightCorner
		);

		// get the count alignment
		s32 count_alignment = stoi(item.metadata.getString("count_alignment"));
		if (count_alignment != 0) {
			s32 a_x = count_alignment & 3;
			s32 a_y = (count_alignment >> 2) & 3;

			s32 x1, x2, y1, y2;
			switch (a_x) {
			case 1: // left
				x1 = rect.UpperLeftCorner.X;
				x2 = x1 + sdim.X;
				break;
			case 2: // middle
				x1 = (rect.UpperLeftCorner.X + rect.LowerRightCorner.X - sdim.X) / 2;
				x2 = x1 + sdim.X;
				break;
			case 3: // right
				x2 = rect.LowerRightCorner.X;
				x1 = x2 - sdim.X;
				break;
			default: // 0 = default
				x1 = rect2.UpperLeftCorner.X;
				x2 = rect2.LowerRightCorner.X;
				break;
			}

			switch (a_y) {
			case 1: // up
				y1 = rect.UpperLeftCorner.Y;
				y2 = y1 + sdim.Y;
				break;
			case 2: // middle
				y1 = (rect.UpperLeftCorner.Y + rect.LowerRightCorner.Y - sdim.Y) / 2;
				y2 = y1 + sdim.Y;
				break;
			case 3: // down
				y2 = rect.LowerRightCorner.Y;
				y1 = y2 - sdim.Y;
				break;
			default: // 0 = default
				y1 = rect2.UpperLeftCorner.Y;
				y2 = rect2.LowerRightCorner.Y;
				break;
			}

			rect2 = core::rect<s32>(x1, y1, x2, y2);
		}

		video::SColor color(255, 255, 255, 255);
		font->draw(utf8_to_wide(text).c_str(), rect2, color, false, false, &viewrect);
	}
}

void drawItemStack(
		video::IVideoDriver *driver,
		gui::IGUIFont *font,
		const ItemStack &item,
		const core::rect<s32> &rect,
		const core::rect<s32> *clip,
		Client *client,
		ItemRotationKind rotation_kind)
{
	drawItemStack(driver, font, item, rect, clip, client, rotation_kind,
		v3s16(0, 0, 0), v3s16(0, 100, 0));
}
