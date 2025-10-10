// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 x2048, Dmitry Kostenko <codeforsmile@gmail.com>
#pragma once

#include "irrlichttypes_bloated.h"
#include <IrrlichtDevice.h> // used in all render/*.cpp
#include <IVideoDriver.h> // used in all render/*.cpp

#include <vector>
#include <memory>
#include <string>

class RenderSource;
class RenderTarget;
class RenderStep;
class Client;
class Hud;
class ShadowRenderer;

namespace video
{
	class IRenderTarget;
}

struct PipelineContext
{
	PipelineContext(IrrlichtDevice *_device, Client *_client, Hud *_hud, ShadowRenderer *_shadow_renderer, video::SColor _color, v2u32 _target_size)
		: device(_device), client(_client), hud(_hud), shadow_renderer(_shadow_renderer), clear_color(_color), target_size(_target_size)
	{
	}

	IrrlichtDevice *device;
	Client *client;
	Hud *hud;
	ShadowRenderer *shadow_renderer;
	video::SColor clear_color;
	v2u32 target_size;

	bool show_hud {true};
	bool draw_wield_tool {true};
	bool draw_crosshair {true};
};

/**
 * Base object that can be owned by RenderPipeline
 *
 */
class RenderPipelineObject
{
public:
	virtual ~RenderPipelineObject() = default;
	virtual void reset(PipelineContext &context) {}
};

/**
 * Represents a source of rendering information such as textures
 */
class RenderSource : virtual public RenderPipelineObject
{
public:
	/**
	 * Return the number of textures in the source.
	 */
	virtual u8 getTextureCount() = 0;

	/**
	 * Get a texture by index.
	 * Returns nullptr is the texture does not exist.
	 */
	virtual video::ITexture *getTexture(u8 index) = 0;
};

/**
 *	Represents a render target (screen or framebuffer)
 */
class RenderTarget : virtual public RenderPipelineObject
{
public:
	/**
	 * Activate the render target and configure OpenGL state for the output.
	 * This is usually done by @see RenderStep implementations.
	 */
	virtual void activate(PipelineContext &context)
	{
		m_clear = false;
	}

	/**
	 * Resets the state of the object for the next pipeline iteration
	 */
	virtual void reset(PipelineContext &context) override
	{
		m_clear = true;
	}

protected:
	bool m_clear {true};
};

/**
 * Texture buffer represents a framebuffer with a multiple attached textures.
 *
 * @note Use of TextureBuffer requires use of gl_FragData[] in the shader
 */
class TextureBuffer : public RenderSource
{
public:
	virtual ~TextureBuffer() override;

	/**
	 * Configure fixed-size texture for the specific index
	 *
	 * @param index index of the texture
	 * @param size width and height of the texture in pixels
	 * @param height height of the texture in pixels
	 * @param name unique name of the texture
	 * @param format color format
	 */
	void setTexture(u8 index, core::dimension2du size, const std::string& name, video::ECOLOR_FORMAT format, bool clear = false, u8 msaa = 0);

	/**
	 * Configure relative-size texture for the specific index
	 *
	 * @param index index of the texture
	 * @param scale_factor relation of the texture dimensions to the screen dimensions
	 * @param name unique name of the texture
	 * @param format color format
	 */
	void setTexture(u8 index, v2f scale_factor, const std::string& name, video::ECOLOR_FORMAT format, bool clear = false, u8 msaa = 0);

	virtual u8 getTextureCount() override { return m_textures.size(); }
	virtual video::ITexture *getTexture(u8 index) override;
	virtual void reset(PipelineContext &context) override;
	void swapTextures(u8 texture_a, u8 texture_b);
private:
	static const u8 NO_DEPTH_TEXTURE = 255;

	struct TextureDefinition
	{
		bool valid { false };
		bool fixed_size { false };
		bool dirty { false };
		bool clear { false };
		v2f scale_factor;
		core::dimension2du size;
		std::string name;
		video::ECOLOR_FORMAT format;
		u8 msaa;
	};

