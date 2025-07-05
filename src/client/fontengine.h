// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 sapier <sapier at gmx dot net>

#pragma once

#include <map>
#include <unordered_map>
#include "irr_ptr.h"
#include "irrlicht_changes/CGUITTFont.h"
#include "util/basic_macros.h"
#include "irrlichttypes.h"
#include "irrString.h" // utf8_to_wide
#include "threading/mutex_auto_lock.h"

namespace gui {
	class IGUIEnvironment;
	class IGUIFont;
}

#define FONT_SIZE_UNSPECIFIED 0xFFFFFFFF

enum FontMode : u8 {
	/// Regular font (settings "font_path*", overwritable)
	FM_Standard = 0,

	/// Monospace font (settings "mono_font*", overwritable)
	FM_Mono,

	/// Use only in `FontEngine`. Fallback font to render glyphs that are not present
	/// in the originally requested font (setting "fallback_font_path")
	_FM_Fallback,

	/// Sum of all font modes
	FM_MaxMode,

	// ----------------------------

	/// Request the defult font specified by `s_default_font_mode`
	FM_Unspecified
};

struct FontSpec {
	FontSpec(unsigned int font_size, FontMode mode, bool bold, bool italic) :
		size(font_size),
		mode(mode),
		bold(bold),
		italic(italic) {}

	static const unsigned VARIANT_BITS = 3;
	static const size_t MAX_VARIANTS = FM_MaxMode << VARIANT_BITS;

	u16 getHash() const
	{
		return (mode << VARIANT_BITS)
			| (static_cast<u8>(allow_server_media) << 2)
			| (static_cast<u8>(bold) << 1)
			| static_cast<u8>(italic);
	}

	unsigned int size;
	FontMode mode;
	bool bold;
	bool italic;
	bool allow_server_media = true;
};

class FontEngine
{
public:

	FontEngine(gui::IGUIEnvironment* env);

	~FontEngine();

	// Get best possible font specified by FontSpec
	gui::IGUIFont *getFont(FontSpec spec);

	gui::IGUIFont *getFont(unsigned int font_size=FONT_SIZE_UNSPECIFIED,
			FontMode mode=FM_Unspecified)
	{
		FontSpec spec(font_size, mode, m_default_bold, m_default_italic);
		return getFont(spec);
	}

	/** get text height for a specific font */
	unsigned int getTextHeight(const FontSpec &spec);

	/** get text width of a text for a specific font */
	unsigned int getTextHeight(
			unsigned int font_size=FONT_SIZE_UNSPECIFIED,
			FontMode mode=FM_Unspecified)
	{
		FontSpec spec(font_size, mode, m_default_bold, m_default_italic);
		return getTextHeight(spec);
	}

	unsigned int getTextWidth(const std::wstring &text, const FontSpec &spec);

	/** get text width of a text for a specific font */
	unsigned int getTextWidth(const std::wstring& text,
			unsigned int font_size=FONT_SIZE_UNSPECIFIED,
			FontMode mode=FM_Unspecified)
	{
		FontSpec spec(font_size, mode, m_default_bold, m_default_italic);
		return getTextWidth(text, spec);
	}

	unsigned int getTextWidth(const std::string &text, const FontSpec &spec)
	{
		return getTextWidth(utf8_to_wide(text), spec);
	}

	unsigned int getTextWidth(const std::string& text,
			unsigned int font_size=FONT_SIZE_UNSPECIFIED,
			FontMode mode=FM_Unspecified)
	{
		FontSpec spec(font_size, mode, m_default_bold, m_default_italic);
		return getTextWidth(utf8_to_wide(text), spec);
	}

	/** get line height for a specific font (including empty room between lines) */
	unsigned int getLineHeight(const FontSpec &spec);

	unsigned int getLineHeight(unsigned int font_size=FONT_SIZE_UNSPECIFIED,
			FontMode mode=FM_Unspecified)
	{
		FontSpec spec(font_size, mode, m_default_bold, m_default_italic);
		return getLineHeight(spec);
	}

	/** get default font size */
	unsigned int getDefaultFontSize();

	/** get font size for a specific mode */
	unsigned int getFontSize(FontMode mode);

	/** update internal parameters from settings */
	void readSettings();

	/** reload fonts if settings were changed */
	void handleReload();

	void setMediaFont(const std::string &name, const std::string &data);

	void clearMediaFonts();

private:
	gui::IGUIFont *getFont(FontSpec spec, bool may_fail);

	/** update content of font cache in case of a setting change made it invalid */
	void updateCache();

	/** initialize a new TTF font */
	gui::IGUIFont *initFont(FontSpec spec);

	/** update current minetest skin with font changes */
	void updateSkin();

	void clearCache();

	/** refresh after fonts have been changed */
	void refresh();

	/** callback to be used on change of font size setting */
	static void fontSettingChanged(const std::string &name, void *userdata);

	/** pointer to irrlicht gui environment */
	gui::IGUIEnvironment* m_env = nullptr;

	/** mutex used to protect font init and cache */
	std::recursive_mutex m_font_mutex;

	/** internal storage for caching fonts of different size */
	std::map<unsigned int, gui::IGUIFont*> m_font_cache[FontSpec::MAX_VARIANTS];

	/** media-provided faces, indexed by filename (without extension) */
	std::unordered_map<std::string, irr_ptr<gui::SGUITTFace>> m_media_faces;

	/** default font size to use */
	unsigned int m_default_size[FM_MaxMode];

	/** default bold and italic */
	bool m_default_bold = false;
	bool m_default_italic = false;

	/** default font engine mode (fixed) */
	static const FontMode s_default_font_mode = FM_Standard;

	bool m_needs_reload = false;

	DISABLE_CLASS_COPY(FontEngine);
};

/** interface to access main font engine*/
extern FontEngine* g_fontengine;
