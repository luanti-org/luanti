// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013 Kahrl <kahrl@gmx.net>

#include <fstream>
#include <iterator>
#include "shader.h"
#include "irr_ptr.h"
#include "debug.h"
#include "filesys.h"
#include "util/container.h"
#include "util/thread.h"
#include "settings.h"
#include <ICameraSceneNode.h>
#include <IGPUProgrammingServices.h>
#include <IMaterialRenderer.h>
#include <IMaterialRendererServices.h>
#include <IShaderConstantSetCallBack.h>
#include "client/renderingengine.h"
#include "gettext.h"
#include "log.h"
#include "gamedef.h"
#include "client/tile.h"
#include "config.h"

#include <mt_opengl.h>

/*
	A cache from shader name to shader path
*/
static MutexedMap<std::string, std::string> g_shadername_to_path_cache;

/*
	Gets the path to a shader by first checking if the file
	  name_of_shader/filename
	exists in shader_path and if not, using the data path.

	If not found, returns "".

	Utilizes a thread-safe cache.
*/
std::string getShaderPath(const std::string &name_of_shader,
		const std::string &filename)
{
	std::string combined = name_of_shader + DIR_DELIM + filename;
	std::string fullpath;
	/*
		Check from cache
	*/
	bool incache = g_shadername_to_path_cache.get(combined, &fullpath);
	if(incache)
		return fullpath;

	/*
		Check from shader_path
	*/
	std::string shader_path = g_settings->get("shader_path");
	if (!shader_path.empty()) {
		std::string testpath = shader_path + DIR_DELIM + combined;
		if(fs::PathExists(testpath))
			fullpath = testpath;
	}

	/*
		Check from default data directory
	*/
	if (fullpath.empty()) {
		std::string rel_path = std::string("client") + DIR_DELIM
				+ "shaders" + DIR_DELIM
				+ name_of_shader + DIR_DELIM
				+ filename;
		std::string testpath = porting::path_share + DIR_DELIM + rel_path;
		if(fs::PathExists(testpath))
			fullpath = testpath;
	}

	// Add to cache (also an empty result is cached)
	g_shadername_to_path_cache.set(combined, fullpath);

	// Finally return it
	return fullpath;
}

/*
	SourceShaderCache: A cache used for storing source shaders.
*/

class SourceShaderCache
{
public:
	void insert(const std::string &name_of_shader, const std::string &filename,
		const std::string &program, bool prefer_local)
	{
		std::string combined = name_of_shader + DIR_DELIM + filename;
		// Try to use local shader instead if asked to
		if(prefer_local){
			std::string path = getShaderPath(name_of_shader, filename);
			if(!path.empty()){
				std::string p = readFile(path);
				if (!p.empty()) {
					m_programs[combined] = p;
					return;
				}
			}
		}
		m_programs[combined] = program;
	}

	std::string get(const std::string &name_of_shader,
		const std::string &filename)
	{
		std::string combined = name_of_shader + DIR_DELIM + filename;
		StringMap::iterator n = m_programs.find(combined);
		if (n != m_programs.end())
			return n->second;
		return "";
	}

	// Primarily fetches from cache, secondarily tries to read from filesystem
	std::string getOrLoad(const std::string &name_of_shader,
		const std::string &filename)
	{
		std::string combined = name_of_shader + DIR_DELIM + filename;
		StringMap::iterator n = m_programs.find(combined);
		if (n != m_programs.end())
			return n->second;
		std::string path = getShaderPath(name_of_shader, filename);
		if (path.empty()) {
			infostream << "SourceShaderCache::getOrLoad(): No path found for \""
				<< combined << "\"" << std::endl;
			return "";
		}
		infostream << "SourceShaderCache::getOrLoad(): Loading path \""
			<< path << "\"" << std::endl;
		std::string p = readFile(path);
		if (!p.empty()) {
			m_programs[combined] = p;
			return p;
		}
		return "";
	}
private:
	StringMap m_programs;

	inline std::string readFile(const std::string &path)
	{
		std::string ret;
		if (!fs::ReadFile(path, ret, true))
			ret.clear();
		return ret;
	}
};


/*
	ShaderCallback: Sets constants that can be used in shaders
*/

class ShaderCallback : public video::IShaderConstantSetCallBack
{
	std::vector<std::unique_ptr<IShaderUniformSetter>> m_setters;

public:
	template <typename Factories>
	ShaderCallback(const Factories &factories)
	{
		for (auto &&factory : factories) {
			auto *setter = factory->create();
			if (setter)
				m_setters.emplace_back(setter);
		}
	}