	/**
	 * Make sure the texture in the given slot matches the texture definition given the current context.
	 * @param textureSlot address of the texture pointer to verify and populate.
	 * @param definition logical definition of the texture
	 * @param context current context of the rendering pipeline
	 * @return true if a new texture was created and put into the slot
	 * @return false if the slot was not modified
	 */
	bool ensureTexture(video::ITexture **textureSlot, const TextureDefinition& definition, PipelineContext &context);

	video::IVideoDriver *m_driver { nullptr };
	std::vector<TextureDefinition> m_definitions;
	core::array<video::ITexture *> m_textures;
};

/**
 * Targets output to designated texture in texture buffer
 */
class TextureBufferOutput : public RenderTarget
{
public:
	TextureBufferOutput(TextureBuffer *buffer, u8 texture_index);
	TextureBufferOutput(TextureBuffer *buffer, const std::vector<u8> &texture_map);
	TextureBufferOutput(TextureBuffer *buffer, const std::vector<u8> &texture_map, u8 depth_stencil);
	virtual ~TextureBufferOutput() override;
	void activate(PipelineContext &context) override;

	video::IRenderTarget *getIrrRenderTarget(PipelineContext &context);

private:
	static const u8 NO_DEPTH_TEXTURE = 255;

	TextureBuffer *buffer;
	std::vector<u8> texture_map;
	u8 depth_stencil { NO_DEPTH_TEXTURE };
	video::IRenderTarget* render_target { nullptr };
	video::IVideoDriver* driver { nullptr };
};

/**
 * Allows remapping texture indicies in another RenderSource.
 *
 * @note all unmapped indexes are passed through to the underlying render source.
 */
class RemappingSource : RenderSource
{
public:
	RemappingSource(RenderSource *source)
			: m_source(source)
	{}

	/**
	 * Maps texture index to a different index in the dependent source.
	 *
	 * @param index texture index as requested by the @see RenderStep.
	 * @param target_index matching texture index in the underlying @see RenderSource.
	 */
	void setMapping(u8 index, u8 target_index)
	{
		if (index >= m_mappings.size()) {
			u8 start = m_mappings.size();
			m_mappings.resize(index);
			for (u8 i = start; i < m_mappings.size(); ++i)
				m_mappings[i] = i;
		}

		m_mappings[index] = target_index;
	}

	virtual u8 getTextureCount() override
	{
		return m_mappings.size();
	}

	virtual video::ITexture *getTexture(u8 index) override
	{
		if (index < m_mappings.size())
			index = m_mappings[index];

		return m_source->getTexture(index);
	}
public:
	RenderSource *m_source;
	std::vector<u8> m_mappings;
};

class DynamicSource : public RenderSource
{
public:
	bool isConfigured() { return upstream != nullptr; }
	void setRenderSource(RenderSource *value) { upstream = value; }

	/**
	 * Return the number of textures in the source.
	 */
	virtual u8 getTextureCount() override;

	/**
	 * Get a texture by index.
	 * Returns nullptr is the texture does not exist.
	 */
	virtual video::ITexture *getTexture(u8 index) override;
private:
	RenderSource *upstream { nullptr };
};

/**
 * Implements direct output to screen framebuffer.
 */
class ScreenTarget : public RenderTarget
{
public:
	virtual void activate(PipelineContext &context) override;
	virtual void reset(PipelineContext &context) override;
private:
	core::dimension2du size;
};

class DynamicTarget : public RenderTarget
{
public:
	bool isConfigured() { return upstream != nullptr; }
	void setRenderTarget(RenderTarget *value) { upstream = value; }
	virtual void activate(PipelineContext &context) override;
private:
	RenderTarget *upstream { nullptr };
};

/**
 * Base class for rendering steps in the pipeline
 */
class RenderStep : virtual public RenderPipelineObject
{
public:
	/**
	 * Assigns render source to this step.
	 *
	 * @param source source of rendering information
	 */
	virtual void setRenderSource(RenderSource *source) = 0;

