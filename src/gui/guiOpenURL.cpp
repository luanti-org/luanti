/*
Part of Minetest
Copyright (C) 2023 rubenwardy

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

#include "guiOpenURL.h"
#include "guiButton.h"
#include "guiEditBoxWithScrollbar.h"
#include <IGUIEditBox.h>
#include <IGUIStaticText.h>
#include <IGUIFont.h>

#ifdef HAVE_TOUCHSCREENGUI
	#include "client/renderingengine.h"
#endif

#include "porting.h"
#include "gettext.h"

const int ID_url = 256;
const int ID_open = 259;
const int ID_cancel = 261;

GUIOpenURLMenu::GUIOpenURLMenu(gui::IGUIEnvironment* env,
		gui::IGUIElement* parent, s32 id,
		IMenuManager *menumgr,
		ISimpleTextureSource *tsrc, const std::string &url
):
	GUIModalMenu(env, parent, id, menumgr),
	m_tsrc(tsrc),
	url(url)
{
}

void GUIOpenURLMenu::regenerateGui(v2u32 screensize)
{
	/*
		Remove stuff
	*/
	removeAllChildren();

	/*
		Calculate new sizes and positions
	*/
#ifdef HAVE_TOUCHSCREENGUI
	const float s = m_gui_scale * RenderingEngine::getDisplayDensity() / 2;
#else
	const float s = m_gui_scale;
#endif
	DesiredRect = core::rect<s32>(
		screensize.X / 2 - 580 * s / 2,
		screensize.Y / 2 - 250 * s / 2,
		screensize.X / 2 + 580 * s / 2,
		screensize.Y / 2 + 250 * s / 2
	);
	recalculateAbsolutePosition(false);

	v2s32 size = DesiredRect.getSize();
	v2s32 topleft_client(40 * s, 0);

	/*
		Add stuff
	*/
	s32 ypos = 40 * s;

	{
		core::rect<s32> rect(0, 0, 500 * s, 20 * s);
		rect += topleft_client + v2s32(20 * s, ypos);
		gui::StaticText::add(Environment, wstrgettext("Open URL?"), rect,
				false, true, this, -1);
	}

	ypos += 50 * s;

	{
		core::rect<s32> rect(0, 0, 440 * s, 60 * s);
		rect += topleft_client + v2s32(20 * s, ypos);
		IGUIEditBox *e = new GUIEditBoxWithScrollBar(utf8_to_wide(url).c_str(), true, Environment,
				this, ID_url, rect, m_tsrc, false, true);
		e->setMultiLine(true);
		e->setWordWrap(true);
		e->setTextAlignment(gui::EGUIA_UPPERLEFT, gui::EGUIA_UPPERLEFT);
		e->setDrawBorder(true);
		e->setDrawBackground(true);
		e->drop();
	}

	ypos += 80 * s;
	{
		core::rect<s32> rect(0, 0, 100 * s, 40 * s);
		rect = rect + v2s32(size.X / 2 - 150 * s, ypos);
		GUIButton::addButton(Environment, rect, m_tsrc, this, ID_open,
				wstrgettext("Open").c_str());
	}
	{
		core::rect<s32> rect(0, 0, 100 * s, 40 * s);
		rect = rect + v2s32(size.X / 2 + 50 * s, ypos);
		GUIButton::addButton(Environment, rect, m_tsrc, this, ID_cancel,
				wstrgettext("Cancel").c_str());
	}
}

void GUIOpenURLMenu::drawMenu()
{
	gui::IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return;
	video::IVideoDriver *driver = Environment->getVideoDriver();

	video::SColor bgcolor(140, 0, 0, 0);
	driver->draw2DRectangle(bgcolor, AbsoluteRect, &AbsoluteClippingRect);

	gui::IGUIElement::draw();
#ifdef __ANDROID__
	getAndroidUIInput();
#endif
}

bool GUIOpenURLMenu::OnEvent(const SEvent &event)
{
	if (event.EventType == EET_KEY_INPUT_EVENT) {
		if ((event.KeyInput.Key == KEY_ESCAPE ||
				event.KeyInput.Key == KEY_CANCEL) &&
				event.KeyInput.PressedDown) {
			quitMenu();
			return true;
		}
		if (event.KeyInput.Key == KEY_RETURN && event.KeyInput.PressedDown) {
			porting::open_url(url);
			quitMenu();
			return true;
		}
	}

	if (event.EventType == EET_GUI_EVENT) {
		if (event.GUIEvent.EventType == gui::EGET_ELEMENT_FOCUS_LOST &&
				isVisible()) {
			if (!canTakeFocus(event.GUIEvent.Element)) {
				infostream << "GUIOpenURLMenu: Not allowing focus change."
					<< std::endl;
				// Returning true disables focus change
				return true;
			}
		}

		if (event.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED) {
			switch (event.GUIEvent.Caller->getID()) {
			case ID_open:
				porting::open_url(url);
				quitMenu();
				return true;
			case ID_cancel:
				quitMenu();
				return true;
			}
		}
	}

	return Parent ? Parent->OnEvent(event) : false;
}
