// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#pragma once

#include "ui/helpers.h"
#include "ui/window.h"
#include "util/basic_macros.h"

#include <IGUIElement.h>

#include <iostream>
#include <map>
#include <string>

class Client;

union SDL_Event;

namespace ui
{
	/* Custom UI-specific event types for events of type SDL_USEREVENT. Create
	 * the event structure with createUiEvent().
	 *
	 * Some events should always return false to give parent elements and other
	 * boxes a chance to see the event. Other events return true to indicate
	 * that the element may become focused or hovered.
	 */
	enum UiEvent
	{
		UI_FOCUS_REQUEST, // Return true to accept request.
		UI_FOCUS_CHANGED, // Never return true.
		UI_FOCUS_SUBVERTED, // Not sent to parent elements. Never return true.

		UI_HOVER_REQUEST, // Return true to accept request.
		UI_HOVER_CHANGED,
	};

#define UI_USER(event) (UI_##event + SDL_USEREVENT)

	SDL_Event createUiEvent(UiEvent type, void *data1 = nullptr, void *data2 = nullptr);

	class Manager
	{
	public:
		// Serialized enum; do not change values of entries.
		enum ReceiveAction : u8
		{
			OPEN_WINDOW   = 0x00,
			REOPEN_WINDOW = 0x01,
			UPDATE_WINDOW = 0x02,
			CLOSE_WINDOW  = 0x03,
		};

		// Serialized enum; do not change values of entries.
		enum SendAction : u8
		{
			WINDOW_EVENT = 0x00,
			ELEM_EVENT   = 0x01,
		};

	private:
		Client *m_client;

		float m_gui_scale = 0.0f;
		float m_hud_scale = 0.0f;

		// Use map rather than unordered_map so that windows are always sorted
		// by window ID to make sure that they are drawn in order of creation.
		std::map<u64, Window> m_windows;

		// Keep track of which GUI windows are currently open. We also use a
		// map so we can easily find the topmost window.
		std::map<u64, Window *> m_gui_windows;

	public:
		Manager()
		{
			reset();
		}

		DISABLE_CLASS_COPY(Manager)

		Client *getClient() const { return m_client; }
		void setClient(Client *client) { m_client = client; }

		video::ITexture *getTexture(const std::string &name) const;

		float getScale(WindowType type) const;

		void reset();
		void removeWindow(u64 id);

		void receiveMessage(const std::string &data);
		void sendMessage(const std::string &data);

		void preDraw();
		void drawType(WindowType type);

		Window *getFocused();

		bool isFocused() const;
		bool processInput(const SDL_Event &event);
	};

	extern Manager g_manager;

	// Inconveniently, we need a way to draw the "gui" window types after the
	// chat console but before other GUIs like the key change menu, formspecs,
	// etc. So, we inject our own mini Irrlicht element in between.
	class GUIManagerElem : public gui::IGUIElement
	{
	public:
		GUIManagerElem(gui::IGUIEnvironment* env, gui::IGUIElement* parent, s32 id) :
			gui::IGUIElement(gui::EGUIET_ELEMENT, env, parent, id, core::recti())
		{}

		virtual void draw() override
		{
			g_manager.drawType(ui::WindowType::GUI);
			gui::IGUIElement::draw();
		}
	};
}