	/**
	 * Assigned render target to this step.
	 *
	 * @param target render target to send output to.
	 */
	virtual void setRenderTarget(RenderTarget *target) = 0;

	/**
	 * Runs the step. This method is invoked by the pipeline.
	 */
	virtual void run(PipelineContext &context) = 0;
};

/**
 * Provides default empty implementation of supporting methods in a rendering step.
 */
class TrivialRenderStep : public RenderStep
{
public:
	virtual void setRenderSource(RenderSource *source) override {}
	virtual void setRenderTarget(RenderTarget *target) override {}
	virtual void reset(PipelineContext &) override {}
};

/**
 * Dynamically changes render target of another step.
 *
 * This allows re-running parts of the pipeline with different outputs
 */
class SetRenderTargetStep : public TrivialRenderStep
{
public:
	SetRenderTargetStep(RenderStep *step, RenderTarget *target);
	virtual void run(PipelineContext &context) override;
private:
	RenderStep *step;
	RenderTarget *target;
};

/**
 * Swaps two textures in the texture buffer.
 *
 */
class SwapTexturesStep : public TrivialRenderStep
{
public:
	SwapTexturesStep(TextureBuffer *buffer, u8 texture_a, u8 texture_b);
	virtual void run(PipelineContext &context) override;
private:
	TextureBuffer *buffer;
	u8 texture_a;
	u8 texture_b;
};

/**
 * Render Pipeline provides a flexible way to execute rendering steps in the engine.
 *
 * RenderPipeline also implements @see RenderStep, allowing for nesting of the pipelines.
 */
class RenderPipeline : public RenderStep
{
public:
	/**
	 * Add a step to the end of the pipeline
	 *
	 * @param step reference to a @see RenderStep implementation.
	 */
	RenderStep *addStep(RenderStep *step)
	{
		m_pipeline.push_back(step);
		return step;
	}

	/**
	 * Capture ownership of a dynamically created @see RenderStep instance.
	 *
	 * RenderPipeline will delete the instance when the pipeline is destroyed.
	 *
	 * @param step reference to the instance.
	 * @return RenderStep* value of the 'step' parameter.
	 */
	template<typename T>
	T *own(std::unique_ptr<T> &&object)
	{
		T* result = object.release();
		m_objects.push_back(std::unique_ptr<RenderPipelineObject>(result));
		return result;
	}

	/**
	 * Create a new object that will be managed by the pipeline
	 *
	 * @tparam T type of the object to be created
	 * @tparam Args types of constructor arguments
	 * @param args constructor arguments
	 * @return T* pointer to the newly created object
	 */
	template<typename T, typename... Args>
	T *createOwned(Args&&... args) {
		return own(std::make_unique<T>(std::forward<Args>(args)...));
	}

	/**
	 * Create and add a step managed by the pipeline and return a pointer
	 * to the step for further configuration.
	 *
	 * @tparam T Type of the step to be added.
	 * @tparam Args Types of the constructor parameters
	 * @param args Constructor parameters
	 * @return RenderStep* Pointer to the created step for further configuration.
	 */
	template<typename T, typename... Args>
	T *addStep(Args&&... args) {
		T* result = own(std::make_unique<T>(std::forward<Args>(args)...));
		addStep(result);
		return result;
	}

	RenderSource *getInput();
	RenderTarget *getOutput();

	v2f getScale() { return scale; }
	void setScale(v2f value) { scale = value; }

	virtual void reset(PipelineContext &context) override {}
	virtual void run(PipelineContext &context) override;

	virtual void setRenderSource(RenderSource *source) override;
	virtual void setRenderTarget(RenderTarget *target) override;
private:
	std::vector<RenderStep *> m_pipeline;
	std::vector< std::unique_ptr<RenderPipelineObject> > m_objects;
	DynamicSource m_input;
	DynamicTarget m_output;
	v2f scale { 1.0f, 1.0f };
};
