/*
Part of Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#pragma once

#include "modalMenu.h"
#include <string>

class Client;
class ISimpleTextureSource;

class GUIPasswordChange : public GUIModalMenu
{
public:
	GUIPasswordChange(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
			IMenuManager *menumgr, Client *client,
			ISimpleTextureSource *tsrc);

	/*
		Remove and re-add (or reposition) stuff
	*/
	void regenerateGui(v2u32 screensize);

	void drawMenu();

	void acceptInput();

	bool processInput();

	bool OnEvent(const SEvent &event);
#ifdef __ANDROID__
	void getAndroidUIInput();
#endif

protected:
	std::wstring getLabelByID(s32 id) { return L""; }
	std::string getNameByID(s32 id);

private:
	Client *m_client;
	std::wstring m_oldpass = L"";
	std::wstring m_newpass = L"";
	std::wstring m_newpass_confirm = L"";
	ISimpleTextureSource *m_tsrc;
};