	virtual void OnSetConstants(video::IMaterialRendererServices *services, s32 userData) override
	{
		for (auto &&setter : m_setters)
			setter->onSetUniforms(services);
	}

	virtual void OnSetMaterial(const video::SMaterial& material) override
	{
		for (auto &&setter : m_setters)
			setter->onSetMaterial(material);
	}
};


/*
	MainShaderConstantSetter: Sets some random general constants
*/

class MainShaderConstantSetter : public IShaderConstantSetter
{
public:
	MainShaderConstantSetter() = default;
	~MainShaderConstantSetter() = default;

	void onGenerate(const std::string &name, ShaderConstants &constants) override
	{
		constants["ENABLE_TONE_MAPPING"] = g_settings->getBool("tone_mapping") ? 1 : 0;

		if (g_settings->getBool("enable_dynamic_shadows")) {
			constants["ENABLE_DYNAMIC_SHADOWS"] = 1;
			if (g_settings->getBool("shadow_map_color"))
				constants["COLORED_SHADOWS"] = 1;

			if (g_settings->getBool("shadow_poisson_filter"))
				constants["POISSON_FILTER"] = 1;

			if (g_settings->getBool("enable_water_reflections"))
				constants["ENABLE_WATER_REFLECTIONS"] = 1;

			if (g_settings->getBool("enable_translucent_foliage"))
				constants["ENABLE_TRANSLUCENT_FOLIAGE"] = 1;

			// FIXME: The node specular effect is currently disabled due to mixed in-game
			// results. This shader should not be applied to all nodes equally. See #15898
			if (false)
				constants["ENABLE_NODE_SPECULAR"] = 1;

			s32 shadow_filter = g_settings->getS32("shadow_filters");
			constants["SHADOW_FILTER"] = shadow_filter;

			float shadow_soft_radius = std::max(1.f,
				g_settings->getFloat("shadow_soft_radius"));
			constants["SOFTSHADOWRADIUS"] = shadow_soft_radius;
		}

		if (g_settings->getBool("enable_bloom")) {
			constants["ENABLE_BLOOM"] = 1;
			if (g_settings->getBool("enable_bloom_debug"))
				constants["ENABLE_BLOOM_DEBUG"] = 1;
		}

		if (g_settings->getBool("enable_auto_exposure"))
			constants["ENABLE_AUTO_EXPOSURE"] = 1;

		if (g_settings->get("antialiasing") == "ssaa") {
			constants["ENABLE_SSAA"] = 1;
			u16 ssaa_scale = std::max<u16>(2, g_settings->getU16("fsaa"));
			constants["SSAA_SCALE"] = ssaa_scale;
		}

		if (g_settings->getBool("debanding"))
			constants["ENABLE_DITHERING"] = 1;

		if (g_settings->getBool("enable_volumetric_lighting"))
			constants["VOLUMETRIC_LIGHT"] = 1;
	}
};


/*
	MainShaderUniformSetter: Set basic uniforms required for almost everything
*/

class MainShaderUniformSetter : public IShaderUniformSetter
{
	using SamplerLayer_t = s32;

	CachedVertexShaderSetting<f32, 16> m_world_view_proj{"mWorldViewProj"};
	CachedVertexShaderSetting<f32, 16> m_world{"mWorld"};

	// Modelview matrix
	CachedVertexShaderSetting<float, 16> m_world_view{"mWorldView"};
	// Texture matrix
	CachedVertexShaderSetting<float, 16> m_texture{"mTexture"};

	CachedPixelShaderSetting<SamplerLayer_t> m_texture0{"texture0"};
	CachedPixelShaderSetting<SamplerLayer_t> m_texture1{"texture1"};
	CachedPixelShaderSetting<SamplerLayer_t> m_texture2{"texture2"};
	CachedPixelShaderSetting<SamplerLayer_t> m_texture3{"texture3"};

	// commonly used way to pass material color to shader
	video::SColor m_material_color;
	CachedPixelShaderSetting<float, 4> m_material_color_setting{"materialColor"};

public:
	~MainShaderUniformSetter() = default;

	virtual void onSetMaterial(const video::SMaterial& material) override
	{
		m_material_color = material.ColorParam;
	}

	virtual void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		video::IVideoDriver *driver = services->getVideoDriver();
		assert(driver);

