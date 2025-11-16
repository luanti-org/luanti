// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "guiInventoryList.h"
#include "guiFormSpecMenu.h"
#include "drawItemStack.h"
#include "client/client.h"
#include "client/renderingengine.h"
#include <IVideoDriver.h>

GUIInventoryList::GUIInventoryList(gui::IGUIEnvironment *env,
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
	gui::IGUIFont *font) :
	gui::IGUIElement(gui::EGUIET_ELEMENT, env, parent, id, rectangle),
	m_invmgr(invmgr),
	m_inventoryloc(inventoryloc),
	m_listname(listname),
	m_geom(geom),
	m_start_item_i(start_item_i),
	m_slot_size(slot_size),
	m_slot_spacing(slot_spacing),
	m_fs_menu(fs_menu),
	m_options(options),
	m_font(font),
	m_hovered_i(-1),
	m_already_warned(false)
{
}

void GUIInventoryList::draw()
{
	if (!IsVisible)
		return;

	Inventory *inv = m_invmgr->getInventory(m_inventoryloc);
	if (!inv) {
		if (!m_already_warned) {
			warningstream << "GUIInventoryList::draw(): "
					<< "The inventory location "
					<< "\"" << m_inventoryloc.dump() << "\" doesn't exist"
					<< std::endl;
			m_already_warned = true;
		}
		return;
	}
	InventoryList *ilist = inv->getList(m_listname);
	if (!ilist) {
		if (!m_already_warned) {
			warningstream << "GUIInventoryList::draw(): "
					<< "The inventory list \"" << m_listname << "\" @ \""
					<< m_inventoryloc.dump() << "\" doesn't exist"
					<< std::endl;
			m_already_warned = true;
		}
		return;
	}
	m_already_warned = false;

	video::IVideoDriver *driver = Environment->getVideoDriver();
	Client *client = m_fs_menu->getClient();
	const ItemSpec *selected_item = m_fs_menu->getSelectedItem();

	core::rect<s32> imgrect(0, 0, m_slot_size.X, m_slot_size.Y);
	v2s32 base_pos = AbsoluteRect.UpperLeftCorner;

	const s32 list_size = (s32)ilist->getSize();

	for (s32 i = 0; i < m_geom.X * m_geom.Y; i++) {
		s32 item_i = i + m_start_item_i;
		if (item_i >= list_size)
			break;

		v2s32 p((i % m_geom.X) * m_slot_spacing.X,
				(i / m_geom.X) * m_slot_spacing.Y);
		core::rect<s32> rect = imgrect + base_pos + p;

		if (!getAbsoluteClippingRect().isRectCollided(rect))
			continue; // out of (parent) clip area

		const ItemStack &orig_item = ilist->getItem(item_i);
		ItemStack item = orig_item;

		bool selected = selected_item
			&& m_invmgr->getInventory(selected_item->inventoryloc) == inv
			&& selected_item->listname == m_listname
			&& selected_item->i == item_i;
		bool hovering = m_hovered_i == item_i;
		ItemRotationKind rotation_kind = selected ? IT_ROT_SELECTED :
			(hovering ? IT_ROT_HOVERED : IT_ROT_NONE);

		// layer 0
		if (hovering) {
			driver->draw2DRectangle(m_options.slotbg_h, rect, &AbsoluteClippingRect);
		} else {
			driver->draw2DRectangle(m_options.slotbg_n, rect, &AbsoluteClippingRect);
		}

		// Draw inv slot borders
		if (m_options.slotborder) {
			s32 x1 = rect.UpperLeftCorner.X;
			s32 y1 = rect.UpperLeftCorner.Y;
			s32 x2 = rect.LowerRightCorner.X;
			s32 y2 = rect.LowerRightCorner.Y;
			s32 border = 1;
			core::rect<s32> clipping_rect = Parent ? Parent->getAbsoluteClippingRect()
					: core::rect<s32>();
			core::rect<s32> *clipping_rect_ptr = Parent ? &clipping_rect : nullptr;
			driver->draw2DRectangle(m_options.slotbordercolor,
				core::rect<s32>(v2s32(x1 - border, y1 - border),
								v2s32(x2 + border, y1)), clipping_rect_ptr);
			driver->draw2DRectangle(m_options.slotbordercolor,
				core::rect<s32>(v2s32(x1 - border, y2),
								v2s32(x2 + border, y2 + border)), clipping_rect_ptr);
			driver->draw2DRectangle(m_options.slotbordercolor,
				core::rect<s32>(v2s32(x1 - border, y1),
								v2s32(x1, y2)), clipping_rect_ptr);
			driver->draw2DRectangle(m_options.slotbordercolor,
				core::rect<s32>(v2s32(x2, y1),
								v2s32(x2 + border, y2)), clipping_rect_ptr);
		}

		// layer 1
		if (selected)
			item.takeItem(m_fs_menu->getSelectedAmount());

		if (!item.empty()) {
			// Draw item stack
			drawItemStack(driver, m_font, item, rect, &AbsoluteClippingRect,
					client, rotation_kind);
		}

		// Add hovering tooltip. The tooltip disappears if any item is selected,
		// including the currently hovered one.
		bool show_tooltip = !item.empty() && hovering && !selected_item;

		if (RenderingEngine::getLastPointerType() == PointerType::Touch) {
			// Touchscreen users cannot hover over an item without selecting it.
			// To allow touchscreen users to see item tooltips, we also show the
			// tooltip if the item is selected and the finger is still on the
			// source slot.
			// The selected amount may be 0 in rare cases during "left-dragging"
			// (used to distribute items evenly).
			// In this case, the user doesn't see an item being dragged,
			// so we don't show the tooltip.
			// Note: `m_fs_menu->getSelectedAmount() != 0` below refers to the
			// part of the selected item the user is dragging.
			// `!item.empty()` would refer to the part of the selected item
			// remaining in the source slot.
			show_tooltip |= hovering && selected && m_fs_menu->getSelectedAmount() != 0;
		}

		if (show_tooltip) {
			std::string tooltip = orig_item.getDescription(client->idef());
			if (m_fs_menu->doTooltipAppendItemname())
				tooltip += "\n[" + orig_item.name + "]";
			m_fs_menu->addHoveredItemTooltip(tooltip);
		}
	}

	IGUIElement::draw();
}

