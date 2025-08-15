// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "inventorymanager.h"
#include <IGUIElement.h>
#include <IGUIEnvironment.h>
#include "irr_v2d.h"


class GUIFormSpecMenu;

class GUIInventoryList : public gui::IGUIElement
{
public:
	struct ItemSpec
	{
		ItemSpec() = default;

		ItemSpec(const InventoryLocation &a_inventoryloc,
				const std::string &a_listname,
				s32 a_i,
				const v2s32 slotsize) :
			inventoryloc(a_inventoryloc),
			listname(a_listname),
			i(a_i),
			slotsize(slotsize)
		{
		}

		bool operator==(const ItemSpec& other) const
		{
			return inventoryloc == other.inventoryloc &&
					listname == other.listname && i == other.i;
		}

		bool isValid() const { return i != -1; }

		InventoryLocation inventoryloc;
		std::string listname;
		s32 i = -1;
		v2s32 slotsize;
	};

	// options for inventorylists that are setable with the lua api
	struct Options {
		// whether a one-pixel border for the slots should be drawn and its color
		bool slotborder = false;
		video::SColor slotbordercolor = video::SColor(200, 0, 0, 0);
		// colors for normal and highlighted slot background
		video::SColor slotbg_n = video::SColor(255, 128, 128, 128);
		video::SColor slotbg_h = video::SColor(255, 192, 192, 192);
	};

	GUIInventoryList(gui::IGUIEnvironment *env,
		gui::IGUIElement *parent,
		s32 id,
		const core::rect<s32> &rectangle,
		InventoryManager *invmgr,
		const InventoryLocation &inventoryloc,
		const std::string &listname,
		const v2s32 &geom,
		const s32 start_item_i,
		const v2s32 &slot_size,
		const v2f32 &slot_spacing,
		GUIFormSpecMenu *fs_menu,
		const Options &options,
		gui::IGUIFont *font);

	void draw() override;

	bool OnEvent(const SEvent &event) override;

	bool isPointInside(const core::position2d<s32> &point) const override;

	const InventoryLocation &getInventoryloc() const
	{
		return m_inventoryloc;
	}

	const std::string &getListname() const
	{
		return m_listname;
	}

	void setSlotBGColors(const video::SColor &slotbg_n, const video::SColor &slotbg_h)
	{
		m_options.slotbg_n = slotbg_n;
		m_options.slotbg_h = slotbg_h;
	}

	void setSlotBorders(bool slotborder, const video::SColor &slotbordercolor)
	{
		m_options.slotborder = slotborder;
		m_options.slotbordercolor = slotbordercolor;
	}

	const v2s32 getSlotSize() const noexcept
	{
		return m_slot_size;
	}

	// returns -1 if not item is at pos p
	s32 getItemIndexAtPos(v2s32 p) const;

private:
	InventoryManager *m_invmgr;
	const InventoryLocation m_inventoryloc;
	const std::string m_listname;

	// the specified width and height of the shown inventorylist in itemslots
	const v2s32 m_geom;
	// the first item's index in inventory
	const s32 m_start_item_i;

	// specifies how large the slot rects are
	const v2s32 m_slot_size;
	// specifies how large the space between slots is (space between is spacing-size)
	const v2f32 m_slot_spacing;

	// the GUIFormSpecMenu can have an item selected and co.
	GUIFormSpecMenu *m_fs_menu;

	Options m_options;

	// the font
	gui::IGUIFont *m_font;

	// the index of the hovered item; -1 if no item is hovered
	s32 m_hovered_i;

	// we do not want to write a warning on every draw
	bool m_already_warned;
};