		// Set world matrix
		core::matrix4 world = driver->getTransform(video::ETS_WORLD);
		m_world.set(world, services);

		// Set clip matrix
		core::matrix4 worldView;
		worldView = driver->getTransform(video::ETS_VIEW);
		worldView *= world;

		core::matrix4 worldViewProj;
		worldViewProj = driver->getTransform(video::ETS_PROJECTION);
		worldViewProj *= worldView;
		m_world_view_proj.set(worldViewProj, services);

		if (driver->getDriverType() == video::EDT_OGLES2 || driver->getDriverType() == video::EDT_OPENGL3) {
			auto &texture = driver->getTransform(video::ETS_TEXTURE_0);
			m_world_view.set(worldView, services);
			m_texture.set(texture, services);
		}

		SamplerLayer_t tex_id;
		tex_id = 0;
		m_texture0.set(&tex_id, services);
		tex_id = 1;
		m_texture1.set(&tex_id, services);
		tex_id = 2;
		m_texture2.set(&tex_id, services);
		tex_id = 3;
		m_texture3.set(&tex_id, services);

		video::SColorf colorf(m_material_color);
		m_material_color_setting.set(colorf, services);
	}
};


class MainShaderUniformSetterFactory : public IShaderUniformSetterFactory
{
public:
	virtual IShaderUniformSetter* create()
		{ return new MainShaderUniformSetter(); }
};


/*
	ShaderSource
*/

class ShaderSource : public IWritableShaderSource
{
public:
	ShaderSource();
	~ShaderSource() override;

	/*
		- If shader material is found from cache, return the cached id.
		- Otherwise generate the shader material, add to cache and return id.

		The id 0 points to a null shader. Its material is EMT_SOLID.
	*/
	u32 getShaderIdDirect(const std::string &name, const ShaderConstants &input_const,
		video::E_MATERIAL_TYPE base_mat);

	/*
		If shader specified by the name pointed by the id doesn't
		exist, create it, then return id.

		Can be called from any thread. If called from some other thread
		and not found in cache, the call is queued to the main thread
		for processing.
	*/
	u32 getShader(const std::string &name, const ShaderConstants &input_const,
		video::E_MATERIAL_TYPE base_mat) override;

	const ShaderInfo &getShaderInfo(u32 id) override;

	// Processes queued shader requests from other threads.
	// Shall be called from the main thread.
	void processQueue() override;

	// Insert a shader program into the cache without touching the
	// filesystem. Shall be called from the main thread.
	void insertSourceShader(const std::string &name_of_shader,
		const std::string &filename, const std::string &program) override;

	// Rebuild shaders from the current set of source shaders
	// Shall be called from the main thread.
	void rebuildShaders() override;

	void addShaderConstantSetter(std::unique_ptr<IShaderConstantSetter> setter) override
	{
		m_constant_setters.emplace_back(std::move(setter));
	}

	void addShaderUniformSetterFactory(std::unique_ptr<IShaderUniformSetterFactory> setter) override
	{
		m_uniform_factories.emplace_back(std::move(setter));
	}

private:

	// The id of the thread that is allowed to use irrlicht directly
	std::thread::id m_main_thread;

	// Cache of source shaders
	// This should be only accessed from the main thread
	SourceShaderCache m_sourcecache;

	// A shader id is index in this array.
	// The first position contains a dummy shader.
	std::vector<ShaderInfo> m_shaderinfo_cache;
	// The former container is behind this mutex
	std::mutex m_shaderinfo_cache_mutex;

#if 0
	// Queued shader fetches (to be processed by the main thread)
	RequestQueue<std::string, u32, u8, u8> m_get_shader_queue;
#endif

	// Global constant setter factories
	std::vector<std::unique_ptr<IShaderConstantSetter>> m_constant_setters;

	// Global uniform setter factories
	std::vector<std::unique_ptr<IShaderUniformSetterFactory>> m_uniform_factories;

	// Generate shader given the shader name.
	ShaderInfo generateShader(const std::string &name,
		const ShaderConstants &input_const, video::E_MATERIAL_TYPE base_mat);

	/// @brief outputs a constant to an ostream
	inline void putConstant(std::ostream &os, const ShaderConstants::mapped_type &it)
	{
		if (auto *ival = std::get_if<int>(&it); ival)
			os << *ival;
		else
			os << std::get<float>(it);
	}
};

IWritableShaderSource *createShaderSource()
{
	return new ShaderSource();
}

