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

#include <SDL2/SDL.h>

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

	Elem *Window::getNextElem(Elem *elem, bool reverse)
	{
		size_t next = elem->getOrder();
		size_t last = m_ordered_elems.size() - 1;

		if (!reverse) {
			next = (next == last) ? 0 : next + 1;
		} else {
			next = (next == 0) ? last : next - 1;
		}

		return m_ordered_elems[next];
	}

	void Window::clearElem(Elem *elem)
	{
		if (m_focused_elem == elem) {
			m_focused_elem = nullptr;
		}
		if (m_hovered_elem == elem) {
			m_hovered_elem = nullptr;
		}
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

		m_focused_elem = nullptr;
		m_hovered_elem = nullptr;

		m_allow_close = true;
		m_events = 0;
	}

	bool Window::read(std::istream &is, bool opening)
	{
		// Read in all the fundamental properties that must be unconditionally
		// provided for the window.
		std::unordered_map<Elem *, std::string> elem_contents;
		readElems(is, elem_contents);

		if (!readRootElem(is)) {
			return false;
		}

		readStyles(is);

		if (opening) {
			m_type = toWindowType(readU8(is));
		}

		// After the unconditional properties, read the conditional ones.
		u32 set_mask = readU32(is);

		bool set_focus = false;
		Elem *new_focused = nullptr;

		if (testShift(set_mask)) {
			new_focused = getElem(readNullStr(is), false);
			set_focus = true;
		}
		if (opening) {
			m_allow_close = testShift(set_mask);
		} else {
			testShift(set_mask);
		}

		if (testShift(set_mask))
			enableEvent(ON_SUBMIT);
		if (testShift(set_mask))
			enableEvent(ON_FOCUS_CHANGE);

		return updateElems(elem_contents, set_focus, new_focused);
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

	PosF Window::getPointerPos() const
	{
		int x, y;
		SDL_GetMouseState(&x, &y);

		return PosF(x, y) / getScale();
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

		// Find the current hovered element, which will be nothing if we're not
		// focused. This may have changed between draw calls due to window
		// resizes or element layouting.
		hoverPointedElem();

		// If this window isn't focused, tell the currently focused element.
		if (!isFocused()) {
			SDL_Event notice = createUiEvent(UI_FOCUS_SUBVERTED);
			sendTreeInput(m_focused_elem, notice, true);
		}

		// Draw all of the newly layouted and updated elements.
		backdrop.draw();
	}

	bool Window::isFocused() const
	{
		return g_manager.isFocused() && g_manager.getFocused() == this;
	}

	bool Window::processInput(const SDL_Event &event)
	{
		switch (event.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP: {
			// Send the keypresses off to the focused element for processing.
			// The hovered element never gets keypresses.
			if (sendFocusedInput(event) != nullptr) {
				return true;
			}

			if (event.type == SDL_KEYDOWN) {
				u16 mod = event.key.keysym.mod;

				switch (event.key.keysym.sym) {
				case SDLK_ESCAPE:
					// If we got an Escape keypress, close the window.
					destroyWindow();
					return true;

				case SDLK_RETURN:
				case SDLK_RETURN2:
				case SDLK_KP_ENTER:
					// If the Enter key was pressed but not handled by any
					// elements, send a submit event to the server.
					if (testEvent(ON_SUBMIT)) {
						g_manager.sendMessage(createEvent(ON_SUBMIT).str());
					}
					return true;

				case SDLK_TAB:
				case SDLK_KP_TAB:
					// If we got a Tab key press, but not a Ctrl + Tab (which
					// is reserved for use by elements), focus the next
					// element, or the previous element if Shift is pressed.
					if (!(mod & KMOD_CTRL)) {
						focusNextElem(mod & KMOD_SHIFT);
						return true;
					}
					break;

				default:
					break;
				}
			}
			return false;
		}

		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEWHEEL: {
			// Make sure that we have an updated hovered element so that the
			// hovered element is the one that receives the mouse motion event.
			if (event.type == SDL_MOUSEMOTION) {
				hoverPointedElem();
			}

			// If we just clicked with the left mouse button, see if there's
			// any element at that position to focus.
			if (event.type == SDL_MOUSEBUTTONDOWN &&
					event.button.button == SDL_BUTTON_LEFT) {
				if (isPointerOutside()) {
					changeFocusedElem(nullptr, true);
				} else {
					focusPointedElem();
				}
			}

			// First, give the focused element a chance to see the mouse event,
			// so it can e.g. unpress a button if a mouse button was released.
			if (sendFocusedInput(event) != nullptr) {
				return true;
			}

			// Then, send the mouse input to the hovered element.
			if (sendPointedInput(event) != nullptr) {
				return true;
			}
			return false;
		}

		default:
			return false;
		}
	}

	void Window::enableEvent(u32 event)
	{
		m_events |= (1 << event);
	}

	bool Window::testEvent(u32 event) const
	{
		return m_events & (1 << event);
	}

	std::ostringstream Window::createEvent(u32 event) const
	{
		auto os = newOs();

		writeU8(os, Manager::WINDOW_EVENT);
		writeU64(os, m_id);
		writeU8(os, event);

		return os;
	}

	void Window::destroyWindow()
	{
		if (m_allow_close) {
			// Always send the close event so the server can update its
			// internal window tables, even if there's no on_close() handler.
			g_manager.sendMessage(createEvent(ON_CLOSE).str());

			// This causes the window object to be destroyed. Do not run any
			// code after this!
			g_manager.removeWindow(m_id);
		}
	}

	Elem *Window::sendTreeInput(Elem *elem, const SDL_Event &event, bool direct)
	{
		// Give the event to the element and all its parents for processing.
		while (elem != nullptr) {
			bool handled = elem->processInput(event);

			if (handled) {
				// If we handled the event, return the element that handled it.
				return elem;
			} else if (direct) {
				// If this event is only intended directly for this element and
				// it didn't handle the event, then we're done.
				return nullptr;
			}

			// Otherwise, give the parent a chance to handle it.
			elem = elem->getParent();
		}
		return nullptr;
	}

	Elem *Window::sendPointedInput(const SDL_Event &event)
	{
		// We want to get the topmost hovered element, so we have to iterate in
		// reverse draw order and check each element the mouse is inside.
		for (size_t i = m_ordered_elems.size(); i > 0; i--) {
			Elem *elem = m_ordered_elems[i - 1];
			if (elem->getMain().isContentPointed() && elem->processInput(event)) {
				return elem;
			}
		}

		return nullptr;
	}

	Elem *Window::sendFocusedInput(const SDL_Event &event)
	{
		if (m_focused_elem == nullptr) {
			return nullptr;
		}

		// Send the event to the focused element and its parents.
		Elem *handled = sendTreeInput(m_focused_elem, event, false);

		// If one of the focused element's parents handled the event, let the
		// focused element know that focus was subverted.
		if (handled != nullptr && handled != m_focused_elem) {
			SDL_Event notice = createUiEvent(UI_FOCUS_SUBVERTED);
			sendTreeInput(m_focused_elem, notice, true);
		}

		return handled;
	}

	void Window::changeFocusedElem(Elem *new_focused, bool send_event)
	{
		// If the same element is being focused, do nothing.
		if (new_focused == m_focused_elem) {
			return;
		}

		Elem *old_focused = m_focused_elem;
		m_focused_elem = new_focused;

		// Let the old and new focused elements know that things have
		// changed, and their parent elements too.
		SDL_Event notice = createUiEvent(UI_FOCUS_CHANGED, old_focused, new_focused);

		sendTreeInput(old_focused, notice, false);
		sendTreeInput(new_focused, notice, false);

		// If the server wants to know when focus changes, send it an event.
		if (send_event && testEvent(ON_FOCUS_CHANGE)) {
			auto os = createEvent(ON_FOCUS_CHANGE);

			// If either the old or the new element was unfocused, send an
			// empty string. Otherwise, send the ID of the element.
			writeNullStr(os, old_focused == nullptr ? "" : old_focused->getId());
			writeNullStr(os, new_focused == nullptr ? "" : new_focused->getId());

			g_manager.sendMessage(os.str());
		}
	}

	bool Window::requestFocusedElem(Elem *new_focused, bool send_event)
	{
		// If this element is already focused, we don't need to do anything.
		if (new_focused == m_focused_elem) {
			return m_focused_elem;
		}

		SDL_Event notice = createUiEvent(UI_FOCUS_REQUEST);

		// Ask the new element if it can take user focus. If it can, make it
		// the focused element.
		if (sendTreeInput(new_focused, notice, true) == new_focused) {
			changeFocusedElem(new_focused, send_event);
			return true;
		}
		return false;
	}

	void Window::focusNextElem(bool reverse)
	{
		// Start tabbing from the focused element if there is one, or the root
		// element otherwise.
		Elem *start = m_focused_elem != nullptr ? m_focused_elem : m_root_elem;

		// Loop through all the elements in order (not including the starting
		// element), trying to focus them, until we reach the place we started
		// again, which means that no element wanted to be focused.
		Elem *next = getNextElem(start, reverse);

		while (next != start) {
			if (requestFocusedElem(next, true)) {
				return;
			}

			next = getNextElem(next, reverse);
		}
	}

	void Window::focusPointedElem()
	{
		SDL_Event notice = createUiEvent(UI_FOCUS_REQUEST);

		// Ask all elements that the mouse just clicked on if they want to
		// be the focused element.
		Elem *new_focused = sendPointedInput(notice);

		// If an element responded to the request that is different from the
		// currently focused element, then update the focused element.
		if (new_focused != nullptr && m_focused_elem != new_focused) {
			changeFocusedElem(new_focused, true);
		}
	}

	void Window::hoverPointedElem()
	{
		SDL_Event notice = createUiEvent(UI_HOVER_REQUEST);

		// If the window is focused, ask all elements that the mouse is
		// currently inside if they want to be the hovered element. Otherwise,
		// make no element hovered.
		Elem *old_hovered = m_hovered_elem;
		Elem *new_hovered = nullptr;

		if (isFocused()) {
			new_hovered = sendPointedInput(notice);
		}

		// If a different element responded to the hover request (or no element
		// at all), then update the hovered element.
		if (old_hovered != new_hovered) {
			m_hovered_elem = new_hovered;

			// Let the old and new hovered elements know that things have
			// changed, and their parent elements too.
			notice = createUiEvent(UI_HOVER_CHANGED, old_hovered, new_hovered);

			sendTreeInput(old_hovered, notice, false);
			sendTreeInput(new_hovered, notice, false);
		}
	}

	bool Window::isPointerOutside() const
	{
		// If the mouse is inside any element, it's not outside the window. We
		// have to check every element, not just the root, because elements may
		// have the noclip property set. However, the backdrop is not included.
		for (Elem *elem : m_ordered_elems) {
			if (elem->getMain().isContentPointed()) {
				return false;
			}
		}

		return true;
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

	bool Window::updateElems(std::unordered_map<Elem *, std::string> &elem_contents,
			bool set_focus, Elem *new_focused)
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

		// If the user wants to focus a new element or unfocus the current
		// element, remove focus from the current element and request focus on
		// the new element.
		if (set_focus && new_focused != m_focused_elem) {
			if (new_focused != nullptr) {
				if (!requestFocusedElem(new_focused, false)) {
					changeFocusedElem(nullptr, false);
				}
			} else {
				changeFocusedElem(nullptr, false);
			}
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
