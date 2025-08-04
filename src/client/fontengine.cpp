// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 sapier <sapier at gmx dot net>

#include "fontengine.h"

#include "client/renderingengine.h"
#include "settings.h"
#include "irrlicht_changes/CGUITTFont.h"
#include "util/numeric.h" // rangelim
#include <IGUIEnvironment.h>
#include <IGUIFont.h>

#include <cmath>
#include <cstring>
#include <unordered_set>

/** reference to access font engine, has to be initialized by main */
FontEngine *g_fontengine = nullptr;

void FontEngine::fontSettingChanged(const std::string &name, void *userdata)
{
	((FontEngine *)userdata)->m_needs_reload = true;
}

static const char *settings[] = {
	"font_size", "font_bold", "font_italic", "font_size_divisible_by",
	"mono_font_size", "mono_font_size_divisible_by",
	"font_shadow", "font_shadow_alpha",
	"font_path", "font_path_bold", "font_path_italic", "font_path_bold_italic",
	"mono_font_path", "mono_font_path_bold", "mono_font_path_italic",
	"mono_font_path_bold_italic",
	"fallback_font_path",
	"dpi_change_notifier", "display_density_factor", "gui_scaling",
};

FontEngine::FontEngine(gui::IGUIEnvironment* env) :
	m_env(env)
{
	for (u32 &i : m_default_size) {
		i = FONT_SIZE_UNSPECIFIED;
	}

	assert(g_settings != NULL); // pre-condition
	assert(m_env != NULL); // pre-condition
	assert(m_env->getSkin() != NULL); // pre-condition

	readSettings();

	for (auto name : settings)
		g_settings->registerChangedCallback(name, fontSettingChanged, this);
}

FontEngine::~FontEngine()
{
	g_settings->deregisterAllChangedCallbacks(this);

	clearCache();
}

void FontEngine::clearCache()
{
	RecursiveMutexAutoLock l(m_font_mutex);

	for (auto &font_cache_it : m_font_cache) {

		for (auto &font_it : font_cache_it) {
			font_it.second->drop();
			font_it.second = nullptr;
		}
		font_cache_it.clear();
	}
}

gui::IGUIFont *FontEngine::getFont(FontSpec spec)
{
	return getFont(spec, false);
}

gui::IGUIFont *FontEngine::getFont(FontSpec spec, bool may_fail)
{
	if (spec.mode == FM_Unspecified) {
		spec.mode = s_default_font_mode;
	} else if (spec.mode == _FM_Fallback) {
		// Fallback font doesn't support these
		spec.bold = false;
		spec.italic = false;
	}

	// Fallback to default size
	if (spec.size == FONT_SIZE_UNSPECIFIED)
		spec.size = m_default_size[spec.mode];

	RecursiveMutexAutoLock l(m_font_mutex);

	const auto &cache = m_font_cache[spec.getHash()];
	auto it = cache.find(spec.size);
	if (it != cache.end())
		return it->second;

	// Font does not yet exist
	gui::IGUIFont *font = initFont(spec);

	if (!font && !may_fail) {
		errorstream << "Minetest cannot continue without a valid font. "
			"Please correct the 'font_path' setting or install the font "
			"file in the proper location." << std::endl;
		abort();
	}

	m_font_cache[spec.getHash()][spec.size] = font;

	return font;
}

unsigned int FontEngine::getTextHeight(const FontSpec &spec)
{
	gui::IGUIFont *font = getFont(spec);

	return font->getDimension(L"Some unimportant example String").Height;
}

unsigned int FontEngine::getTextWidth(const std::wstring &text, const FontSpec &spec)
{
	gui::IGUIFont *font = getFont(spec);

	return font->getDimension(text.c_str()).Width;
}

/** get line height for a specific font (including empty room between lines) */
unsigned int FontEngine::getLineHeight(const FontSpec &spec)
{
	gui::IGUIFont *font = getFont(spec);

	return font->getDimension(L"Some unimportant example String").Height
			+ font->getKerning(L'S').Y;
}

unsigned int FontEngine::getDefaultFontSize()
{
	return m_default_size[s_default_font_mode];
}

unsigned int FontEngine::getFontSize(FontMode mode)
{
	if (mode == FM_Unspecified)
		return m_default_size[FM_Standard];

	return m_default_size[mode];
}

void FontEngine::readSettings()
{
	m_default_size[FM_Standard]  = rangelim(g_settings->getU16("font_size"), 5, 72);
	m_default_size[_FM_Fallback] = m_default_size[FM_Standard];
	m_default_size[FM_Mono]      = rangelim(g_settings->getU16("mono_font_size"), 5, 72);

	m_default_bold = g_settings->getBool("font_bold");
	m_default_italic = g_settings->getBool("font_italic");

	refresh();
}

void FontEngine::handleReload()
{
	if (!m_needs_reload)
		return;

	m_needs_reload = false;
	readSettings();
}

