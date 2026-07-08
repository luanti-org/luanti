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

	Client *client = m_fs_menu->getClient();
	const ItemSpec *selected_item = m_fs_menu->getSelectedItem();
	const s32 selected_index = selected_item
				&& m_invmgr->getInventory(selected_item->inventoryloc) == inv
				&& selected_item->listname == m_listname
			? selected_item->i : -1;

	std::vector<video::S3DVertex> vertices;
	std::vector<u16> indices_2, indices_3;
	const s32 count_slots = m_geom.X * m_geom.Y;
	vertices.reserve(count_slots * 4);
	indices_2.reserve(m_options.slotborder * count_slots * 8); // line strip (borders)
	indices_3.reserve(count_slots * 6); // triangles (filled)

	auto add_rectangle = [&vertices, &indices_2, &indices_3](
			bool filled, video::SColor color, const core::rect<s32> &rect) {

		const u16 prev_i = (u16)vertices.size();
		if (filled) {
			for (u16 i : { 0,1,2, 0,2,3 })
				indices_3.emplace_back(prev_i + i);
		} else {
			for (u16 i : { 0,1, 1,2, 2,3, 3,0 })
				indices_2.emplace_back(prev_i + i);
		}

		/*
			0 --- 1
			|     |
			3 --- 2
		*/
		const auto min = rect.UpperLeftCorner;
		const auto max = rect.LowerRightCorner;
		vertices.emplace_back(min.X, min.Y - 1, 4, 0,0,0, color, 0,0);
		vertices.emplace_back(max.X, min.Y - 1, 4, 0,0,0, color, 0,0);
		vertices.emplace_back(max.X, max.Y - 0, 4, 0,0,0, color, 0,0);
		vertices.emplace_back(min.X, max.Y - 0, 4, 0,0,0, color, 0,0);
	};

	const s32 list_size = (s32)ilist->getSize();

	struct StackToDraw {
		core::rect<s32> rect;
		const ItemStack &item;
		bool hovered;
		bool selected;
	};
	std::vector<StackToDraw> stacks;
	stacks.reserve(list_size);

	core::rect<s32> imgrect(0, 0, m_slot_size.X, m_slot_size.Y);
	imgrect += AbsoluteRect.UpperLeftCorner;

	for (s32 i = 0; i < m_geom.X * m_geom.Y; i++) {
		s32 item_i = i + m_start_item_i;
		if (item_i >= list_size)
			break;

		v2s32 p((i % m_geom.X) * m_slot_spacing.X,
				(i / m_geom.X) * m_slot_spacing.Y);
		const core::rect<s32> rect = imgrect + p;
		core::rect<s32> rect_clip = rect;
		rect_clip.clipAgainst(AbsoluteClippingRect);

		if (!rect_clip.isValid() || rect_clip.getArea() == 0)
			continue; // out of (parent) clip area

		bool selected = item_i == selected_index;
		bool hovering = item_i == m_hovered_i;

		add_rectangle(true, hovering ? m_options.slotbg_h : m_options.slotbg_n, rect_clip);

		// Draw inv slot borders
		if (m_options.slotborder) {
			core::rect<s32> rect_border = rect;
			rect_border.UpperLeftCorner -= 1;
			rect_border.clipAgainst(AbsoluteClippingRect);

			add_rectangle(false, m_options.slotbordercolor, rect_border);
		}

		const ItemStack &item = ilist->getItem(item_i);
		if (item.empty())
			continue;

		// Delay drawing item stacks after slots because this clears the depth buffer.
		stacks.emplace_back(StackToDraw{ rect, item, hovering, selected });

		// Add hovering tooltip. The tooltip disappears if any item is selected,
		// including the currently hovered one.
		bool show_tooltip = hovering && !selected_item;

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
			std::string tooltip = item.getDescription(client->idef());
			if (m_fs_menu->doTooltipAppendItemname())
				tooltip += "\n[" + item.name + "]";
			m_fs_menu->addHoveredItemTooltip(tooltip);
		}
	}

	video::IVideoDriver *driver = Environment->getVideoDriver();

	video::SMaterial mat = driver->getMaterial2D();
	mat.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
	driver->setMaterial(mat);

	driver->draw2DVertexPrimitiveList(
		vertices.data(), vertices.size(),
		indices_3.data(), indices_3.size() / 3,
		video::EVT_STANDARD, scene::EPT_TRIANGLES, video::EIT_16BIT
	);
	driver->draw2DVertexPrimitiveList(
		vertices.data(), vertices.size(),
		indices_2.data(), indices_2.size() / 2,
		video::EVT_STANDARD, scene::EPT_LINES, video::EIT_16BIT
	);

	// Draw items
	for (const StackToDraw &stack : stacks) {
		ItemRotationKind rotation_kind = stack.selected ? IT_ROT_SELECTED :
				(stack.hovered ? IT_ROT_HOVERED : IT_ROT_NONE);

		if (stack.selected) {
			ItemStack remaining_item = stack.item;
			remaining_item.takeItem(m_fs_menu->getSelectedAmount());
			if (!remaining_item.empty()) {
				drawItemStack(driver, m_font, remaining_item, stack.rect,
						&AbsoluteClippingRect, client, rotation_kind);
			}
			continue;
		}

		drawItemStack(driver, m_font, stack.item, stack.rect,
				&AbsoluteClippingRect, client, rotation_kind);
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
