// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 grorp, Gregor Parzefall <grorp@posteo.de>

#include "touchscreenlayout.h"
#include "client/renderingengine.h"
#include "client/texturesource.h"
#include "convert_json.h"
#include "gettext.h"
#include "settings.h"
#include <json/json.h>

#include "IGUIFont.h"
#include "IGUIStaticText.h"

const char *button_names[] = {
	"jump",
	"sneak",
	"zoom",
	"aux1",
	"overflow",

	"chat",
	"inventory",
	"drop",
	"exit",

	"fly",
	"fast",
	"noclip",
	"debug",
	"camera",
	"range",
	"minimap",
	"toggle_chat",

	"joystick_off",
	"joystick_bg",
	"joystick_center",
};

// compare with GUIKeyChangeMenu::init_keys
const char *button_titles[] = {
	N_("Jump"),
	N_("Sneak"),
	N_("Zoom"),
	N_("Aux1"),
	N_("Overflow menu"),

	N_("Chat"),
	N_("Inventory"),
	N_("Drop"),
	N_("Exit"),

	N_("Toggle fly"),
	N_("Toggle fast"),
	N_("Toggle noclip"),
	N_("Toggle debug"),
	N_("Change camera"),
	N_("Range select"),
	N_("Toggle minimap"),
	N_("Toggle chat log"),

	N_("Joystick"),
	N_("Joystick"),
	N_("Joystick"),
};

const char *button_image_names[] = {
	"jump_btn.png",
	"down.png",
	"zoom.png",
	"aux1_btn.png",
	"overflow_btn.png",

	"chat_btn.png",
	"inventory_btn.png",
	"drop_btn.png",
	"exit_btn.png",

	"fly_btn.png",
	"fast_btn.png",
	"noclip_btn.png",
	"debug_btn.png",
	"camera_btn.png",
	"rangeview_btn.png",
	"minimap_btn.png",
	// toggle button: switches between "chat_hide_btn.png" and "chat_show_btn.png"
	"chat_hide_btn.png",

	"joystick_off.png",
	"joystick_bg.png",
	"joystick_center.png",
};

v2s32 ButtonMeta::getPos(v2u32 screensize, s32 button_size) const
{
	return v2s32((position.X * screensize.X) + (offset.X * button_size),
			(position.Y * screensize.Y) + (offset.Y * button_size));
}

void ButtonMeta::setPos(v2s32 pos, v2u32 screensize, s32 button_size)
{
	v2s32 third(screensize.X / 3, screensize.Y / 3);

	if (pos.X < third.X)
		position.X = 0.0f;
	else if (pos.X < 2 * third.X)
		position.X = 0.5f;
	else
		position.X = 1.0f;

	if (pos.Y < third.Y)
		position.Y = 0.0f;
	else if (pos.Y < 2 * third.Y)
		position.Y = 0.5f;
	else
		position.Y = 1.0f;

	offset.X = (pos.X - (position.X * screensize.X)) / button_size;
	offset.Y = (pos.Y - (position.Y * screensize.Y)) / button_size;
}

bool ButtonLayout::isButtonAllowed(touch_gui_button_id id)
{
	return id != joystick_off_id && id != joystick_bg_id && id != joystick_center_id &&
			id != touch_gui_button_id_END;
}

bool ButtonLayout::isButtonRequired(touch_gui_button_id id)
{
	return id == overflow_id;
}

s32 ButtonLayout::getButtonSize(v2u32 screensize)
{
	return std::min(screensize.Y / 4.5f,
			RenderingEngine::getDisplayDensity() * 65.0f *
					g_settings->getFloat("hud_scaling"));
}

const ButtonLayout ButtonLayout::predefined {{
	{jump_id, {
		v2f(1.0f, 1.0f),
		v2f(-1.0f, -0.5f),
	}},
	{sneak_id, {
		v2f(1.0f, 1.0f),
		v2f(-2.5f, -0.5f),
	}},
	{zoom_id, {
		v2f(1.0f, 1.0f),
		v2f(-0.75f, -3.5f),
	}},
	{aux1_id, {
		v2f(1.0f, 1.0f),
		v2f(-0.75f, -2.0f),
	}},
	{overflow_id, {
		v2f(1.0f, 1.0f),
		v2f(-0.75f, -5.0f),
	}},
}};

ButtonLayout ButtonLayout::loadFromSettings()
{
	bool restored = false;
	ButtonLayout layout;

	std::string str = g_settings->get("touch_layout");
	if (!str.empty()) {
		std::istringstream iss(str);
		try {
			layout.deserializeJson(iss);
			restored = true;
		} catch (const Json::Exception &e) {
			warningstream << "Could not parse touchscreen layout: " << e.what() << std::endl;
		}
	}

	if (!restored)
		return predefined;

	return layout;
}

std::unordered_map<touch_gui_button_id, irr_ptr<video::ITexture>> ButtonLayout::texture_cache;

video::ITexture *ButtonLayout::getTexture(touch_gui_button_id btn, ISimpleTextureSource *tsrc)
{
	if (texture_cache.count(btn) > 0)
		return texture_cache.at(btn).get();

	video::ITexture *tex = tsrc->getTexture(button_image_names[btn]);
	if (!tex)
		// necessary in the mainmenu
		tex = tsrc->getTexture(porting::path_share + "/textures/base/pack/" +
				button_image_names[btn]);
	irr_ptr<video::ITexture> ptr;
	ptr.grab(tex);
	texture_cache[btn] = ptr;
	return tex;
}

