// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 grorp, Gregor Parzefall <grorp@posteo.de>

#pragma once

#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include "rect.h"
#include "util/enum_string.h"
#include <iostream>
#include <unordered_map>

class ISimpleTextureSource;
namespace gui
{
	class IGUIStaticText;
}
namespace video
{
	class ITexture;
}

enum TouchInteractionStyle : u8
{
	TAP,
	TAP_CROSSHAIR,
	BUTTONS_CROSSHAIR,
};
extern const struct EnumString es_TouchInteractionStyle[];

enum touch_gui_button_id : u8
{
	dig_id = 0,
	place_id,

	jump_id,
	sneak_id,
	zoom_id,
	aux1_id,
	overflow_id,

	// formerly "rare controls bar"
	chat_id,
	inventory_id,
	drop_id,
	exit_id,

	// formerly "settings bar"
	fly_id,
	fast_id,
	noclip_id,
	debug_id,
	camera_id,
	range_id,
	minimap_id,
	toggle_chat_id,

	// the joystick
	joystick_off_id,
	joystick_bg_id,
	joystick_center_id,

	touch_gui_button_id_END,
};

extern const char *button_names[];
extern const char *button_titles[];
extern const char *button_image_names[];

struct ButtonMeta {
	// Position, specified as a percentage of the screensize in the range [0,1].
	// The editor currently writes the values 0, 0.5 and 1.
	v2f position;
	// Offset, multiplied by the global button size before it is applied.
	// Together, position and offset define the position of the button's center.
	v2f offset;

	// Returns the button's effective center position in pixels.
	v2s32 getPos(v2u32 screensize, s32 button_size) const;
	// Sets the button's effective center position in pixels.
	void setPos(v2s32 pos, v2u32 screensize, s32 button_size);
};

struct ButtonLayout {
	using ButtonMap = std::unordered_map<touch_gui_button_id, ButtonMeta>;

	static bool isButtonValid(touch_gui_button_id id);
	static bool isButtonAllowed(touch_gui_button_id id);
	static bool isButtonRequired(touch_gui_button_id id);
	static s32 getButtonSize(v2u32 screensize);

	// Returns the default layout.
	// Note: Indirectly depends on settings.
	static ButtonLayout loadDefault();
	// Reads the layout from the "touch_layout" setting. Falls back to loadDefault
	// if the setting is absent or invalid.
	// Note: Indirectly depends on additional settings.
	static ButtonLayout loadFromSettings();

	static video::ITexture *getTexture(touch_gui_button_id btn, ISimpleTextureSource *tsrc);
	static void clearTextureCache();

	ButtonMap layout;

	// Contains disallowed buttons that have been loaded.
	// These are only preserved to be saved again later.
	// This exists to prevent data loss: If you edit the layout while some button
	// is temporarily disallowed, this prevents that button's custom position
	// from being lost. See isButtonAllowed.
	// This may result in overlapping buttons when the buttons are allowed again
	// (could be fixed by resolving collisions in postProcessLoaded).
	ButtonMap preserved_disallowed;

	core::recti getRect(touch_gui_button_id btn,
			v2u32 screensize, s32 button_size, ISimpleTextureSource *tsrc);

	std::vector<touch_gui_button_id> getMissingButtons();

	void serializeJson(std::ostream &os) const;

private:
	static const ButtonMap default_data;
	static ButtonMap deserializeJson(std::istream &is);
	static ButtonLayout postProcessLoaded(const ButtonMap &map);

	static std::unordered_map<touch_gui_button_id, irr_ptr<video::ITexture>> texture_cache;
};

void layout_button_grid(v2u32 screensize, ISimpleTextureSource *tsrc,
		const std::vector<touch_gui_button_id> &buttons,
		// pos refers to the center of the button.
		const std::function<void(touch_gui_button_id btn, v2s32 pos, core::recti rect)> &callback);

void make_button_grid_title(gui::IGUIStaticText *text,
		touch_gui_button_id btn,v2s32 pos, core::recti rect);
