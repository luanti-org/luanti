// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#include "ui/box.h"

#include "debug.h"
#include "log.h"
#include "porting.h"
#include "client/fontengine.h"
#include "ui/elem.h"
#include "ui/manager.h"
#include "ui/window.h"
#include "util/serialize.h"

#include <SDL2/SDL.h>

namespace ui
{
	Window &Box::getWindow()
	{
		return m_elem.getWindow();
	}

	const Window &Box::getWindow() const
	{
		return m_elem.getWindow();
	}

	void Box::reset()
	{
		m_content.clear();
		m_label = "";

		m_style.reset();

		for (State i = 0; i < m_style_refs.size(); i++) {
			m_style_refs[i] = NO_STYLE;
		}

		m_text = L"";
		m_font = nullptr;
	}

	void Box::read(std::istream &full_is)
	{
		auto is = newIs(readStr16(full_is));
		u32 style_mask = readU32(is);

		for (State i = 0; i < m_style_refs.size(); i++) {
			// If we have a style for this state in the mask, add it to the
			// list of styles.
			if (!testShift(style_mask)) {
				continue;
			}

			u32 index = readU32(is);
			if (getWindow().getStyleStr(index) != nullptr) {
				m_style_refs[i] = index;
			} else {
				errorstream << "Style " << index << " does not exist" << std::endl;
			}
		}
	}

	void Box::restyle()
	{
		// First, clear our current style and compute what state we're in.
		m_style.reset();
		State state = STATE_NONE;

		if (m_elem.isBoxFocused(*this))
			state |= STATE_FOCUSED;
		if (m_elem.isBoxSelected(*this))
			state |= STATE_SELECTED;
		if (m_elem.isBoxHovered(*this))
			state |= STATE_HOVERED;
		if (m_elem.isBoxPressed(*this))
			state |= STATE_PRESSED;
		if (m_elem.isBoxDisabled(*this))
			state |= STATE_DISABLED;

		// Loop over each style state from lowest precedence to highest since
		// they should be applied in that order.
		for (State i = 0; i < m_style_refs.size(); i++) {
			// If this state we're looking at is a subset of the current state,
			// then it's a match for styling.
			if ((state & i) != i) {
				continue;
			}

			u32 index = m_style_refs[i];

			// If the index for this state has an associated style string,
			// apply it to our current style.
			if (index != NO_STYLE) {
				auto is = newIs(*getWindow().getStyleStr(index));
				m_style.read(is);
			}
		}

		// Now that we have updated text style properties, we can update our
		// cached text string and font object.
		m_text = utf8_to_wide(m_style.text.prepend) + utf8_to_wide(m_label) +
			utf8_to_wide(m_style.text.append);

		FontSpec spec(m_style.text.size, m_style.text.mono ? FM_Mono : FM_Standard,
			m_style.text.bold, m_style.text.italic);
		m_font = g_fontengine->getFont(spec);

		// Since our box has been restyled, the previously computed layout
		// information is no longer valid.
		m_img_src = RectF();
		m_img_border = DispF();

		m_min_layout = SizeF();
		m_min_content = SizeF();

		m_display_rect = RectF();
		m_content_rect = RectF();
		m_clip_rect = RectF();

		// Finally, make sure to restyle our content.
		for (Box *box : m_content) {
			box->restyle();
		}
	}

	void Box::resize()
	{
		for (Box *box : m_content) {
			box->resize();
		}

		switch (m_style.layout.type) {
		case LayoutType::PLACE:
			resizePlace();
			break;
		}

		resizeBox();
	}

	void Box::relayout(RectF layout_rect, RectF layout_clip)
	{
		relayoutBox(layout_rect, layout_clip);

		switch (m_style.layout.type) {
		case LayoutType::PLACE:
			relayoutPlace();
			break;
		}
	}

	void Box::draw()
	{
		if (!m_style.visual.hidden) {
			drawBox();
		}

		for (Box *box : m_content) {
			box->draw();
		}
	}

	bool Box::isPointed() const
	{
		return m_clip_rect.contains(getWindow().getPointerPos());
	}

	bool Box::isContentPointed() const {
		// If we're pointed, then we clearly have a pointed box.
		if (isPointed()) {
			return true;
		}

		// Search through our content. If any of them are contained within the
		// same element as this box, they are candidates for being pointed.
		for (Box *box : m_content) {
			if (&box->getElem() == &m_elem && box->isContentPointed()) {
				return true;
			}
		}

		return false;
	}

