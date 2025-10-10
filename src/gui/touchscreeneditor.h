// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 grorp, Gregor Parzefall <grorp@posteo.de>

#pragma once

#include "touchscreenlayout.h"
#include "modalMenu.h"

#include <memory>
#include <unordered_map>

class ISimpleTextureSource;
namespace gui
{
	class IGUIButton;
	class IGUIImage;
}

class GUITouchscreenLayout : public GUIModalMenu
{
public:
	GUITouchscreenLayout(gui::IGUIEnvironment* env,
			gui::IGUIElement* parent, s32 id,
			IMenuManager *menumgr, ISimpleTextureSource *tsrc);
	~GUITouchscreenLayout();

	void regenerateGui(v2u32 screensize);
	void drawMenu();
	bool OnEvent(const SEvent& event);

protected:
	std::wstring getLabelByID(s32 id) { return L""; }
	std::string getNameByID(s32 id) { return ""; }

private:
	ISimpleTextureSource *m_tsrc;

	ButtonLayout m_layout;
	v2u32 m_last_screensize;
	s32 m_button_size;

	enum class Mode {
		Default,
		Dragging,
		Add,
	};
	Mode m_mode = Mode::Default;

	std::unordered_map<touch_gui_button_id, std::shared_ptr<gui::IGUIImage>> m_gui_images;
	// unused if m_mode == Mode::Add
	std::unordered_map<touch_gui_button_id, v2s32> m_gui_images_target_pos;
	void clearGUIImages();
	void regenerateGUIImagesRegular(v2u32 screensize);
	void regenerateGUIImagesAddMode(v2u32 screensize);
	void interpolateGUIImages();

	// interaction state
	bool m_mouse_down = false;
	v2s32 m_last_mouse_pos;
	touch_gui_button_id m_selected_btn = touch_gui_button_id_END;

	// dragging
	ButtonLayout m_last_good_layout;
	std::vector<core::recti> m_error_rects;
	void updateDragState(v2u32 screensize, v2s32 mouse_movement);

	// add mode
	ButtonLayout m_add_layout;
	std::vector<std::shared_ptr<gui::IGUIStaticText>> m_add_button_titles;

	// Menu GUI elements
	std::shared_ptr<gui::IGUIStaticText> m_gui_help_text;

	std::shared_ptr<gui::IGUIButton> m_gui_add_btn;
	std::shared_ptr<gui::IGUIButton> m_gui_reset_btn;
	std::shared_ptr<gui::IGUIButton> m_gui_done_btn;

	std::shared_ptr<gui::IGUIButton> m_gui_remove_btn;

	void regenerateMenu(v2u32 screensize);
};