ShaderSource::ShaderSource()
{
	m_main_thread = std::this_thread::get_id();

	// Add a dummy ShaderInfo as the first index, named ""
	m_shaderinfo_cache.emplace_back();

	// Add global stuff
	addShaderConstantSetter(std::make_unique<MainShaderConstantSetter>());
	addShaderUniformSetterFactory(std::make_unique<MainShaderUniformSetterFactory>());
}

ShaderSource::~ShaderSource()
{
	MutexAutoLock lock(m_shaderinfo_cache_mutex);

	// Delete materials
	auto *gpu = RenderingEngine::get_video_driver()->getGPUProgrammingServices();
	assert(gpu);
	u32 n = 0;
	for (ShaderInfo &i : m_shaderinfo_cache) {
		if (!i.name.empty()) {
			gpu->deleteShaderMaterial(i.material);
			n++;
		}
	}
	m_shaderinfo_cache.clear();

	infostream << "~ShaderSource() cleaned up " << n << " materials" << std::endl;
}

u32 ShaderSource::getShader(const std::string &name,
	const ShaderConstants &input_const, video::E_MATERIAL_TYPE base_mat)
{
	/*
		Get shader
	*/

	if (std::this_thread::get_id() == m_main_thread) {
		return getShaderIdDirect(name, input_const, base_mat);
	}

	errorstream << "ShaderSource::getShader(): getting from "
		"other thread not implemented" << std::endl;

#if 0
	// We're gonna ask the result to be put into here

	static ResultQueue<std::string, u32, u8, u8> result_queue;

	// Throw a request in
	m_get_shader_queue.add(name, 0, 0, &result_queue);

	/* infostream<<"Waiting for shader from main thread, name=\""
			<<name<<"\""<<std::endl;*/

	while(true) {
		GetResult<std::string, u32, u8, u8>
			result = result_queue.pop_frontNoEx();

		if (result.key == name) {
			return result.item;
		}

		errorstream << "Got shader with invalid name: " << result.key << std::endl;
	}

	infostream << "getShader(): Failed" << std::endl;
#endif

	return 0;
}

/*
	This method generates all the shaders
*/
u32 ShaderSource::getShaderIdDirect(const std::string &name,
	const ShaderConstants &input_const, video::E_MATERIAL_TYPE base_mat)
{
	// Empty name means shader 0
	if (name.empty()) {
		infostream<<"getShaderIdDirect(): name is empty"<<std::endl;
		return 0;
	}

	// Check if already have such instance
	for (u32 i = 0; i < m_shaderinfo_cache.size(); i++) {
		auto &info = m_shaderinfo_cache[i];
		if (info.name == name && info.base_material == base_mat &&
			info.input_constants == input_const)
			return i;
	}

	/*
		Calling only allowed from main thread
	*/
	if (std::this_thread::get_id() != m_main_thread) {
		errorstream<<"ShaderSource::getShaderIdDirect() "
				"called not from main thread"<<std::endl;
		return 0;
	}

	ShaderInfo info = generateShader(name, input_const, base_mat);

	/*
		Add shader to caches (add dummy shaders too)
	*/

	MutexAutoLock lock(m_shaderinfo_cache_mutex);

	u32 id = m_shaderinfo_cache.size();
	m_shaderinfo_cache.push_back(std::move(info));
	return id;
}


const ShaderInfo &ShaderSource::getShaderInfo(u32 id)
{
	MutexAutoLock lock(m_shaderinfo_cache_mutex);

	if (id >= m_shaderinfo_cache.size()) {
		static ShaderInfo empty;
		return empty;
	}
	return m_shaderinfo_cache[id];
}

void ShaderSource::processQueue()
{


}

void ShaderSource::insertSourceShader(const std::string &name_of_shader,
		const std::string &filename, const std::string &program)
{
	/*infostream<<"ShaderSource::insertSourceShader(): "
			"name_of_shader=\""<<name_of_shader<<"\", "
			"filename=\""<<filename<<"\""<<std::endl;*/

	sanity_check(std::this_thread::get_id() == m_main_thread);

	m_sourcecache.insert(name_of_shader, filename, program, true);
}