	bool Box::processInput(const SDL_Event &event)
	{
		switch (event.type) {
		case UI_USER(FOCUS_REQUEST):
			// The box is dynamic, so it can be focused.
			return true;

		case UI_USER(FOCUS_CHANGED):
			// If the box is no longer focused, it can't be pressed.
			if (event.user.data1 == &m_elem) {
				setPressed(false);
			}
			return false;

		case UI_USER(FOCUS_SUBVERTED):
			// If some non-focused element used an event instead of this one,
			// unpress the box because user interaction has been diverted.
			setPressed(false);
			return false;

		case UI_USER(HOVER_REQUEST):
			// The box can be hovered if the pointer is inside it.
			return isPointed();

		case UI_USER(HOVER_CHANGED):
			// Make this box hovered if the element became hovered and the
			// pointer is inside this box.
			setHovered(event.user.data2 == &m_elem && isPointed());
			return true;

		default:
			return false;
		}
	}

	bool Box::processFullPress(const SDL_Event &event, void (*on_press)(Elem &))
	{
		switch (event.type) {
		case SDL_KEYDOWN:
			// If the space key is pressed not due to a key repeat, then the
			// box becomes pressed. If the escape key is pressed while the box
			// is pressed, that unpresses the box without triggering it.
			if (event.key.keysym.sym == SDLK_SPACE && !event.key.repeat) {
				setPressed(true);
				return true;
			} else if (event.key.keysym.sym == SDLK_ESCAPE && isPressed()) {
				setPressed(false);
				return true;
			}
			return false;

		case SDL_KEYUP:
			// Releasing the space key while the box is pressed causes it to be
			// unpressed and triggered.
			if (event.key.keysym.sym == SDLK_SPACE && isPressed()) {
				setPressed(false);
				on_press(m_elem);
				return true;
			}
			return false;

		case SDL_MOUSEBUTTONDOWN:
			// If the box is hovered, then pressing the left mouse button
			// causes it to be pressed. Otherwise, the mouse is directed at
			// some other box.
			if (isHovered() && event.button.button == SDL_BUTTON_LEFT) {
				setPressed(true);
				return true;
			}
			return false;

		case SDL_MOUSEBUTTONUP:
			// If the mouse button was released, the box becomes unpressed. If
			// it was released while inside the bounds of the box, that counts
			// as the box being triggered.
			if (event.button.button == SDL_BUTTON_LEFT) {
				bool was_pressed = isPressed();
				setPressed(false);

				if (isHovered() && was_pressed) {
					on_press(m_elem);
					return true;
				}
			}
			return false;

		default:
			return processInput(event);
		}
	}

	void Box::resizeBox()
	{
		// Calculate the normalized source rect for the image. If we have
		// animations, we need to adjust the slice rect by the frame offset in
		// accordance with the current frame.
		m_img_src = m_style.img.slice;

		if (m_style.img.frames > 1) {
			float frame_height = m_img_src.H() / m_style.img.frames;
			m_img_src.B = m_img_src.T + frame_height;

			s32 frames_elapsed = (porting::getTimeMs() / m_style.img.frame_time);
			float frame_offset = frame_height * (frames_elapsed % m_style.img.frames);

			m_img_src.T += frame_offset;
			m_img_src.B += frame_offset;
		}

		// Next, we need to figure out the borders of the pane created by the
		// border rect so we can apply it for layout purposes alongside padding
		// and margins. So, scale the border rect by the scaling factor and
		// de-normalize it into actual pixels based on the image source rect.
		SizeF pane_size = m_img_src.size() *
			getTextureSize(m_style.img.pane) * m_style.img.scale;
		m_img_border = m_style.img.border * DispF(pane_size);

		// Now we can finalize the minimum size of the box. First, we
		// potentially need to expand the box to accommodate the size of the
		// overlay as well as any text it might contain.
		SizeF overlay_size = m_img_src.size() *
			getTextureSize(m_style.img.overlay) * m_style.img.scale;
		SizeF text_size = getWindow().getTextSize(m_font, m_text);

		m_min_content = m_min_content.max(overlay_size).max(text_size);

		// If the box is set to clip its contents in either dimension, we can
		// set the minimum content size to zero for that coordinate.
		if (m_style.layout.truncate == DirFlags::X ||
				m_style.layout.truncate == DirFlags::BOTH) {
			m_min_content.W = 0.0f;
		}
		if (m_style.layout.truncate == DirFlags::Y ||
				m_style.layout.truncate == DirFlags::BOTH) {
			m_min_content.H = 0.0f;
		}

		// Now that we have a minimum size for the padding rect, we can
		// calculate the display rect size by adjusting for the padding and
		// borders. We also clamp the size of the display rect to be at least
		// as large as the user-specified minimum size.
		SizeF display_size = (m_min_content + m_style.sizing.padding.extents() +
			m_img_border.extents()).max(m_style.sizing.min);

		// The final minimum size is the display size adjusted for the margin.
		m_min_layout = (display_size + m_style.sizing.margin.extents()).clip();
	}