bool GUIInventoryList::OnEvent(const SEvent &event)
{
	if (event.EventType != EET_MOUSE_INPUT_EVENT) {
		if (event.EventType == EET_GUI_EVENT &&
				event.GUIEvent.EventType == EGET_ELEMENT_LEFT) {
			// element is no longer hovered
			m_hovered_i = -1;
		}
		return IGUIElement::OnEvent(event);
	}

	m_hovered_i = getItemIndexAtPos(v2s32(event.MouseInput.X, event.MouseInput.Y));

	return IGUIElement::OnEvent(event);
}


bool GUIInventoryList::isPointInside(const core::position2d<s32> &point) const
{
	return getItemIndexAtPos(point) != -1;
}

s32 GUIInventoryList::getItemIndexAtPos(v2s32 p) const
{
	// no item if no gui element at pointer
	if (!IsVisible || AbsoluteClippingRect.getArea() <= 0 ||
			!AbsoluteClippingRect.isPointInside(p))
		return -1;

	// there cannot be an item if the inventory or the inventorylist does not exist
	Inventory *inv = m_invmgr->getInventory(m_inventoryloc);
	if (!inv)
		return -1;
	InventoryList *ilist = inv->getList(m_listname);
	if (!ilist)
		return -1;

	core::rect<s32> imgrect(0, 0, m_slot_size.X, m_slot_size.Y);
	v2s32 base_pos = AbsoluteRect.UpperLeftCorner;

	// instead of looping through each slot, we look where p would be in the grid
	s32 i = static_cast<s32>((p.X - base_pos.X) / m_slot_spacing.X)
			+ static_cast<s32>((p.Y - base_pos.Y) / m_slot_spacing.Y) * m_geom.X;

	v2s32 p0((i % m_geom.X) * m_slot_spacing.X,
			(i / m_geom.X) * m_slot_spacing.Y);

	core::rect<s32> rect = imgrect + base_pos + p0;

	rect.clipAgainst(AbsoluteClippingRect);

	if (rect.getArea() > 0 && rect.isPointInside(p) &&
			i + m_start_item_i < (s32)ilist->getSize())
		return i + m_start_item_i;

	return -1;
}