void ButtonLayout::clearTextureCache()
{
	texture_cache.clear();
}

core::recti ButtonLayout::getRect(touch_gui_button_id btn,
		v2u32 screensize, s32 button_size, ISimpleTextureSource *tsrc)
{
	const ButtonMeta &meta = layout.at(btn);
	v2s32 pos = meta.getPos(screensize, button_size);

	v2u32 orig_size = getTexture(btn, tsrc)->getOriginalSize();
	v2s32 size((button_size * orig_size.X) / orig_size.Y, button_size);

	return core::recti(pos - size / 2, core::dimension2di(size));
}

std::vector<touch_gui_button_id> ButtonLayout::getMissingButtons()
{
	std::vector<touch_gui_button_id> missing_buttons;
	for (u8 i = 0; i < touch_gui_button_id_END; i++) {
		touch_gui_button_id btn = (touch_gui_button_id)i;
		if (isButtonAllowed(btn) && layout.count(btn) == 0)
			missing_buttons.push_back(btn);
	}
	return missing_buttons;
}

void ButtonLayout::serializeJson(std::ostream &os) const
{
	Json::Value root = Json::objectValue;
	root["layout"] = Json::objectValue;

	for (const auto &[id, meta] : layout) {
		Json::Value button = Json::objectValue;
		button["position_x"] = meta.position.X;
		button["position_y"] = meta.position.Y;
		button["offset_x"] = meta.offset.X;
		button["offset_y"] = meta.offset.Y;

		root["layout"][button_names[id]] = button;
	}

	fastWriteJson(root, os);
}

static touch_gui_button_id button_name_to_id(const std::string &name)
{
	for (u8 i = 0; i < touch_gui_button_id_END; i++) {
		if (name == button_names[i])
			return (touch_gui_button_id)i;
	}
	return touch_gui_button_id_END;
}

void ButtonLayout::deserializeJson(std::istream &is)
{
	layout.clear();

	Json::Value root;
	is >> root;

	if (!root["layout"].isObject())
		throw Json::RuntimeError("invalid type for layout");

	Json::Value &obj = root["layout"];
	Json::ValueIterator iter;
	for (iter = obj.begin(); iter != obj.end(); iter++) {
		touch_gui_button_id id = button_name_to_id(iter.name());
		if (!isButtonAllowed(id))
			throw Json::RuntimeError("invalid button name");

		Json::Value &value = *iter;
		if (!value.isObject())
			throw Json::RuntimeError("invalid type for button metadata");

		ButtonMeta meta;

		if (!value["position_x"].isNumeric() || !value["position_y"].isNumeric())
			throw Json::RuntimeError("invalid type for position_x or position_y in button metadata");
		meta.position.X = value["position_x"].asFloat();
		meta.position.Y = value["position_y"].asFloat();

		if (!value["offset_x"].isNumeric() || !value["offset_y"].isNumeric())
			throw Json::RuntimeError("invalid type for offset_x or offset_y in button metadata");
		meta.offset.X = value["offset_x"].asFloat();
		meta.offset.Y = value["offset_y"].asFloat();

		layout.emplace(id, meta);
	}
}

void layout_button_grid(v2u32 screensize, ISimpleTextureSource *tsrc,
		const std::vector<touch_gui_button_id> &buttons,
		// pos refers to the center of the button
		const std::function<void(touch_gui_button_id btn, v2s32 pos, core::recti rect)> &callback)
{
	s32 cols = 4;
	s32 rows = 3;
	f32 screen_aspect = (f32)screensize.X / (f32)screensize.Y;
	while ((s32)buttons.size() > cols * rows) {
		f32 aspect = (f32)cols / (f32)rows;
		if (aspect > screen_aspect)
			rows++;
		else
			cols++;
	}

	s32 button_size = ButtonLayout::getButtonSize(screensize);
	v2s32 spacing(screensize.X / (cols + 1), screensize.Y / (rows + 1));
	v2s32 pos(spacing);

	for (touch_gui_button_id btn : buttons) {
		v2u32 orig_size = ButtonLayout::getTexture(btn, tsrc)->getOriginalSize();
		v2s32 size((button_size * orig_size.X) / orig_size.Y, button_size);

		core::recti rect(pos - size / 2, core::dimension2di(size));

		if (rect.LowerRightCorner.X > (s32)screensize.X) {
			pos.X = spacing.X;
			pos.Y += spacing.Y;
			rect = core::recti(pos - size / 2, core::dimension2di(size));
		}

		callback(btn, pos, rect);

		pos.X += spacing.X;
	}
}

void make_button_grid_title(gui::IGUIStaticText *text, touch_gui_button_id btn, v2s32 pos, core::recti rect)
{
	std::wstring str = wstrgettext(button_titles[btn]);
	text->setText(str.c_str());
	gui::IGUIFont *font = text->getActiveFont();
	core::dimension2du dim = font->getDimension(str.c_str());
	dim = core::dimension2du(dim.Width * 1.25f, dim.Height * 1.25f); // avoid clipping
	text->setRelativePosition(core::recti(pos.X - dim.Width / 2, rect.LowerRightCorner.Y,
			pos.X + dim.Width / 2, rect.LowerRightCorner.Y + dim.Height));
	text->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_UPPERLEFT);
}