	void Box::relayoutBox(RectF layout_rect, RectF layout_clip)
	{
		// The display rect is created by insetting the layout rect by the
		// margin. The padding rect is inset from that by the borders and the
		// padding. We must make sure these do not have negative sizes.
		m_display_rect = layout_rect.insetBy(m_style.sizing.margin).clip();
		m_content_rect = m_display_rect.insetBy(
			m_img_border + m_style.sizing.padding).clip();

		switch (m_style.visual.clip) {
		case ClipMode::NORMAL:
			// If the box is visible, then we clip the box and its contents as
			// normal against the drawing and layout clip rects.
			m_clip_rect = m_display_rect.intersectWith(layout_clip);
			break;
		case ClipMode::OVERFLOW:
			// If the box allows overflow, then clip to the drawing rect, since
			// we never want to expand outside our own visible boundaries.
			m_clip_rect = m_display_rect;
			break;
		case ClipMode::COMPLETE:
			// If the box and its content should be entirely clipped away, then
			// we set the clip rect to an empty rect.
			m_clip_rect = RectF();
			break;
		}
	}

	void Box::resizePlace()
	{
		for (Box *box : m_content) {
			// Calculate the size of the box's layout rect according to the
			// size and scale properties. If the scale is zero, we don't know
			// how big the rect will end up being, so the size goes to zero.
			SizeF layout_size = box->m_style.sizing.size * m_style.layout.scale;

			// Ensure that the computed minimum size for our content is at
			// least as large as the minimum size of the box and the computed
			// size of the layout rect.
			m_min_content = m_min_content.max(box->m_min_layout).max(layout_size);
		}
	}

	void Box::relayoutPlace()
	{
		for (Box *box : m_content) {
			const SizingProps &sizing = box->m_style.sizing;

			// Compute the scale factor. If the scale is zero, then we use the
			// size of the parent box to achieve normalized coordinates.
			SizeF scale = m_style.layout.scale == 0.0f ?
				m_content_rect.size() : SizeF(m_style.layout.scale);

			// Calculate the position and size of the box relative to the
			// origin, taking into account the scale factor and anchor. Also
			// make sure the size doesn't go below the minimum size.
			SizeF size = (sizing.size * scale).max(box->m_min_layout);
			SizeF pos = (sizing.pos * scale) - (sizing.anchor * size);

			// The layout rect of the box is made by shifting the above rect by
			// the top left of the content rect.
			RectF layout_rect = RectF(m_content_rect.TopLeft + pos, size);
			box->relayout(layout_rect, m_clip_rect);
		}
	}

	void Box::drawBox()
	{
		// First, fill the display rect with the fill color.
		getWindow().drawRect(m_display_rect, m_clip_rect, m_style.visual.fill);

		// If there's no pane or overlay layers, then we don't need to do a
		// bunch of calculations in order to draw nothing.
		if (m_style.img.pane != nullptr) {
			drawPane();
		}
		if (m_style.img.overlay != nullptr) {
			drawOverlay();
		}

		// The window handles all the complicated text layout, so we can just
		// draw the text with all the appropriate styling.
		getWindow().drawText(m_content_rect, m_clip_rect, m_font, m_text,
			m_style.text.color, m_style.text.mark,
			m_style.text.align, m_style.text.valign);
	}

