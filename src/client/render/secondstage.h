// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#pragma once
#include "pipeline.h"
#include <vector>

class GenericCAO;

/**
 *  Step to apply post-processing filter to the rendered image
 */
class PostProcessingStep : public RenderStep
{
public:
	/**
	 * Construct a new PostProcessingStep object
	 *
	 * @param shader_id ID of the shader in IShaderSource
	 * @param texture_map Map of textures to be chosen from the render source
	 */
	PostProcessingStep(u32 shader_id, const std::vector<u8> &texture_map);


	void setRenderSource(RenderSource *source) override;
	void setRenderTarget(RenderTarget *target) override;
	void reset(PipelineContext &context) override;
	void run(PipelineContext &context) override;

	/**
	 * Configure bilinear filtering for a specific texture layer
	 *
	 * @param index Index of the texture layer
	 * @param value true to enable the bilinear filter, false to disable
	 */
	void setBilinearFilter(u8 index, bool value);
private:
	u32 shader_id;
	std::vector<u8> texture_map;
	RenderSource *source { nullptr };
	RenderTarget *target { nullptr };
	video::SMaterial material;

	void configureMaterial();
};


class ResolveMSAAStep : public TrivialRenderStep
{
public:
	ResolveMSAAStep(TextureBufferOutput *_msaa_fbo, TextureBufferOutput *_target_fbo) :
			msaa_fbo(_msaa_fbo), target_fbo(_target_fbo) {};
	void run(PipelineContext &context) override;

private:
	TextureBufferOutput *msaa_fbo;
	TextureBufferOutput *target_fbo;
};


/**
 * Render target that borrows the depth buffer of an earlier step.
 *
 * TextureBufferOutput clears every attachment on its first activation of a
 * frame. That is wrong when the depth attachment is shared: it would wipe the
 * scene depth that this target wants to test against, and that later steps still
 * need to read. Suppress the blanket clear and clear only the colour attachment.
 */
class SharedDepthTextureOutput : public TextureBufferOutput
{
public:
	using TextureBufferOutput::TextureBufferOutput;

	void activate(PipelineContext &context) override;
};

/**
 * Renders the objects that are rigidly attached to the camera into a mask.
 *
 * Camera motion blur reconstructs each pixel's world position and reprojects it
 * through the previous frame's camera, which silently assumes every pixel is
 * fixed in the world. That is wrong for anything carried along by the camera:
 * the local player's own body, and whatever it is riding, are motionless on
 * screen yet get the largest smear of anything in the frame, because they sit
 * closest to the camera. This step marks those pixels so the blur can skip them.
 *
 * The set is the connected attachment component containing the local player:
 * walk up the parent chain to its root, then render that whole subtree. Anything
 * attached to the thing the player is riding rides along with it too.
 *
 * The scene depth buffer is shared rather than rebuilt, so parts of the player
 * hidden behind terrain in third person simply fail the depth test and stay out
 * of the mask, with no special casing.
 */
class CameraRigidMaskStep : public RenderStep
{
public:
	CameraRigidMaskStep(Client *client);

	void setRenderSource(RenderSource *source) override {}
	void setRenderTarget(RenderTarget *target) override { m_target = target; }
	void reset(PipelineContext &context) override {}
	void run(PipelineContext &context) override;

private:
	void renderObject(video::IVideoDriver *driver, GenericCAO *cao);

	Client *m_client;
	RenderTarget *m_target = nullptr;
	video::E_MATERIAL_TYPE m_mask_material = video::EMT_SOLID;
	/// Scratch space for the material swap, kept to avoid reallocating per frame.
	std::vector<video::E_MATERIAL_TYPE> m_saved_material_types;
};

RenderStep *addPostProcessing(RenderPipeline *pipeline, RenderStep *previousStep, v2f scale, Client *client);