void ShaderSource::rebuildShaders()
{
	MutexAutoLock lock(m_shaderinfo_cache_mutex);

	// Delete materials
	auto *gpu = RenderingEngine::get_video_driver()->getGPUProgrammingServices();
	assert(gpu);
	for (ShaderInfo &i : m_shaderinfo_cache) {
		if (!i.name.empty()) {
			gpu->deleteShaderMaterial(i.material);
			i.material = video::EMT_SOLID; // invalidate
		}
	}

	infostream << "ShaderSource: recreating " << m_shaderinfo_cache.size()
			<< " shaders" << std::endl;

	// Recreate shaders
	for (ShaderInfo &i : m_shaderinfo_cache) {
		ShaderInfo *info = &i;
		if (!info->name.empty()) {
			*info = generateShader(info->name, info->input_constants, info->base_material);
		}
	}
}


ShaderInfo ShaderSource::generateShader(const std::string &name,
	const ShaderConstants &input_const, video::E_MATERIAL_TYPE base_mat)
{
	ShaderInfo shaderinfo;
	shaderinfo.name = name;
	shaderinfo.input_constants = input_const;
	// fixed pipeline materials don't make sense here
	assert(base_mat != video::EMT_TRANSPARENT_VERTEX_ALPHA && base_mat != video::EMT_ONETEXTURE_BLEND);
	shaderinfo.base_material = base_mat;
	shaderinfo.material = shaderinfo.base_material;

	auto *driver = RenderingEngine::get_video_driver();
	// The null driver doesn't support shaders (duh), but we can pretend it does.
	if (driver->getDriverType() == video::EDT_NULL)
		return shaderinfo;

	auto *gpu = driver->getGPUProgrammingServices();
	if (!driver->queryFeature(video::EVDF_ARB_GLSL) || !gpu) {
		throw ShaderException(gettext("GLSL is not supported by the driver"));
	}

	// Create shaders header
	bool fully_programmable = driver->getDriverType() == video::EDT_OGLES2 || driver->getDriverType() == video::EDT_OPENGL3;
	std::ostringstream shaders_header;
	shaders_header
		<< std::noboolalpha
		<< std::showpoint // for GLSL ES
		;
	std::string vertex_header, fragment_header, geometry_header;
	if (fully_programmable) {
		if (driver->getDriverType() == video::EDT_OPENGL3) {
			shaders_header << "#version 150\n";
		} else {
			shaders_header << "#version 100\n";
		}
		// cf. EVertexAttributes.h for the predefined ones
		vertex_header = R"(
			precision mediump float;

			uniform highp mat4 mWorldView;
			uniform highp mat4 mWorldViewProj;
			uniform mediump mat4 mTexture;

			attribute highp vec4 inVertexPosition;
			attribute lowp vec4 inVertexColor;
			attribute mediump vec2 inTexCoord0;
			attribute mediump vec3 inVertexNormal;
			attribute mediump vec4 inVertexTangent;
			attribute mediump vec4 inVertexBinormal;
		)";
		// Our vertex color has components reversed compared to what OpenGL
		// normally expects, so we need to take that into account.
		vertex_header += "#define inVertexColor (inVertexColor.bgra)\n";
		fragment_header = R"(
			precision mediump float;
		)";
	} else {
		/* legacy OpenGL driver */
		shaders_header << R"(
			#version 120
			#define lowp
			#define mediump
			#define highp
		)";
		vertex_header = R"(
			#define mWorldView gl_ModelViewMatrix
			#define mWorldViewProj gl_ModelViewProjectionMatrix
			#define mTexture (gl_TextureMatrix[0])

			#define inVertexPosition gl_Vertex
			#define inVertexColor gl_Color
			#define inTexCoord0 gl_MultiTexCoord0
			#define inVertexNormal gl_Normal
			#define inVertexTangent gl_MultiTexCoord1
			#define inVertexBinormal gl_MultiTexCoord2
		)";
	}

	// map legacy semantic texture names to texture identifiers
	fragment_header += R"(
		#define baseTexture texture0
		#define normalTexture texture1
		#define textureFlags texture2
	)";

	/// Unique name of this shader, for debug/logging
	std::string log_name = name;
	for (auto &it : input_const) {
		if (log_name.size() > 60) { // it shouldn't be too long
			log_name.append("...");
			break;
		}
		std::ostringstream oss;
		putConstant(oss, it.second);
		log_name.append(" ").append(it.first).append("=").append(oss.str());
	}

	ShaderConstants constants = input_const;

	bool use_discard = fully_programmable;
	if (!use_discard) {
		// workaround for a certain OpenGL implementation lacking GL_ALPHA_TEST
		const char *renderer = reinterpret_cast<const char*>(GL.GetString(GL.RENDERER));
		if (strstr(renderer, "GC7000"))
			use_discard = true;
	}
	if (use_discard) {
		if (shaderinfo.base_material == video::EMT_TRANSPARENT_ALPHA_CHANNEL)
			constants["USE_DISCARD"] = 1;
		else if (shaderinfo.base_material == video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF)
			constants["USE_DISCARD_REF"] = 1;
	}

	/* Let the constant setters do their job and emit constants */
	for (auto &setter : m_constant_setters) {
		setter->onGenerate(name, constants);
	}

	for (auto &it : constants) {
		// spaces could cause duplicates
		assert(trim(it.first) == it.first);
		shaders_header << "#define " << it.first << ' ';
		putConstant(shaders_header, it.second);
		shaders_header << '\n';
	}

	std::string common_header = shaders_header.str();
	const char *final_header = "#line 0\n"; // reset the line counter for meaningful diagnostics

	std::string vertex_shader = m_sourcecache.getOrLoad(name, "opengl_vertex.glsl");
	std::string fragment_shader = m_sourcecache.getOrLoad(name, "opengl_fragment.glsl");
	std::string geometry_shader = m_sourcecache.getOrLoad(name, "opengl_geometry.glsl");

	vertex_shader = common_header + vertex_header + final_header + vertex_shader;
	fragment_shader = common_header + fragment_header + final_header + fragment_shader;
	const char *geometry_shader_ptr = nullptr; // optional
	if (!geometry_shader.empty()) {
		geometry_shader = common_header + geometry_header + final_header + geometry_shader;
		geometry_shader_ptr = geometry_shader.c_str();
	}

	auto cb = make_irr<ShaderCallback>(m_uniform_factories);
	infostream << "Compiling high level shaders for " << log_name << std::endl;
	s32 shadermat = gpu->addHighLevelShaderMaterial(
		vertex_shader.c_str(), fragment_shader.c_str(), geometry_shader_ptr,
		log_name.c_str(), scene::EPT_TRIANGLES, scene::EPT_TRIANGLES, 0,
		cb.get(), shaderinfo.base_material);
	if (shadermat == -1) {
		errorstream << "generateShader(): failed to generate shaders for "
			<< log_name << ", addHighLevelShaderMaterial failed." << std::endl;
		dumpShaderProgram(warningstream, "Vertex", vertex_shader);
		dumpShaderProgram(warningstream, "Fragment", fragment_shader);
		dumpShaderProgram(warningstream, "Geometry", geometry_shader);
		throw ShaderException(
			fmtgettext("Failed to compile the \"%s\" shader.", log_name.c_str()) +
			strgettext("\nCheck debug.txt for details."));
	}

	// Apply the newly created material type
	shaderinfo.material = (video::E_MATERIAL_TYPE) shadermat;
	return shaderinfo;
}

