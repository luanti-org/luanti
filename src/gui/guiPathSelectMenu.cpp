// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier

#include "guiPathSelectMenu.h"
#include "guiFormSpecMenu.h" //required because of TextDest only !!!

GUIFileSelectMenu::GUIFileSelectMenu(gui::IGUIEnvironment* env,
		gui::IGUIElement* parent, s32 id, IMenuManager *menumgr,
		const std::string &title, const std::string &formname,
		bool is_file_select) :
	GUIModalMenu(env, parent, id, menumgr),
	m_title(utf8_to_wide(title)),
	m_formname(formname),
	m_file_select_dialog(is_file_select)
{
}

GUIFileSelectMenu::~GUIFileSelectMenu()
{
	setlocale(LC_NUMERIC, "C");
}

void GUIFileSelectMenu::regenerateGui(v2u32 screensize)
{
	removeAllChildren();
	m_fileOpenDialog = 0;

	core::dimension2du size(600 * m_gui_scale, 400 * m_gui_scale);
	core::rect<s32> rect(0, 0, screensize.X, screensize.Y);

	DesiredRect = rect;
	recalculateAbsolutePosition(false);

	m_fileOpenDialog =
			Environment->addFileOpenDialog(m_title.c_str(), false, this, -1);

	core::position2di pos = core::position2di(screensize.X / 2 - size.Width / 2,
			screensize.Y / 2 - size.Height / 2);
	m_fileOpenDialog->setRelativePosition(pos);
	m_fileOpenDialog->setMinSize(size);
}

void GUIFileSelectMenu::drawMenu()
{
	gui::IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return;

	gui::IGUIElement::draw();
}

void GUIFileSelectMenu::acceptInput()
{
	if (m_text_dst && !m_formname.empty()) {
		StringMap fields;
		if (m_accepted) {
			std::string path;
			if (!m_file_select_dialog) {
				core::string<fschar_t> string =
						m_fileOpenDialog->getDirectoryName();
				path = std::string(string.c_str());
			} else {
				path = wide_to_utf8(m_fileOpenDialog->getFileName());
			}
			fields[m_formname + "_accepted"] = path;
		} else {
			fields[m_formname + "_canceled"] = m_formname;
		}
		m_text_dst->gotText(fields);
	}
	quitMenu();
}

bool GUIFileSelectMenu::OnEvent(const SEvent &event)
{
	if (event.EventType == EET_GUI_EVENT) {
		switch (event.GUIEvent.EventType) {
		case gui::EGET_ELEMENT_CLOSED:
		case gui::EGET_FILE_CHOOSE_DIALOG_CANCELLED:
			m_accepted = false;
			acceptInput();
			return true;
		case gui::EGET_DIRECTORY_SELECTED:
			m_accepted = !m_file_select_dialog;
			acceptInput();
			return true;
		case gui::EGET_FILE_SELECTED:
			m_accepted = m_file_select_dialog;
			acceptInput();
			return true;
		default:
			// ignore this event
			break;
		}
	}
	return Parent ? Parent->OnEvent(event) : false;
}
