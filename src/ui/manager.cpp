// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#include "ui/manager.h"

#include "debug.h"
#include "log.h"
#include "settings.h"
#include "client/client.h"
#include "client/renderingengine.h"
#include "client/texturesource.h"
#include "client/tile.h"
#include "gui/mainmenumanager.h"
#include "util/serialize.h"

#include <SDL2/SDL.h>

namespace ui
{
	SDL_Event createUiEvent(UiEvent type, void *data1, void *data2)
	{
		SDL_Event event;

		event.user.type = type + SDL_USEREVENT;
		event.user.code = 0;
		event.user.data1 = data1;
		event.user.data2 = data2;

		return event;
	}

	video::ITexture *Manager::getTexture(const std::string &name) const
	{
		return m_client->tsrc()->getTexture(name);
	}

	float Manager::getScale(WindowType type) const
	{
		if (type == WindowType::GUI || type == WindowType::CHAT) {
			return m_gui_scale;
		}
		return m_hud_scale;
	}

	void Manager::reset()
	{
		m_client = nullptr;

		m_windows.clear();
		m_gui_windows.clear();
	}

	void Manager::removeWindow(u64 id)
	{
		auto it = m_windows.find(id);
		if (it == m_windows.end()) {
			errorstream << "Window " << id << " is already closed" << std::endl;
			return;
		}

		m_windows.erase(it);
		m_gui_windows.erase(id);
	}

	void Manager::receiveMessage(const std::string &data)
	{
		auto is = newIs(data);

		u32 action = readU8(is);
		u64 id = readU64(is);

		switch (action) {
		case REOPEN_WINDOW: {
			u64 close_id = readU64(is);
			removeWindow(close_id);

			[[fallthrough]];
		}

		case OPEN_WINDOW: {
			auto it = m_windows.find(id);
			if (it != m_windows.end()) {
				errorstream << "Window " << id << " is already open" << std::endl;
				break;
			}

			it = m_windows.emplace(id, id).first;
			if (!it->second.read(is, true)) {
				errorstream << "Fatal error when opening window " << id <<
					"; closing window" << std::endl;
				removeWindow(id);
				break;
			}

			if (it->second.getType() == WindowType::GUI) {
				m_gui_windows.emplace(id, &it->second);
			}
			break;
		}

		case UPDATE_WINDOW: {
			auto it = m_windows.find(id);
			if (it == m_windows.end()) {
				errorstream << "Window " << id << " does not exist" << std::endl;
			}

			if (!it->second.read(is, false)) {
				errorstream << "Fatal error when updating window " << id <<
					"; closing window" << std::endl;
				removeWindow(id);
				break;
			}
			break;
		}

		case CLOSE_WINDOW:
			removeWindow(id);
			break;

		default:
			errorstream << "Invalid manager action: " << action << std::endl;
			break;
		}
	}

	void Manager::sendMessage(const std::string &data)
	{
		m_client->sendUiMessage(data.c_str(), data.size());
	}

	void Manager::preDraw()
	{
		float base_scale = RenderingEngine::getDisplayDensity();
		m_gui_scale = base_scale * g_settings->getFloat("gui_scaling");
		m_hud_scale = base_scale * g_settings->getFloat("hud_scaling");
	}

	void Manager::drawType(WindowType type)
	{
		for (auto &it : m_windows) {
			if (it.second.getType() == type) {
				it.second.drawAll();
			}
		}
	}

	Window *Manager::getFocused()
	{
		if (m_gui_windows.empty()) {
			return nullptr;
		}
		return m_gui_windows.rbegin()->second;
	}

	bool Manager::isFocused() const
	{
		return g_menumgr.menuCount() == 0 && !m_gui_windows.empty();
	}

	bool Manager::processInput(const SDL_Event &event)
	{
		Window *focused = getFocused();
		if (focused != nullptr) {
			return focused->processInput(event);
		}

		return false;
	}

	Manager g_manager;
}
