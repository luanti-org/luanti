// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIElement.h"
#include "path.h"

namespace gui
{

//! Standard file chooser dialog.
/** \warning When the user selects a folder this does change the current working directory

\par This element can create the following events of type EGUI_EVENT_TYPE:
\li EGET_DIRECTORY_SELECTED
\li EGET_FILE_SELECTED
\li EGET_FILE_CHOOSE_DIALOG_CANCELLED
*/
class IGUIFileOpenDialog : public IGUIElement
{
public:
	//! constructor
	IGUIFileOpenDialog(IGUIEnvironment *environment, IGUIElement *parent, s32 id, core::rect<s32> rectangle) :
			IGUIElement(EGUIET_FILE_OPEN_DIALOG, environment, parent, id, rectangle) {}

	//! Returns the filename of the selected file converted to wide characters. Returns NULL if no file was selected.
	virtual const wchar_t *getFileName() const = 0;

	//! Returns the filename of the selected file. Is empty if no file was selected.
	virtual const io::path &getFileNameP() const = 0;

	//! Returns the directory of the selected file. Empty if no directory was selected.
	virtual const io::path &getDirectoryName() const = 0;

	//! Returns the directory of the selected file converted to wide characters. Returns NULL if no directory was selected.
	virtual const wchar_t *getDirectoryNameW() const = 0;
};

} // end namespace gui
