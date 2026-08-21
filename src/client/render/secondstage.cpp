// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>
// Copyright (C) 2020 appgurueu, Lars Mueller <appgurulars@gmx.de>

#include "secondstage.h"
#include "client/client.h"
#include "client/camera.h"
#include "client/content_cao.h"
#include "client/localplayer.h"
#include "client/shader.h"
#include "settings.h"
#include "plain.h"
#include <ICameraSceneNode.h>
#include <ISceneManager.h>

PostProcessingStep::PostProcessingStep(u32 _shader_id, const std::vector<u8> &_texture_map) :
	shader_id(_shader_id), texture_map(_texture_map)
{
	assert(texture_map.size() <= video::MATERIAL_MAX_TEXTURES);
	configureMaterial();
}

void PostProcessingStep::configureMaterial()
{
	material.UseMipMaps = false;
	material.ZBuffer = video::ECFN_LESSEQUAL;
	material.ZWriteEnable = video::EZW_ON;
	for (u32 k = 0; k < texture_map.size(); ++k) {
		material.TextureLayers[k].AnisotropicFilter = 0;
		material.TextureLayers[k].MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
		material.TextureLayers[k].MagFilter = video::ETMAGF_NEAREST;
		material.TextureLayers[k].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		material.TextureLayers[k].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	}
}

void PostProcessingStep::setRenderSource(RenderSource *_source)
{
	source = _source;
}

void PostProcessingStep::setRenderTarget(RenderTarget *_target)
{
	target = _target;
}

void PostProcessingStep::reset(PipelineContext &context)
{
}

void PostProcessingStep::run(PipelineContext &context)
{
	if (target)
		target->activate(context);

	// attach the shader
	material.MaterialType = context.client->getShaderSource()->getShaderInfo(shader_id).material;

	auto driver = context.device->getVideoDriver();

	for (u32 i = 0; i < texture_map.size(); i++)
		material.TextureLayers[i].Texture = source->getTexture(texture_map[i]);

	static const video::SColor color = video::SColor(0, 0, 0, 255);
	static const video::S3DVertex vertices[4] = {
			video::S3DVertex(1.0, -1.0, 0.0, 0.0, 0.0, -1.0,
					color, 1.0, 0.0),
			video::S3DVertex(-1.0, -1.0, 0.0, 0.0, 0.0, -1.0,
					color, 0.0, 0.0),
			video::S3DVertex(-1.0, 1.0, 0.0, 0.0, 0.0, -1.0,
					color, 0.0, 1.0),
			video::S3DVertex(1.0, 1.0, 0.0, 0.0, 0.0, -1.0,
					color, 1.0, 1.0),
	};
	static const u16 indices[6] = {0, 1, 2, 2, 3, 0};
	driver->setMaterial(material);
	driver->drawVertexPrimitiveList(&vertices, 4, &indices, 2);
}

void PostProcessingStep::setBilinearFilter(u8 index, bool value)
{
	assert(index < video::MATERIAL_MAX_TEXTURES);
	material.TextureLayers[index].MinFilter = value ? video::ETMINF_LINEAR_MIPMAP_NEAREST : video::ETMINF_NEAREST_MIPMAP_NEAREST;
	material.TextureLayers[index].MagFilter = value ? video::ETMAGF_LINEAR : video::ETMAGF_NEAREST;
}

void SharedDepthTextureOutput::activate(PipelineContext &context)
{
	// Never let the base class issue its blanket clear, which would destroy the
	// borrowed depth buffer. Clearing our own colour attachment afterwards is
	// enough: the mask has to start from zero every frame, the depth does not.
	m_clear = false;
	TextureBufferOutput::activate(context);
	context.device->getVideoDriver()->clearBuffers(video::ECBF_COLOR,
			video::SColor(0, 0, 0, 0));
}

CameraRigidMaskStep::CameraRigidMaskStep(Client *client) :
	m_client(client)
{
	auto *driver = client->getSceneManager()->getVideoDriver();
	auto *shdsrc = client->getShaderSource();

	// The mask is drawn over entity meshes, which may be skinned, so the shader
	// has to be built with skinning support exactly as the shadow depth shader
	// is. One variant covers both cases: the vertex shader falls back to the
	// unskinned path when the weight attribute is disabled.
	ShaderConstants consts;
	const auto max_joints = driver->getMaxJointTransforms();
	if (max_joints > 0) {
		consts["USE_SKINNING"] = 1;
		consts["MAX_JOINTS"] = max_joints;
	}

	u32 shader_id = shdsrc->getShader("object_mask", consts, video::EMT_SOLID);
	m_mask_material = shdsrc->getShaderInfo(shader_id).material;
}