void FontEngine::updateSkin()
{
	gui::IGUIFont *font = getFont();
	assert(font);

	m_env->getSkin()->setFont(font);
}

void FontEngine::updateCache()
{
	/* the only font to be initialized is default one,
	 * all others are re-initialized on demand */
	getFont(FONT_SIZE_UNSPECIFIED, FM_Unspecified);
}

void FontEngine::refresh() {
	clearCache();
	updateCache();
	updateSkin();
}

void FontEngine::setMediaFont(const std::string &name, const std::string &data)
{
	static std::unordered_set<std::string> valid_names {
		"regular", "bold", "italic", "bold_italic",
		"mono", "mono_bold", "mono_italic", "mono_bold_italic",
	};
	if (!valid_names.count(name)) {
		warningstream << "Ignoring unrecognized media font: " << name << std::endl;
		return;
	}

	constexpr char TTF_MAGIC[5] = {0, 1, 0, 0, 0};
	if (data.size() < 5 || (memcmp(data.data(), "wOFF", 4) &&
			memcmp(data.data(), TTF_MAGIC, 5))) {
		warningstream << "Rejecting media font with unrecognized magic" << std::endl;
		return;
	}

	std::string copy = data;
	irr_ptr<gui::SGUITTFace> face(gui::SGUITTFace::createFace(std::move(copy)));
	m_media_faces.emplace(name, face);
	refresh();
}

void FontEngine::clearMediaFonts()
{
	RecursiveMutexAutoLock l(m_font_mutex);
	m_media_faces.clear();
	refresh();
}

gui::IGUIFont *FontEngine::initFont(FontSpec spec)
{
	assert(spec.mode != FM_Unspecified);
	assert(spec.size != FONT_SIZE_UNSPECIFIED);

	if (spec.mode == _FM_Fallback)
		spec.allow_server_media = false;

	std::string setting_prefix = "";
	if (spec.mode == FM_Mono)
		setting_prefix = "mono_";

	std::string setting_suffix = "";
	if (spec.bold)
		setting_suffix.append("_bold");
	if (spec.italic)
		setting_suffix.append("_italic");

	// Font size in pixels for FreeType
	u32 size = rangelim(spec.size * RenderingEngine::getDisplayDensity() *
			g_settings->getFloat("gui_scaling"), 1U, 500U);

	// Constrain the font size to a certain multiple, if necessary
	u16 divisible_by = g_settings->getU16(setting_prefix + "font_size_divisible_by");
	if (divisible_by > 1) {
		size = std::max<u32>(
				std::round((float)size / divisible_by) * divisible_by, divisible_by);
	}

	sanity_check(size != 0);

	u16 font_shadow       = 0;
	u16 font_shadow_alpha = 0;
	g_settings->getU16NoEx(setting_prefix + "font_shadow", font_shadow);
	g_settings->getU16NoEx(setting_prefix + "font_shadow_alpha",
			font_shadow_alpha);

	auto createFont = [&](gui::SGUITTFace *face) -> gui::CGUITTFont* {
		auto *font = gui::CGUITTFont::createTTFont(m_env,
				face, size, true, true, font_shadow,
				font_shadow_alpha);

		if (!font)
			return nullptr;

		if (spec.mode != _FM_Fallback) {
			FontSpec spec2(spec);
			spec2.mode = _FM_Fallback;
			font->setFallback(getFont(spec2, true));
		}

		return font;
	};

	// Use the server-provided font media (if available)
	if (spec.allow_server_media) {
		std::string media_name = spec.mode == FM_Mono
				? "mono" + setting_suffix
				: (setting_suffix.empty() ? "" : setting_suffix.substr(1));
		if (media_name.empty())
			media_name = "regular";

		auto it = m_media_faces.find(media_name);
		if (it != m_media_faces.end()) {
			auto *face = it->second.get();
			if (auto *font = createFont(face))
				return font;
			errorstream << "FontEngine: Cannot load media font '" << media_name <<
				"'. Falling back to client settings." << std::endl;
		}
	}

	// Use the local font files specified by the settings
	std::string path_setting;
	if (spec.mode == _FM_Fallback)
		path_setting = "fallback_font_path";
	else
		path_setting = setting_prefix + "font_path" + setting_suffix;

	std::string fallback_settings[] = {
		g_settings->get(path_setting),
		Settings::getLayer(SL_DEFAULTS)->get(path_setting)
	};
	for (const std::string &font_path : fallback_settings) {
		infostream << "Creating new font: " << font_path.c_str()
				<< " " << size << "pt" << std::endl;

		// Grab the face.
		if (auto *face = gui::SGUITTFace::loadFace(font_path)) {
			auto *font = createFont(face);
			face->drop();
			return font;
		}

		errorstream << "FontEngine: Cannot load '" << font_path <<
			"'. Trying to fall back to another path." << std::endl;
	}
	return nullptr;
}
