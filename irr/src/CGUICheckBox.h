// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUICheckBox.h"

namespace gui
{

class CGUICheckBox : public IGUICheckBox
{
public:
	//! constructor
	CGUICheckBox(bool checked, IGUIEnvironment *environment, IGUIElement *parent, s32 id, core::rect<s32> rectangle);

	//! set if box is checked
	void setChecked(bool checked) override;

	//! returns if box is checked
	bool isChecked() const override;

	//! Sets whether to draw the background
	void setDrawBackground(bool draw) override;

	//! Checks if background drawing is enabled
	/** \return true if background drawing is enabled, false otherwise */
	bool isDrawBackgroundEnabled() const override;

	//! Sets whether to draw the border
	void setDrawBorder(bool draw) override;

	//! Checks if border drawing is enabled
	/** \return true if border drawing is enabled, false otherwise */
	bool isDrawBorderEnabled() const override;

	//! called if an event happened.
	bool OnEvent(const SEvent &event) override;

	//! draws the element and its children
	void draw() override;

private:
	u32 CheckTime;
	bool Pressed;
	bool Checked;
	bool Border;
	bool Background;
};

} // end namespace gui