	void Box::drawPane()
	{
		// We need to make sure the the border rect is relative to the source
		// rect rather than the entire pane, so scale the edges appropriately.
		DispF border_src = m_style.img.border * DispF(m_img_src.size());
		DispF border_dst = m_img_border;

		// If the source rect for this pane is flipped, we need to flip the
		// sign of our border rect as well to get the right adjustments.
		if (m_img_src.W() < 0.0f) {
			border_src.L = -border_src.L;
			border_src.R = -border_src.R;
		}
		if (m_img_src.H() < 0.0f) {
			border_src.T = -border_src.T;
			border_src.B = -border_src.B;
		}

		for (int slice_y = 0; slice_y < 3; slice_y++) {
			for (int slice_x = 0; slice_x < 3; slice_x++) {
				// Compute each slice of the nine-slice texture. If the border
				// rect equals the whole source rect, the middle slice will
				// occupy the entire display rect.
				RectF slice_src = m_img_src;
				RectF slice_dst = m_display_rect;

				switch (slice_x) {
				case 0:
					slice_dst.R = slice_dst.L + border_dst.L;
					slice_src.R = slice_src.L + border_src.L;
					break;

				case 1:
					slice_dst.L += border_dst.L;
					slice_dst.R -= border_dst.R;
					slice_src.L += border_src.L;
					slice_src.R -= border_src.R;
					break;

				case 2:
					slice_dst.L = slice_dst.R - border_dst.R;
					slice_src.L = slice_src.R - border_src.R;
					break;
				}

				switch (slice_y) {
				case 0:
					slice_dst.B = slice_dst.T + border_dst.T;
					slice_src.B = slice_src.T + border_src.T;
					break;

				case 1:
					slice_dst.T += border_dst.T;
					slice_dst.B -= border_dst.B;
					slice_src.T += border_src.T;
					slice_src.B -= border_src.B;
					break;

				case 2:
					slice_dst.T = slice_dst.B - border_dst.B;
					slice_src.T = slice_src.B - border_src.B;
					break;
				}

				// If we have a tiled pane, then some of the tiles may bleed
				// out of the slice rect, so we need to clip to both the
				// clipping rect and the destination rect.
				RectF slice_clip = m_clip_rect.intersectWith(slice_dst);

				// If this slice is empty or has been entirely clipped, then
				// don't bother drawing anything.
				if (slice_clip.empty()) {
					continue;
				}

				// This may be a tiled pane, so we need to calculate the size
				// of each tile. If the pane is not tiled, this should equal
				// the size of the destination rect.
				SizeF tile_size = slice_dst.size();

				if (m_style.img.tile != DirFlags::NONE) {
					// We need to calculate the tile size based on the texture
					// size and the scale of each tile. If the scale is too
					// small, then the number of tiles will explode, so we
					// clamp it to a reasonable minimum of 1/8 of a pixel.
					SizeF tex_size = getTextureSize(m_style.img.pane);
					float tile_scale = std::max(m_style.img.scale, 0.125f);

					if (m_style.img.tile != DirFlags::Y) {
						tile_size.W = slice_src.W() * tex_size.W * tile_scale;
					}
					if (m_style.img.tile != DirFlags::X) {
						tile_size.H = slice_src.H() * tex_size.H * tile_scale;
					}
				}

				// Now we can draw each tile for this slice. If the pane is not
				// tiled, then each of these loops will run only once.
				float tile_y = slice_dst.T;

				while (tile_y < slice_dst.B) {
					float tile_x = slice_dst.L;

					while (tile_x < slice_dst.R) {
						// Draw the texture in the appropriate destination rect
						// for this tile, and clip it to the clipping rect for
						// this slice.
						RectF tile_dst = RectF(PosF(tile_x, tile_y), tile_size);

						getWindow().drawTexture(tile_dst, slice_clip,
							m_style.img.pane, slice_src, m_style.img.tint);

						tile_x += tile_size.W;
					}
					tile_y += tile_size.H;
				}
			}
		}
	}

	void Box::drawOverlay()
	{
		RectF border_rect = m_display_rect.insetBy(m_img_border);

		// Calculate the size of the overlay rect based on the size of the
		// texture and the scale, and align this inside the border rect.
		SizeF size = m_img_src.size() *
			getTextureSize(m_style.img.overlay) * m_style.img.scale;
		PosF pos = m_style.img.align * (border_rect.size() - size);

		RectF overlay_dst = RectF(border_rect.TopLeft + SizeF(pos), size);

		// Otherwise, we just draw the overlay texture using the same source
		// rect and tint as the pane texture.
		getWindow().drawTexture(overlay_dst, m_clip_rect,
			m_style.img.overlay, m_img_src, m_style.img.tint);
	}

	bool Box::isHovered() const
	{
		return m_elem.getHoveredBox() == getId();
	}

	bool Box::isPressed() const
	{
		return m_elem.getPressedBox() == getId();
	}

	void Box::setHovered(bool hovered)
	{
		if (hovered) {
			m_elem.setHoveredBox(getId());
		} else if (isHovered()) {
			m_elem.setHoveredBox(NO_ID);
		}
	}

	void Box::setPressed(bool pressed)
	{
		if (pressed) {
			m_elem.setPressedBox(getId());
		} else if (isPressed()) {
			m_elem.setPressedBox(NO_ID);
		}
	}
}
