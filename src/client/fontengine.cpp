// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 sapier <sapier at gmx dot net>

#include "fontengine.h"
#include <cmath>
#include "client/renderingengine.h"
#include "config.h"
#include "porting.h"
#include "filesys.h"
#include "gettext.h"
#include "settings.h"
#include "irrlicht_changes/CGUITTFont.h"
#include "util/numeric.h" // rangelim
#include <IGUIEnvironment.h>
#include <IGUIFont.h>

/** reference to access font engine, has to be initialized by main */
FontEngine *g_fontengine = nullptr;

/** callback to be used on change of font size setting */
static void font_setting_changed(const std::string &name, void *userdata)
{
	static_cast<FontEngine *>(userdata)->readSettings();
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

/******************************************************************************/
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
		g_settings->registerChangedCallback(name, font_setting_changed, this);
}

/******************************************************************************/
FontEngine::~FontEngine()
{
	g_settings->deregisterAllChangedCallbacks(this);

	cleanCache();
}

/******************************************************************************/
void FontEngine::cleanCache()
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

/******************************************************************************/
irr::gui::IGUIFont *FontEngine::getFont(FontSpec spec)
{
	return getFont(spec, false);
}

irr::gui::IGUIFont *FontEngine::getFont(FontSpec spec, bool may_fail)
{
	if (spec.mode == FM_Unspecified) {
		spec.mode = m_currentMode;
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

/******************************************************************************/
unsigned int FontEngine::getTextHeight(const FontSpec &spec)
{
	gui::IGUIFont *font = getFont(spec);

	return font->getDimension(L"Some unimportant example String").Height;
}

/******************************************************************************/
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
			+ font->getKerningHeight();
}

/******************************************************************************/
unsigned int FontEngine::getDefaultFontSize()
{
	return m_default_size[m_currentMode];
}

unsigned int FontEngine::getFontSize(FontMode mode)
{
	if (mode == FM_Unspecified)
		return m_default_size[FM_Standard];

	return m_default_size[mode];
}

/******************************************************************************/
void FontEngine::readSettings()
{
	m_default_size[FM_Standard]  = rangelim(g_settings->getU16("font_size"), 5, 72);
	m_default_size[_FM_Fallback] = m_default_size[FM_Standard];
	m_default_size[FM_Mono]      = rangelim(g_settings->getU16("mono_font_size"), 5, 72);

	m_default_bold = g_settings->getBool("font_bold");
	m_default_italic = g_settings->getBool("font_italic");

	cleanCache();
	updateFontCache();
	updateSkin();
}

/******************************************************************************/
void FontEngine::updateSkin()
{
	gui::IGUIFont *font = getFont();
	assert(font);

	m_env->getSkin()->setFont(font);
}

/******************************************************************************/
void FontEngine::updateFontCache()
{
	/* the only font to be initialized is default one,
	 * all others are re-initialized on demand */
	getFont(FONT_SIZE_UNSPECIFIED, FM_Unspecified);
}

/******************************************************************************/
gui::IGUIFont *FontEngine::initFont(const FontSpec &spec)
{
	assert(spec.mode != FM_Unspecified);
	assert(spec.size != FONT_SIZE_UNSPECIFIED);

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
		gui::CGUITTFont *font = gui::CGUITTFont::createTTFont(m_env,
				font_path.c_str(), size, true, true, font_shadow,
				font_shadow_alpha);

		if (!font) {
			errorstream << "FontEngine: Cannot load '" << font_path <<
				"'. Trying to fall back to another path." << std::endl;
			continue;
		}

		if (spec.mode != _FM_Fallback) {
			FontSpec spec2(spec);
			spec2.mode = _FM_Fallback;
			font->setFallback(getFont(spec2, true));
		}
		return font;
	}
	return nullptr;
}