void CameraRigidMaskStep::renderObject(video::IVideoDriver *driver, GenericCAO *cao)
{
	scene::ISceneNode *node = cao->getSceneNode();
	if (node && node->isVisible()) {
		const u32 count = node->getMaterialCount();

		// Swap in the mask shader, following the same backup/restore dance the
		// shadow renderer uses for entities. Depth writes are disabled because
		// the depth buffer is shared with the scene: the values would come out
		// identical, but writing to a buffer later steps read from is a trap
		// worth not leaving lying around.
		m_saved_material_types.clear();
		m_saved_material_types.reserve(count);
		for (u32 i = 0; i < count; i++) {
			auto &material = node->getMaterial(i);
			m_saved_material_types.push_back(material.MaterialType);
			material.MaterialType = m_mask_material;
		}

		auto &override_material = driver->getOverrideMaterial();
		override_material.reset();
		override_material.Material.ZWriteEnable = video::EZW_OFF;
		override_material.EnableProps = video::EMP_ZWRITE_ENABLE;
		// We are drawing outside the scene manager, so the "enabled in this
		// pass" flag the scene manager would normally maintain is ours to set.
		override_material.Enabled = true;

		driver->setTransform(video::ETS_WORLD, node->getAbsoluteTransformation());
		node->render();

		override_material.reset();

		for (u32 i = 0; i < count; i++)
			node->getMaterial(i).MaterialType = m_saved_material_types[i];
	}

	// Recurse into anything attached to this object. Note that renderObject
	// reuses m_saved_material_types, so the recursion has to come after this
	// object is done with it.
	for (auto child_id : cao->getAttachmentChildIds()) {
		if (GenericCAO *child = m_client->getEnv().getGenericCAO(child_id))
			renderObject(driver, child);
	}
}

void CameraRigidMaskStep::run(PipelineContext &context)
{
	if (m_target)
		m_target->activate(context);

	LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	if (!player)
		return;

	// Nothing will be blurred this frame, so nothing needs masking. The target
	// was still activated above, which clears the mask to zero.
	if (resolveMotionBlurStrength(player->getLighting(),
			g_settings->getBool("enable_motion_blur"),
			g_settings->getFloat("motion_blur_strength", 0.0f, 4.0f)) <= 0.0f)
		return;

	GenericCAO *cao = player->getCAO();
	if (!cao)
		return;

	auto *driver = context.device->getVideoDriver();

	// The camera transforms are still those left behind by the 3D draw, but set
	// them explicitly rather than relying on that.
	auto *camera_node = m_client->getCamera()->getCameraNode();
	driver->setTransform(video::ETS_PROJECTION, camera_node->getProjectionMatrix());
	driver->setTransform(video::ETS_VIEW, camera_node->getViewMatrix());

	// Climb to the root of the attachment tree the player belongs to (the boat
	// it is sitting in, say) and mask that whole tree.
	GenericCAO *root = cao;
	while (ClientActiveObject *parent = root->getParent()) {
		GenericCAO *parent_cao = dynamic_cast<GenericCAO *>(parent);
		if (!parent_cao)
			break;
		root = parent_cao;
	}

	renderObject(driver, root);
}

