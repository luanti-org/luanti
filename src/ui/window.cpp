// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#include "ui/window.h"

#include "debug.h"
#include "log.h"
#include "settings.h"
#include "client/client.h"
#include "client/renderingengine.h"
#include "client/tile.h"
#include "ui/box.h"
#include "ui/manager.h"
#include "ui/static_elems.h"
#include "util/serialize.h"
#include "util/string.h"

namespace ui
{
	SizeI getTextureSize(video::ITexture *texture)
	{
		if (texture != nullptr) {
			return SizeI(texture->getOriginalSize());
		}
		return SizeI(0, 0);
	}

	WindowType toWindowType(u8 type)
	{
		if (type > (u8)WindowType::MAX) {
			return WindowType::HUD;
		}
		return (WindowType)type;
	}

	Elem *Window::getElem(const std::string &id, bool required)
	{
		// Empty IDs may be valid values if the element is optional.
		if (id.empty() && !required) {
			return nullptr;
		}

		// If the ID is not empty, then we need to search for an actual
		// element. Not finding one means that an error occurred.
		auto it = m_elems.find(id);
		if (it != m_elems.end()) {
			return it->second.get();
		}

		errorstream << "Element \"" << id << "\" does not exist" << std::endl;
		return nullptr;
	}

	const std::string *Window::getStyleStr(u32 index) const
	{
		if (index < m_style_strs.size()) {
			return &m_style_strs[index];
		}
		return nullptr;
	}

	void Window::reset()
	{
		m_elems.clear();
		m_ordered_elems.clear();

		m_root_elem = nullptr;

		m_style_strs.clear();
	}

	bool Window::read(std::istream &is, bool opening)
	{
		std::unordered_map<Elem *, std::string> elem_contents;
		readElems(is, elem_contents);

		if (!readRootElem(is)) {
			return false;
		}

		readStyles(is);

		if (opening) {
			m_type = toWindowType(readU8(is));
		}

		// Finally, we can proceed to read in all the element properties.
		return updateElems(elem_contents);
	}

	float Window::getScale() const
	{
		return g_manager.getScale(m_type);
	}

	SizeF Window::getScreenSize() const
	{
		SizeF size = RenderingEngine::get_video_driver()->getCurrentRenderTargetSize();
		return size / getScale();
	}

	void Window::drawRect(RectF dst, RectF clip, video::SColor color)
	{
		if (dst.intersectWith(clip).empty() || color.getAlpha() == 0) {
			return;
		}

		core::recti scaled_clip = clip * getScale();

		RenderingEngine::get_video_driver()->draw2DRectangle(
			color, dst * getScale(), &scaled_clip);
	}

	void Window::drawTexture(RectF dst, RectF clip, video::ITexture *texture,
			RectF src, video::SColor tint)
	{
		if (dst.intersectWith(clip).empty() ||
				texture == nullptr || tint.getAlpha() == 0) {
			return;
		}

		core::recti scaled_clip = clip * getScale();
		video::SColor colors[] = {tint, tint, tint, tint};

		RenderingEngine::get_video_driver()->draw2DImage(texture, dst * getScale(),
			src * DispF(getTextureSize(texture)), &scaled_clip, colors, true);
	}

	void Window::drawAll()
	{
		Box &backdrop = m_root_elem->getBackdrop();

		// Since the elements, screen size, pixel size, or style properties
		// might have changed since the last frame, we need to recompute stuff
		// before drawing: restyle all the boxes, recompute the base sizes from
		// the leaves to the root, and then layout each element in the element
		// tree from the root to the leaves.
		backdrop.restyle();
		backdrop.resize();

		RectF layout_rect(getScreenSize());
		backdrop.relayout(layout_rect, layout_rect);

		// Draw all of the newly layouted and updated elements.
		backdrop.draw();
	}