/*
	Other functions and helpers
*/

u32 IShaderSource::getShader(const std::string &name,
	MaterialType material_type, NodeDrawType drawtype)
{
	ShaderConstants input_const;
	input_const["MATERIAL_TYPE"] = (int)material_type;
	input_const["DRAWTYPE"] = (int)drawtype;

	video::E_MATERIAL_TYPE base_mat = video::EMT_SOLID;
	switch (material_type) {
		case TILE_MATERIAL_ALPHA:
		case TILE_MATERIAL_PLAIN_ALPHA:
		case TILE_MATERIAL_LIQUID_TRANSPARENT:
		case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			base_mat = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			break;
		case TILE_MATERIAL_BASIC:
		case TILE_MATERIAL_PLAIN:
		case TILE_MATERIAL_WAVING_LEAVES:
		case TILE_MATERIAL_WAVING_PLANTS:
		case TILE_MATERIAL_WAVING_LIQUID_BASIC:
			base_mat = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			break;
		default:
			break;
	}

	return getShader(name, input_const, base_mat);
}

void dumpShaderProgram(std::ostream &output_stream,
		const std::string &program_type, std::string_view program)
{
	output_stream << program_type << " shader program:" << std::endl <<
		"----------------------------------" << std::endl;
	size_t pos = 0;
	size_t prev = 0;
	s16 line = 1;
	while ((pos = program.find('\n', prev)) != std::string::npos) {
		output_stream << line++ << ": "<< program.substr(prev, pos - prev) <<
			std::endl;
		prev = pos + 1;
	}
	output_stream << line << ": " << program.substr(prev) << std::endl <<
		"End of " << program_type << " shader program." << std::endl <<
		" " << std::endl;
}