RenderStep *addPostProcessing(RenderPipeline *pipeline, RenderStep *previousStep, v2f scale, Client *client)
{
	auto buffer = pipeline->createOwned<TextureBuffer>();
	auto driver = client->getSceneManager()->getVideoDriver();

	// configure texture formats
	video::ECOLOR_FORMAT color_format = selectColorFormat(driver);
	video::ECOLOR_FORMAT depth_format = selectDepthFormat(driver);

	verbosestream << "addPostProcessing(): color = "
		<< video::ColorFormatName(color_format) << ", depth = "
		<< video::ColorFormatName(depth_format) << std::endl;

	// init post-processing buffer
	static const u8 TEXTURE_COLOR = 0;
	static const u8 TEXTURE_DEPTH = 1;
	static const u8 TEXTURE_BLOOM = 2;
	static const u8 TEXTURE_EXPOSURE_1 = 3;
	static const u8 TEXTURE_EXPOSURE_2 = 4;
	static const u8 TEXTURE_FXAA = 5;
	static const u8 TEXTURE_VOLUME = 6;

	static const u8 TEXTURE_MSAA_COLOR = 7;
	static const u8 TEXTURE_MSAA_DEPTH = 8;

	static const u8 TEXTURE_MOTIONBLUR = 9;
	static const u8 TEXTURE_RIGID_MASK = 30;

	static const u8 TEXTURE_SCALE_DOWN = 10;
	static const u8 TEXTURE_SCALE_UP = 20;

	// because bloom_format is floating point
	const bool bloom_available = driver->queryFeature(video::EVDF_RENDER_TO_FLOAT_TEXTURE);
	const bool enable_bloom = g_settings->getBool("enable_bloom") && bloom_available;
	const bool enable_volumetric_light = g_settings->getBool("enable_volumetric_lighting") && enable_bloom;
	const bool enable_auto_exposure = g_settings->getBool("enable_auto_exposure") && bloom_available;
	if (g_settings->getBool("enable_bloom") && !bloom_available) {
		warningstream << "Ignoring configured bloom since it's not supported by "
			"the current video driver." << std::endl;
	}
	if (g_settings->getBool("enable_auto_exposure") && !bloom_available) {
		warningstream << "Ignoring configured auto exposure since it's not supported by "
			"the current video driver." << std::endl;
	}

	verbosestream << "addPostProcessing(): bloom = "
		<< enable_bloom << (enable_volumetric_light ? " + volumetric" : "")
		<< ", exposure = " << enable_auto_exposure << std::endl;

	const std::string antialiasing = g_settings->get("antialiasing");
	const u16 antialiasing_scale = MYMAX(2, g_settings->getU16("fsaa"));

	// This code only deals with MSAA in combination with post-processing. MSAA without
	// post-processing works via a flag at OpenGL context creation instead.
	// To make MSAA work with post-processing, we need multisample texture support,
	// which has higher OpenGL (ES) version requirements.
	// Note: This is not about renderbuffer objects, but about textures,
	// since that's what we use and what Irrlicht allows us to use.

	const bool msaa_available = driver->queryFeature(video::EVDF_TEXTURE_MULTISAMPLE);
	const bool enable_msaa = antialiasing == "fsaa" && msaa_available;
	if (antialiasing == "fsaa" && !msaa_available) {
		warningstream << "Ignoring configured FSAA since it's not supported in "
			"combination with post-processing by the current video driver." << std::endl;
	}

	const bool enable_ssaa = antialiasing == "ssaa";
	const bool enable_fxaa = g_settings->getBool("fxaa");
	// Note this is deliberately NOT gated on the `enable_motion_blur` setting.
	// A server can push a motion blur strength that overrides the player's
	// settings, including switching the effect on for someone who has it off,
	// so the pass has to exist even when the player does not currently want it.
	// It idles cheaply in that case: the shader early-outs at zero velocity and
	// the mask step below skips its geometry entirely.
	const bool enable_motion_blur = true;

	verbosestream << "addPostProcessing(): AA = "
		<< (enable_msaa ? "msaa" : enable_ssaa ? "ssaa" : "none")
		<< " " << antialiasing_scale << "x" << (enable_fxaa ? " + fxaa" : "") << std::endl;

	// Super-sampling is simply rendering into a larger texture.
	// Downscaling is done by the final step when rendering to the screen.
	if (enable_ssaa) {
		scale *= antialiasing_scale;
	}

	if (enable_msaa) {
		buffer->setTexture(TEXTURE_MSAA_COLOR, scale, "3d_render_msaa", color_format, false, antialiasing_scale);
		buffer->setTexture(TEXTURE_MSAA_DEPTH, scale, "3d_depthmap_msaa", depth_format, false, antialiasing_scale);
	}

	buffer->setTexture(TEXTURE_COLOR, scale, "3d_render", color_format);
	buffer->setTexture(TEXTURE_EXPOSURE_1, core::dimension2du(1,1), "exposure_1", color_format, /*clear:*/ true);
	buffer->setTexture(TEXTURE_EXPOSURE_2, core::dimension2du(1,1), "exposure_2", color_format, /*clear:*/ true);
	buffer->setTexture(TEXTURE_DEPTH, scale, "3d_depthmap", depth_format);

	// attach buffer to the previous step
	if (enable_msaa) {
		TextureBufferOutput *msaa = pipeline->createOwned<TextureBufferOutput>(buffer, std::vector<u8> { TEXTURE_MSAA_COLOR }, TEXTURE_MSAA_DEPTH);
		previousStep->setRenderTarget(msaa);
		TextureBufferOutput *normal = pipeline->createOwned<TextureBufferOutput>(buffer, std::vector<u8> { TEXTURE_COLOR }, TEXTURE_DEPTH);
		pipeline->addStep<ResolveMSAAStep>(msaa, normal);
	} else {
		previousStep->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, std::vector<u8> { TEXTURE_COLOR }, TEXTURE_DEPTH));
	}

	// shared variables
	u32 shader_id;

	// Number of mipmap levels of the bloom downsampling texture
	// (this affects the bloom strength, so don't blindly change it)
	const u8 MIPMAP_LEVELS = 4;

	// color_format can be a normalized integer format, but bloom requires
	// values outside of [0,1] so this needs to be a different one.
	const auto bloom_format = video::ECF_A16B16G16R16F;

	// post-processing stage

	// The base color texture that all following steps read from. Motion blur,
	// if enabled, replaces it with a blurred copy.
	u8 base = TEXTURE_COLOR;

	// Camera (velocity) motion blur. Reconstructs each pixel's world position
	// from the depth buffer and reprojects it with the previous frame's
	// view-projection matrix to obtain a screen-space velocity, then blurs
	// the color buffer along it.
	if (enable_motion_blur) {
		// Mask of objects that move with the camera (the player's own body and
		// whatever it is riding). These are motionless on screen, so the blur
		// has to be told to leave them alone; see CameraRigidMaskStep. A plain
		// 8-bit target is plenty for a 0/1 mask. This shares TEXTURE_DEPTH, so
		// it must run after the 3D draw has filled it and before anything
		// overwrites it.
		buffer->setTexture(TEXTURE_RIGID_MASK, scale, "rigidmask", video::ECF_A8R8G8B8);
		auto rigid_mask = pipeline->addStep<CameraRigidMaskStep>(client);
		rigid_mask->setRenderTarget(pipeline->createOwned<SharedDepthTextureOutput>(
				buffer, std::vector<u8> { TEXTURE_RIGID_MASK }, TEXTURE_DEPTH));

		buffer->setTexture(TEXTURE_MOTIONBLUR, scale, "motionblur", color_format);
		// The sample count is baked into the shader as a compile-time constant
		// so that its sampling loop has a literal bound and can be unrolled.
		// One program is compiled and cached per distinct value; the cost is
		// that changing the setting only takes effect on world reload.
		ShaderConstants motion_blur_constants;
		motion_blur_constants["MOTION_BLUR_SAMPLES"] =
				(int)rangelim(g_settings->getS32("motion_blur_quality"), 2, 32);
		shader_id = client->getShaderSource()->getShader("motion_blur",
				motion_blur_constants, video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
		// Feed the (previous frame's) exposure texture as texture2 so the blur
		// can scale its length with exposure: longer smear in dark,
		// exposure-boosted scenes and shorter in bright ones. TEXTURE_EXPOSURE_1
		// holds the settled exposure at this point in the pipeline (it is
		// swapped in at the end of the frame).
		//
		// This is bound unconditionally on purpose. The shader guards its use
		// behind ENABLE_AUTO_EXPOSURE, which the shader source derives from the
		// raw `enable_auto_exposure` setting, whereas `enable_auto_exposure`
		// here *also* requires float render target support. When those two
		// disagree the shader would sample an unbound sampler. The texture is
		// created unconditionally above and cleared to zero, which decodes to a
		// neutral exposure factor of 1.
		std::vector<u8> motion_blur_textures {
				TEXTURE_COLOR, TEXTURE_DEPTH, TEXTURE_EXPOSURE_1, TEXTURE_RIGID_MASK };
		auto motion_blur = pipeline->addStep<PostProcessingStep>(shader_id, motion_blur_textures);
		motion_blur->setRenderSource(buffer);
		motion_blur->setBilinearFilter(0, true);
		motion_blur->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_MOTIONBLUR));
		base = TEXTURE_MOTIONBLUR;
	}

	u8 source = base;

	// common downsampling step for bloom or autoexposure
	if (enable_bloom || enable_auto_exposure) {

		v2f downscale = scale * 0.5f;
		for (u8 i = 0; i < MIPMAP_LEVELS; i++) {
			buffer->setTexture(TEXTURE_SCALE_DOWN + i, downscale, std::string("downsample") + std::to_string(i), bloom_format);
			if (enable_bloom)
				buffer->setTexture(TEXTURE_SCALE_UP + i, downscale, std::string("upsample") + std::to_string(i), bloom_format);
			downscale *= 0.5f;
		}

		if (enable_bloom) {
			buffer->setTexture(TEXTURE_BLOOM, scale, "bloom", bloom_format);

			// get bright spots
			u32 shader_id = client->getShaderSource()->getShaderRaw("extract_bloom");
			RenderStep *extract_bloom = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { source, TEXTURE_EXPOSURE_1 });
			extract_bloom->setRenderSource(buffer);
			extract_bloom->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_BLOOM));
			source = TEXTURE_BLOOM;
		}

		u8 downsample_start = 0;
		if (enable_volumetric_light) {
			// The volumetric pass is expensive (a raymarch per target pixel)
			// and its output only reaches the screen through the blur of the
			// bloom chain, whose first target is half resolution anyway. So
			// with performance_tradeoffs enabled, render it directly into the
			// first downsample target and skip the then-redundant first
			// bloom_downsample step.
			const bool half_res = g_settings->getFlag("performance_tradeoffs");

			shader_id = client->getShaderSource()->getShaderRaw("volumetric_light");
			auto volume = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { source, TEXTURE_DEPTH });
			volume->setRenderSource(buffer);
			if (half_res) {
				// Only the color input is minified; depth stays unfiltered
				// because the shader does exact comparisons against 1.0.
				volume->setBilinearFilter(0, true);
				volume->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_SCALE_DOWN));
				source = TEXTURE_SCALE_DOWN;
				downsample_start = 1;
			} else {
				buffer->setTexture(TEXTURE_VOLUME, scale, "volume", bloom_format);
				volume->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_VOLUME));
				source = TEXTURE_VOLUME;
			}
		}

		// downsample
		shader_id = client->getShaderSource()->getShaderRaw("bloom_downsample");
		for (u8 i = downsample_start; i < MIPMAP_LEVELS; i++) {
			auto step = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { source });
			step->setRenderSource(buffer);
			step->setBilinearFilter(0, true);
			step->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_SCALE_DOWN + i));
			source = TEXTURE_SCALE_DOWN + i;
		}
	}

	// Bloom pt 2
	if (enable_bloom) {
		// upsample
		shader_id = client->getShaderSource()->getShaderRaw("bloom_upsample");
		for (u8 i = MIPMAP_LEVELS - 1; i > 0; i--) {
			auto step = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { u8(TEXTURE_SCALE_DOWN + i - 1), source });
			step->setRenderSource(buffer);
			step->setBilinearFilter(0, true);
			step->setBilinearFilter(1, true);
			step->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, u8(TEXTURE_SCALE_UP + i - 1)));
			source = TEXTURE_SCALE_UP + i - 1;
		}
	}

	// Dynamic Exposure pt2
	if (enable_auto_exposure) {
		shader_id = client->getShaderSource()->getShaderRaw("update_exposure");
		auto update_exposure = pipeline->addStep<PostProcessingStep>(shader_id, std::vector<u8> { TEXTURE_EXPOSURE_1, u8(TEXTURE_SCALE_DOWN + MIPMAP_LEVELS - 1) });
		update_exposure->setBilinearFilter(1, true);
		update_exposure->setRenderSource(buffer);
		update_exposure->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_EXPOSURE_2));
	}

	// FXAA
	u8 final_stage_source = base;

	if (enable_fxaa) {
		final_stage_source = TEXTURE_FXAA;

		buffer->setTexture(TEXTURE_FXAA, scale, "fxaa", color_format);
		shader_id = client->getShaderSource()->getShaderRaw("fxaa");
		PostProcessingStep *effect = pipeline->createOwned<PostProcessingStep>(shader_id, std::vector<u8> { base });
		pipeline->addStep(effect);
		effect->setBilinearFilter(0, true);
		effect->setRenderSource(buffer);
		effect->setRenderTarget(pipeline->createOwned<TextureBufferOutput>(buffer, TEXTURE_FXAA));
	}

	// final merge
	shader_id = client->getShaderSource()->getShaderRaw("second_stage");
	PostProcessingStep *effect = pipeline->createOwned<PostProcessingStep>(shader_id, std::vector<u8> { final_stage_source, TEXTURE_SCALE_UP, TEXTURE_EXPOSURE_2 });
	pipeline->addStep(effect);
	if (enable_ssaa)
		effect->setBilinearFilter(0, true);
	effect->setBilinearFilter(1, true);
	effect->setRenderSource(buffer);

	if (enable_auto_exposure) {
		pipeline->addStep<SwapTexturesStep>(buffer, TEXTURE_EXPOSURE_1, TEXTURE_EXPOSURE_2);
	}

	return effect;
}

void ResolveMSAAStep::run(PipelineContext &context)
{
	context.device->getVideoDriver()->blitRenderTarget(msaa_fbo->getIrrRenderTarget(context),
			target_fbo->getIrrRenderTarget(context));
}