	void Window::readElems(std::istream &is,
			std::unordered_map<Elem *, std::string> &elem_contents)
	{
		// Read in all the new elements and updates to existing elements.
		u32 num_elems = readU32(is);

		std::unordered_map<std::string, std::unique_ptr<Elem>> new_elems;

		for (size_t i = 0; i < num_elems; i++) {
			u32 type = readU8(is);
			std::string id = readNullStr(is);

			// Make sure that elements have non-empty IDs since that indicates
			// a nonexistent element in getElem(). If the string has non-ID
			// characters in it, though, we don't particularly care.
			if (id.empty()) {
				errorstream << "Element has empty ID" << std::endl;
				continue;
			}

			// Each element has a size prefix stating how big the element is.
			// This allows new fields to be added to elements without breaking
			// compatibility. So, read it in as a string and save it for later.
			std::string contents = readStr32(is);

			// If this is a duplicate element, skip it right away.
			if (new_elems.find(id) != new_elems.end()) {
				errorstream << "Duplicate element \"" << id << "\"" << std::endl;
				continue;
			}

			/* Now we need to decide whether to create a new element or to
			 * modify the state of an already existing one. This allows
			 * changing attributes of an element (like the style or the
			 * element's children) while leaving leaving persistent state
			 * intact (such as the position of a scrollbar or the contents of a
			 * text field).
			 */
			std::unique_ptr<Elem> elem = nullptr;

			// Search for a pre-existing element.
			auto it = m_elems.find(id);

			if (it == m_elems.end() || it->second->getType() != type) {
				// If the element was not found or the existing element has the
				// wrong type, create a new element.
				elem = Elem::create((Elem::Type)type, *this, id);

				// If we couldn't create the element, the type was invalid.
				// Skip this element entirely.
				if (elem == nullptr) {
					errorstream << "Element \"" << id << "\" has an invalid type: " <<
						type << std::endl;
					continue;
				}
			} else {
				// Otherwise, use the existing element.
				elem = std::move(it->second);
			}

			// Now that we've gotten our element, reset its contents.
			elem->reset();

			// We need to read in all elements before updating each element, so
			// save the element's contents for later.
			elem_contents[elem.get()] = contents;
			new_elems.emplace(id, std::move(elem));
		}

		// Set these elements as our list of new elements.
		m_elems = std::move(new_elems);

		// Clear the ordered elements for now. They will be regenerated later.
		m_ordered_elems.clear();
	}

	bool Window::readRootElem(std::istream &is)
	{
		// Get the root element of the window and make sure it's valid.
		Elem *root = getElem(readNullStr(is), true);

		if (root == nullptr) {
			errorstream << "Window " << m_id << " has no root element" << std::endl;
			return false;
		} else if (root->getType() != Elem::ROOT) {
			errorstream << "Window " << m_id <<
				" has wrong type for root element" << std::endl;
			return false;
		}

		m_root_elem = static_cast<Root *>(root);
		return true;
	}

	void Window::readStyles(std::istream &is)
	{
		// Styles are stored in their raw binary form; every time a style needs
		// to be recalculated, these binary strings can be applied one over the
		// other, resulting in automatic cascading styles.
		u32 num_styles = readU32(is);
		m_style_strs.clear();

		for (size_t i = 0; i < num_styles; i++) {
			m_style_strs.push_back(readStr16(is));
		}
	}

	bool Window::updateElems(std::unordered_map<Elem *, std::string> &elem_contents)
	{
		// Now that we have a fully updated window, we can update each element
		// with its contents and set up the parent-child relations. We couldn't
		// do this before because elements need to be able to call getElem()
		// and getStyleStr().
		for (auto &contents : elem_contents) {
			auto is = newIs(contents.second);
			contents.first->read(is);
		}

		// Check the depth of the element tree; if it's too deep, there's
		// potential for stack overflow. We also create the list of ordered
		// elements since we're already doing a preorder traversal.
		if (!updateTree(m_root_elem, 1)) {
			return false;
		}

		// If the number of elements discovered by the tree traversal is less
		// than the total number of elements, orphaned elements must exist.
		if (m_elems.size() != m_ordered_elems.size()) {
			errorstream << "Window " << m_id << " has orphaned elements" << std::endl;
			return false;
		}
		return true;
	}

	bool Window::updateTree(Elem *elem, size_t depth)
	{
		// The parent gets ordered before its children since the ordering of
		// elements follows draw order.
		elem->setOrder(m_ordered_elems.size());
		m_ordered_elems.push_back(elem);

		if (depth > MAX_TREE_DEPTH) {
			errorstream << "Window " << m_id <<
					" exceeds the max tree depth of " << MAX_TREE_DEPTH << std::endl;
			return false;
		}

		for (Elem *child : elem->getChildren()) {
			if (child->getType() == Elem::ROOT) {
				errorstream << "Element of root type \"" << child->getId() <<
					"\" is not root of window" << std::endl;
				return false;
			}

			if (!updateTree(child, depth + 1)) {
				return false;
			}
		}

		return true;
	}
}
