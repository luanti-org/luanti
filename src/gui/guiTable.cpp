// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>


#include "guiTable.h"
#include <queue>
#include <sstream>
#include <utility>
#include <cstring>
#include <IGUISkin.h>
#include <IGUIFont.h>
#include "client/renderingengine.h"
#include "debug.h"
#include "irrlicht_changes/CGUITTFont.h"
#include "log.h"
#include "client/texturesource.h"
#include "gettime.h"
#include "util/string.h"
#include "util/numeric.h"
#include "util/string.h" // for parseColorString()
#include "settings.h" // for settings
#include "porting.h" // for dpi
#include "client/guiscalingfilter.h"

/*
	GUITable
*/

GUITable::GUITable(gui::IGUIEnvironment *env,
		gui::IGUIElement* parent, s32 id,
		core::rect<s32> rectangle,
		ISimpleTextureSource *tsrc
):
	gui::IGUIElement(gui::EGUIET_TABLE, env, parent, id, rectangle),
	m_tsrc(tsrc)
{
	assert(tsrc != NULL);

	gui::IGUISkin* skin = Environment->getSkin();

	m_font = skin->getFont();
	if (m_font) {
		m_font->grab();
		m_rowheight = m_font->getDimension(L"Ay").Height + 4;
		m_rowheight = MYMAX(m_rowheight, 1);
	}

	const s32 s = skin->getSize(gui::EGDS_SCROLLBAR_SIZE);
	m_scrollbar = new GUIScrollBar(Environment, this, -1,
			core::rect<s32>(RelativeRect.getWidth() - s,
					0,
					RelativeRect.getWidth(),
					RelativeRect.getHeight()),
			false, true, tsrc);
	m_scrollbar->setSubElement(true);
	m_scrollbar->setTabStop(false);
	m_scrollbar->setAlignment(gui::EGUIA_LOWERRIGHT, gui::EGUIA_LOWERRIGHT,
			gui::EGUIA_UPPERLEFT, gui::EGUIA_LOWERRIGHT);
	m_scrollbar->setVisible(false);
	m_scrollbar->setPos(0);

	setTabStop(true);
	setTabOrder(-1);
	updateAbsolutePosition();
}

GUITable::~GUITable()
{
	for (GUITable::Row &row : m_rows)
		delete[] row.cells;

	if (m_font)
		m_font->drop();

	if (m_scrollbar)
		m_scrollbar->drop();
}

GUITable::Option GUITable::splitOption(const std::string &str)
{
	size_t equal_pos = str.find('=');
	if (equal_pos == std::string::npos)
		return GUITable::Option(str, "");

	return GUITable::Option(str.substr(0, equal_pos),
			str.substr(equal_pos + 1));
}

void GUITable::setTextList(const std::vector<std::string> &content,
		bool transparent)
{
	clear();

	if (transparent) {
		m_background.setAlpha(0);
		m_border = false;
	}

	m_is_textlist = true;

	s32 empty_string_index = allocString("");

	m_rows.resize(content.size());
	for (s32 i = 0; i < (s32) content.size(); ++i) {
		Row *row = &m_rows[i];
		row->cells = new Cell[1];
		row->cellcount = 1;
		row->indent = 0;
		row->visible_index = i;
		m_visible_rows.push_back(i);

		Cell *cell = row->cells;
		cell->xmin = 0;
		cell->xmax = 0x7fff;  // something large enough
		cell->xpos = 6;
		cell->content_type = COLUMN_TYPE_TEXT;
		cell->content_index = empty_string_index;
		cell->tooltip_index = empty_string_index;
		cell->color.set(255, 255, 255, 255);
		cell->color_defined = false;
		cell->reported_column = 1;

		// parse row content (color)
		const std::string &s = content[i];
		if (s[0] == '#' && s[1] == '#') {
			// double # to escape
			cell->content_index = allocString(s.substr(2));
		}
		else if (s[0] == '#' && s.size() >= 7 &&
				parseColorString(
					s.substr(0,7), cell->color, false)) {
			// single # for color
			cell->color_defined = true;
			cell->content_index = allocString(s.substr(7));
		}
		else {
			// no #, just text
			cell->content_index = allocString(s);
		}

	}

	allocationComplete();

	// Clamp scroll bar position
	updateScrollBar();
}

void GUITable::setTable(const TableOptions &options,
		const TableColumns &columns,
		std::vector<std::string> &content)
{
	clear();

	// Naming conventions:
	// i is always a row index, 0-based
	// j is always a column index, 0-based
	// k is another index, for example an option index

	// Handle a stupid error case... (issue #1187)
	if (columns.empty()) {
		TableColumn text_column;
		text_column.type = "text";
		TableColumns new_columns;
		new_columns.push_back(text_column);
		setTable(options, new_columns, content);
		return;
	}

	// Handle table options
	s32 opendepth = 0;
	for (const Option &option : options) {
		const std::string &name = option.name;
		const std::string &value = option.value;
		if (name == "color")
			parseColorString(value, m_color, false);
		else if (name == "background")
			parseColorString(value, m_background, false);
		else if (name == "border")
			m_border = is_yes(value);
		else if (name == "highlight")
			parseColorString(value, m_highlight, false);
		else if (name == "highlight_text")
			parseColorString(value, m_highlight_text, false);
		else if (name == "opendepth")
			opendepth = stoi(value);
		else
			errorstream<<"Invalid table option: \""<<name<<"\""
				<<" (value=\""<<value<<"\")"<<std::endl;
	}

	// Get number of columns and rows
	// note: error case columns.size() == 0 was handled above
	s32 colcount = columns.size();
	assert(colcount >= 1);
	// rowcount = ceil(cellcount / colcount) but use integer arithmetic
	s32 rowcount = std::min(((u32)content.size() + colcount - 1) / colcount, (u32)S32_MAX);
	assert(rowcount >= 0);
	// Append empty strings to content if there is an incomplete row
	s32 cellcount = rowcount * colcount;
	while (content.size() < (u32) cellcount)
		content.emplace_back("");

	// Create temporary rows (for processing columns)
	struct TempRow {
		// Current horizontal position (may different between rows due
		// to indent/tree columns, or text/image columns with width<0)
		s32 x;
		// Tree indentation level
		s32 indent;
		// Next cell: Index into m_strings or m_images
		s32 content_index;
		// Next cell: Width in pixels
		s32 content_width;
		// Next cell: Image scale (only for "image" column type)
		f32 image_scale;
		// Vector of completed cells in this row
		std::vector<Cell> cells;
		// Stores colors and how long they last (maximum column index)
		std::vector<std::pair<video::SColor, s32> > colors;

		TempRow(): x(0), indent(0), content_index(0), content_width(0) {}
	};
	TempRow *rows = new TempRow[rowcount];

	CGUITTFont *ttfont = dynamic_cast<CGUITTFont *>(m_font);
	f32 desired_image_scale = 1.0f;
	if (ttfont) {
		// This gives us the effective font size, which is chosen taking display
		// density and gui_scaling into account.
		// Since row height scales with font size, this gives better results than
		// just using display density and gui_scaling when a non-standard font
		// size is used (e.g. Android default of 14).
		desired_image_scale = std::max(1.0f, ttfont->getFontSize() / 16.0f);
	}

	// Get em width. Pedantically speaking, the width of "M" is not
	// necessarily the same as the em width, but whatever, close enough.
	s32 em = 6;
	if (m_font)
		em = m_font->getDimension(L"M").Width;

	s32 default_tooltip_index = allocString("");

	std::map<s32, s32> active_image_indices;

	// Process content in column-major order
	for (s32 j = 0; j < colcount; ++j) {
		// Check column type
		ColumnType columntype = COLUMN_TYPE_TEXT;
		if (columns[j].type == "text")
			columntype = COLUMN_TYPE_TEXT;
		else if (columns[j].type == "image")
			columntype = COLUMN_TYPE_IMAGE;
		else if (columns[j].type == "color")
			columntype = COLUMN_TYPE_COLOR;
		else if (columns[j].type == "indent")
			columntype = COLUMN_TYPE_INDENT;
		else if (columns[j].type == "tree")
			columntype = COLUMN_TYPE_TREE;
		else
			errorstream<<"Invalid table column type: \""
				<<columns[j].type<<"\""<<std::endl;

		// Process column options
		s32 padding = myround(0.5 * em);
		s32 tooltip_index = default_tooltip_index;
		s32 align = 0;
		s32 width = 0;
		s32 span = colcount;

		if (columntype == COLUMN_TYPE_INDENT) {
			padding = 0; // default indent padding
		}
		if (columntype == COLUMN_TYPE_INDENT ||
				columntype == COLUMN_TYPE_TREE) {
			width = myround(em * 1.5); // default indent width
		}

		for (const Option &option : columns[j].options) {
			const std::string &name = option.name;
			const std::string &value = option.value;
			if (name == "padding")
				padding = myround(stof(value) * em);
			else if (name == "tooltip")
				tooltip_index = allocString(value);
			else if (name == "align" && value == "left")
				align = 0;
			else if (name == "align" && value == "center")
				align = 1;
			else if (name == "align" && value == "right")
				align = 2;
			else if (name == "align" && value == "inline")
				align = 3;
			else if (name == "width")
				width = myround(stof(value) * em);
			else if (name == "span" && columntype == COLUMN_TYPE_COLOR)
				span = stoi(value);
			else if (columntype == COLUMN_TYPE_IMAGE &&
					!name.empty() &&
					string_allowed(name, "0123456789")) {
				s32 content_index = allocImage(value);
				active_image_indices.insert(std::make_pair(
							stoi(name),
							content_index));
			}
			else {
				errorstream<<"Invalid table column option: \""<<name<<"\""
					<<" (value=\""<<value<<"\")"<<std::endl;
			}
		}

		// If current column type can use information from "color" columns,
		// find out which of those is currently active
		if (columntype == COLUMN_TYPE_TEXT) {
			for (s32 i = 0; i < rowcount; ++i) {
				TempRow *row = &rows[i];
				while (!row->colors.empty() && row->colors.back().second < j)
					row->colors.pop_back();
			}
		}

		// Make template for new cells
		Cell newcell;
		newcell.content_type = columntype;
		newcell.tooltip_index = tooltip_index;
		newcell.reported_column = j+1;

		if (columntype == COLUMN_TYPE_TEXT) {
			// Find right edge of column
			s32 xmax = 0;
			for (s32 i = 0; i < rowcount; ++i) {
				TempRow *row = &rows[i];
				row->content_index = allocString(content[i * colcount + j]);
				const core::stringw &text = m_strings[row->content_index];
				row->content_width = m_font ?
					m_font->getDimension(text.c_str()).Width : 0;
				row->content_width = MYMAX(row->content_width, width);
				s32 row_xmax = row->x + padding + row->content_width;
				xmax = MYMAX(xmax, row_xmax);
			}
			// Add a new cell (of text type) to each row
			for (s32 i = 0; i < rowcount; ++i) {
				newcell.xmin = rows[i].x + padding;
				alignContent(&newcell, xmax, rows[i].content_width, align);
				newcell.content_index = rows[i].content_index;
				newcell.color_defined = !rows[i].colors.empty();
				if (newcell.color_defined)
					newcell.color = rows[i].colors.back().first;
				rows[i].cells.push_back(newcell);
				rows[i].x = newcell.xmax;
			}
		}
		else if (columntype == COLUMN_TYPE_IMAGE) {
			// Find right edge of column
			s32 xmax = 0;
			for (s32 i = 0; i < rowcount; ++i) {
				TempRow *row = &rows[i];
				row->content_index = -1;

				// Find content_index. Image indices are defined in
				// column options so check active_image_indices.
				s32 image_index = stoi(content[i * colcount + j]);
				auto image_iter =active_image_indices.find(image_index);
				if (image_iter != active_image_indices.end())
					row->content_index = image_iter->second;

				// Get texture object (might be NULL)
				video::ITexture *image = NULL;
				if (row->content_index >= 0)
					image = m_images[row->content_index];

				row->image_scale = 1.0f;
				row->content_width = 0;
				if (image) {
					f32 max_image_scale = (f32)m_rowheight / (f32)image->getOriginalSize().Height;
					// Scale with display density and make sure it fits into the row
					row->image_scale = std::min(desired_image_scale, max_image_scale);
					// When upscaling, fractional factors would cause artifacts
					if (row->image_scale > 1.0f)
						row->image_scale = std::floor(row->image_scale);
					row->content_width = image->getOriginalSize().Width * row->image_scale;
				}
				// Get content width and update xmax
				row->content_width = MYMAX(row->content_width, width);
				s32 row_xmax = row->x + padding + row->content_width;
				xmax = MYMAX(xmax, row_xmax);
			}
			// Add a new cell (of image type) to each row
			for (s32 i = 0; i < rowcount; ++i) {
				newcell.xmin = rows[i].x + padding;
				alignContent(&newcell, xmax, rows[i].content_width, align);
				newcell.content_index = rows[i].content_index;
				newcell.image_scale = rows[i].image_scale;
				rows[i].cells.push_back(newcell);
				rows[i].x = newcell.xmax;
			}
			active_image_indices.clear();
		}
		else if (columntype == COLUMN_TYPE_COLOR) {
			for (s32 i = 0; i < rowcount; ++i) {
				video::SColor cellcolor(255, 255, 255, 255);
				if (parseColorString(content[i * colcount + j], cellcolor, true))
					rows[i].colors.emplace_back(cellcolor, j+span);
			}
		}
		else if (columntype == COLUMN_TYPE_INDENT ||
				columntype == COLUMN_TYPE_TREE) {
			// For column type "tree", reserve additional space for +/-
			// Also enable special processing for treeview-type tables
			s32 content_width = 0;
			if (columntype == COLUMN_TYPE_TREE) {
				content_width = m_font ? m_font->getDimension(L"+").Width : 0;
				m_has_tree_column = true;
			}
			// Add a new cell (of indent or tree type) to each row
			for (s32 i = 0; i < rowcount; ++i) {
				TempRow *row = &rows[i];

				s32 indentlevel = stoi(content[i * colcount + j]);
				indentlevel = MYMAX(indentlevel, 0);
				if (columntype == COLUMN_TYPE_TREE)
					row->indent = indentlevel;

				newcell.xmin = row->x + padding;
				newcell.xpos = newcell.xmin + indentlevel * width;
				newcell.xmax = newcell.xpos + content_width;
				newcell.content_index = 0;
				newcell.color_defined = !rows[i].colors.empty();
				if (newcell.color_defined)
					newcell.color = rows[i].colors.back().first;
				row->cells.push_back(newcell);
				row->x = newcell.xmax;
			}
		}
	}

	// Copy temporary rows to not so temporary rows
	if (rowcount >= 1) {
		m_rows.resize(rowcount);
		for (s32 i = 0; i < rowcount; ++i) {
			Row *row = &m_rows[i];
			row->cellcount = rows[i].cells.size();
			row->cells = new Cell[row->cellcount];
			memcpy((void*) row->cells, (void*) &rows[i].cells[0],
					row->cellcount * sizeof(Cell));
			row->indent = rows[i].indent;
			row->visible_index = i;
			m_visible_rows.push_back(i);
		}
	}

	if (m_has_tree_column) {
		// Treeview: convert tree to indent cells on leaf rows
		for (s32 i = 0; i < rowcount; ++i) {
			if (i == rowcount-1 || m_rows[i].indent >= m_rows[i+1].indent)
				for (s32 j = 0; j < m_rows[i].cellcount; ++j)
					if (m_rows[i].cells[j].content_type == COLUMN_TYPE_TREE)
						m_rows[i].cells[j].content_type = COLUMN_TYPE_INDENT;
		}

		// Treeview: close rows according to opendepth option
		std::set<s32> opened_trees;
		for (s32 i = 0; i < rowcount; ++i)
			if (m_rows[i].indent < opendepth)
				opened_trees.insert(i);
		setOpenedTrees(opened_trees);
	}

	// Delete temporary information used only during setTable()
	delete[] rows;
	allocationComplete();

	// Clamp scroll bar position
	updateScrollBar();
}

void GUITable::clear()
{
	// Clean up cells and rows
	for (GUITable::Row &row : m_rows)
		delete[] row.cells;
	m_rows.clear();
	m_visible_rows.clear();

	// Get colors from skin
	gui::IGUISkin *skin = Environment->getSkin();
	m_color          = skin->getColor(gui::EGDC_BUTTON_TEXT);
	m_background     = skin->getColor(gui::EGDC_3D_HIGH_LIGHT);
	m_highlight      = skin->getColor(gui::EGDC_HIGH_LIGHT);
	m_highlight_text = skin->getColor(gui::EGDC_HIGH_LIGHT_TEXT);

	// Reset members
	m_is_textlist = false;
	m_has_tree_column = false;
	m_selected = -1;
	m_sel_column = 0;
	m_sel_doubleclick = false;
	m_keynav_time = 0;
	m_keynav_buffer = L"";
	m_border = true;
	m_strings.clear();
	m_images.clear();
	m_alloc_strings.clear();
	m_alloc_images.clear();
}

std::string GUITable::checkEvent()
{
	s32 sel = getSelected();
	assert(sel >= 0);

	if (sel == 0) {
		return "INV";
	}

	std::ostringstream os(std::ios::binary);
	if (m_sel_doubleclick) {
		os<<"DCL:";
		m_sel_doubleclick = false;
	}
	else {
		os<<"CHG:";
	}
	os<<sel;
	if (!m_is_textlist) {
		os<<":"<<m_sel_column;
	}
	return os.str();
}

s32 GUITable::getSelected() const
{
	if (m_selected < 0)
		return 0;

	assert(m_selected >= 0 && m_selected < (s32) m_visible_rows.size());
	return m_visible_rows[m_selected] + 1;
}

void GUITable::setSelected(s32 index)
{
	s32 old_selected = m_selected;

	m_selected = -1;
	m_sel_column = 0;
	m_sel_doubleclick = false;

	--index; // Switch from 1-based indexing to 0-based indexing

	s32 rowcount = m_rows.size();
	if (rowcount == 0 || index < 0) {
		return;
	}

	if (index >= rowcount) {
		index = rowcount - 1;
	}

	// If the selected row is not visible, open its ancestors to make it visible
	bool selection_invisible = m_rows[index].visible_index < 0;
	if (selection_invisible) {
		std::set<s32> opened_trees;
		getOpenedTrees(opened_trees);
		s32 indent = m_rows[index].indent;
		for (s32 j = index - 1; j >= 0; --j) {
			if (m_rows[j].indent < indent) {
				opened_trees.insert(j);
				indent = m_rows[j].indent;
			}
		}
		setOpenedTrees(opened_trees);
	}

	if (index >= 0) {
		m_selected = m_rows[index].visible_index;
		assert(m_selected >= 0 && m_selected < (s32) m_visible_rows.size());
	}

	if (m_selected != old_selected || selection_invisible) {
		autoScroll();
	}
}

void GUITable::setOverrideFont(IGUIFont *font)
{
	if (m_font == font)
		return;

	if (font == nullptr)
		font = Environment->getSkin()->getFont();

	if (m_font)
		m_font->drop();

	m_font = font;
	m_font->grab();

	m_rowheight = m_font->getDimension(L"Ay").Height + 4;
	m_rowheight = MYMAX(m_rowheight, 1);

	updateScrollBar();
}

IGUIFont *GUITable::getOverrideFont() const
{
	return m_font;
}

GUITable::DynamicData GUITable::getDynamicData() const
{
	DynamicData dyndata;
	dyndata.selected = getSelected();
	dyndata.scrollpos = m_scrollbar->getPos();
	dyndata.keynav_time = m_keynav_time;
	dyndata.keynav_buffer = m_keynav_buffer;
	if (m_has_tree_column)
		getOpenedTrees(dyndata.opened_trees);
	return dyndata;
}

void GUITable::setDynamicData(const DynamicData &dyndata)
{
	if (m_has_tree_column)
		setOpenedTrees(dyndata.opened_trees);

	m_keynav_time = dyndata.keynav_time;
	m_keynav_buffer = dyndata.keynav_buffer;

	setSelected(dyndata.selected);
	m_sel_column = 0;
	m_sel_doubleclick = false;

	m_scrollbar->setPos(dyndata.scrollpos);
}

void GUITable::updateAbsolutePosition()
{
	IGUIElement::updateAbsolutePosition();
	updateScrollBar();
}

void GUITable::draw()
{
	if (!IsVisible)
		return;

	gui::IGUISkin *skin = Environment->getSkin();

	// draw background

	bool draw_background = m_background.getAlpha() > 0;
	if (m_border)
		skin->draw3DSunkenPane(this, m_background,
				true, draw_background,
				AbsoluteRect, &AbsoluteClippingRect);
	else if (draw_background)
		skin->draw2DRectangle(this, m_background,
				AbsoluteRect, &AbsoluteClippingRect);

	// get clipping rect

	core::rect<s32> client_clip(AbsoluteRect);
	client_clip.UpperLeftCorner.Y += 1;
	client_clip.UpperLeftCorner.X += 1;
	client_clip.LowerRightCorner.Y -= 1;
	client_clip.LowerRightCorner.X -= 1;
	if (m_scrollbar->isVisible()) {
		client_clip.LowerRightCorner.X =
				m_scrollbar->getAbsolutePosition().UpperLeftCorner.X;
	}
	client_clip.clipAgainst(AbsoluteClippingRect);

	// draw visible rows

	s32 scrollpos = m_scrollbar->getPos();
	s32 row_min = scrollpos / m_rowheight;
	s32 row_max = (scrollpos + AbsoluteRect.getHeight() - 1)
			/ m_rowheight + 1;
	row_max = MYMIN(row_max, (s32) m_visible_rows.size());

	core::rect<s32> row_rect(AbsoluteRect);
	if (m_scrollbar->isVisible())
		row_rect.LowerRightCorner.X -=
			skin->getSize(gui::EGDS_SCROLLBAR_SIZE);
	row_rect.UpperLeftCorner.Y += row_min * m_rowheight - scrollpos;
	row_rect.LowerRightCorner.Y = row_rect.UpperLeftCorner.Y + m_rowheight;

	for (s32 i = row_min; i < row_max; ++i) {
		Row *row = &m_rows[m_visible_rows[i]];
		bool is_sel = i == m_selected;
		video::SColor color = m_color;

		if (is_sel) {
			skin->draw2DRectangle(this, m_highlight, row_rect, &client_clip);
			color = m_highlight_text;
		}

		for (s32 j = 0; j < row->cellcount; ++j)
			drawCell(&row->cells[j], color, row_rect, client_clip);

		row_rect.UpperLeftCorner.Y += m_rowheight;
		row_rect.LowerRightCorner.Y += m_rowheight;
	}

	// Draw children
	IGUIElement::draw();
}

void GUITable::drawCell(const Cell *cell, video::SColor color,
		const core::rect<s32> &row_rect,
		const core::rect<s32> &client_clip)
{
	if ((cell->content_type == COLUMN_TYPE_TEXT)
			|| (cell->content_type == COLUMN_TYPE_TREE)) {

		core::rect<s32> text_rect = row_rect;
		text_rect.UpperLeftCorner.X = row_rect.UpperLeftCorner.X
				+ cell->xpos;
		text_rect.LowerRightCorner.X = row_rect.UpperLeftCorner.X
				+ cell->xmax;

		if (cell->color_defined)
			color = cell->color;

		if (m_font) {
			if (cell->content_type == COLUMN_TYPE_TEXT)
				m_font->draw(m_strings[cell->content_index],
						text_rect, color,
						false, true, &client_clip);
			else // tree
				m_font->draw(cell->content_index ? L"+" : L"-",
						text_rect, color,
						false, true, &client_clip);
		}
	}
	else if (cell->content_type == COLUMN_TYPE_IMAGE) {

		if (cell->content_index < 0)
			return;

		video::IVideoDriver *driver = Environment->getVideoDriver();
		video::ITexture *image = m_images[cell->content_index];

		if (image) {
			core::rect<s32> source_rect(
					core::position2d<s32>(0, 0),
					image->getOriginalSize());
			core::rect<s32> dest_rect(
					0, 0,
					image->getOriginalSize().Width * cell->image_scale,
					image->getOriginalSize().Height * cell->image_scale);
			dest_rect += row_rect.UpperLeftCorner + v2s32(cell->xpos, 0);

			s32 imgh = dest_rect.getHeight();
			s32 rowh = row_rect.getHeight();
			// Center vertically if needed
			if (imgh < rowh)
				dest_rect += v2s32(0, (rowh - imgh) / 2);

			driver->draw2DImage(image, dest_rect, source_rect,
					&client_clip, nullptr, true);
		}
	}
}

bool GUITable::OnEvent(const SEvent &event)
{
	if (!isEnabled())
		return IGUIElement::OnEvent(event);

	if (event.EventType == EET_KEY_INPUT_EVENT) {
		if (event.KeyInput.PressedDown && (
				event.KeyInput.Key == KEY_DOWN ||
				event.KeyInput.Key == KEY_UP   ||
				event.KeyInput.Key == KEY_HOME ||
				event.KeyInput.Key == KEY_END  ||
				event.KeyInput.Key == KEY_NEXT ||
				event.KeyInput.Key == KEY_PRIOR)) {
			s32 offset = 0;
			switch (event.KeyInput.Key) {
				case KEY_DOWN:
					offset = 1;
					break;
				case KEY_UP:
					offset = -1;
					break;
				case KEY_HOME:
					offset = - (s32) m_visible_rows.size();
					break;
				case KEY_END:
					offset = m_visible_rows.size();
					break;
				case KEY_NEXT:
					offset = AbsoluteRect.getHeight() / m_rowheight;
					break;
				case KEY_PRIOR:
					offset = - (s32) (AbsoluteRect.getHeight() / m_rowheight);
					break;
				default:
					break;
			}
			s32 old_selected = m_selected;
			s32 rowcount = m_visible_rows.size();
			if (rowcount != 0) {
				m_selected = rangelim(m_selected + offset, 0, rowcount-1);
				autoScroll();
			}

			if (m_selected != old_selected)
				sendTableEvent(0, false);

			return true;
		}

		if (event.KeyInput.PressedDown && (
				event.KeyInput.Key == KEY_LEFT ||
				event.KeyInput.Key == KEY_RIGHT)) {
			// Open/close subtree via keyboard
			if (m_selected >= 0) {
				int dir = event.KeyInput.Key == KEY_LEFT ? -1 : 1;
				toggleVisibleTree(m_selected, dir, true);
			}
			return true;
		}
		else if (!event.KeyInput.PressedDown && (
				event.KeyInput.Key == KEY_RETURN ||
				event.KeyInput.Key == KEY_SPACE)) {
			sendTableEvent(0, true);
			return true;
		}
		else if (event.KeyInput.Key == KEY_ESCAPE ||
				event.KeyInput.Key == KEY_SPACE ||
				(event.KeyInput.Key == KEY_TAB && event.KeyInput.Control)) {
			// pass to parent
			return IGUIElement::OnEvent(event);
		}
		else if (event.KeyInput.PressedDown && event.KeyInput.Char) {
			// change selection based on text as it is typed
			u64 now = porting::getTimeMs();
			if (now - m_keynav_time >= 500)
				m_keynav_buffer = L"";
			m_keynav_time = now;

			// add to key buffer if not a key repeat
			if (!(m_keynav_buffer.size() == 1 &&
					m_keynav_buffer[0] == event.KeyInput.Char)) {
				m_keynav_buffer.append(event.KeyInput.Char);
			}

			// find the selected item, starting at the current selection
			// don't change selection if the key buffer matches the current item
			s32 old_selected = m_selected;
			s32 start = MYMAX(m_selected, 0);
			s32 rowcount = m_visible_rows.size();
			for (s32 k = 1; k < rowcount; ++k) {
				s32 current = start + k;
				if (current >= rowcount)
					current -= rowcount;
				if (doesRowStartWith(getRow(current), m_keynav_buffer)) {
					m_selected = current;
					break;
				}
			}
			autoScroll();
			if (m_selected != old_selected)
				sendTableEvent(0, false);

			return true;
		}
	}
	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		core::position2d<s32> p(event.MouseInput.X, event.MouseInput.Y);

		if (event.MouseInput.Event == EMIE_MOUSE_WHEEL) {
			m_scrollbar->setPosInterpolated(m_scrollbar->getTargetPos() +
					(event.MouseInput.Wheel < 0 ? -3 : 3) *
					- (s32) m_rowheight / 2);
			return true;
		}

		// Find hovered row and cell
		bool really_hovering = false;
		s32 row_i = getRowAt(p.Y, really_hovering);
		const Cell *cell = NULL;
		if (really_hovering) {
			s32 cell_j = getCellAt(p.X, row_i);
			if (cell_j >= 0)
				cell = &(getRow(row_i)->cells[cell_j]);
		}

		// Update tooltip
		setToolTipText(cell ? m_strings[cell->tooltip_index].c_str() : L"");

		// Fix for #1567/#1806:
		// GUIScrollBar passes double click events to its parent,
		// which we don't want. Detect this case and discard the event
		if (event.MouseInput.Event != EMIE_MOUSE_MOVED &&
				m_scrollbar->isVisible() &&
				m_scrollbar->isPointInside(p))
			return true;

		if (event.MouseInput.isLeftPressed() &&
				(isPointInside(p) ||
				 event.MouseInput.Event == EMIE_MOUSE_MOVED)) {
			s32 sel_column = 0;
			bool sel_doubleclick = (event.MouseInput.Event
					== EMIE_LMOUSE_DOUBLE_CLICK);
			bool plusminus_clicked = false;

			// For certain events (left click), report column
			// Also open/close subtrees when the +/- is clicked
			if (cell && (
					event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN ||
					event.MouseInput.Event == EMIE_LMOUSE_DOUBLE_CLICK ||
					event.MouseInput.Event == EMIE_LMOUSE_TRIPLE_CLICK)) {
				sel_column = cell->reported_column;
				if (cell->content_type == COLUMN_TYPE_TREE)
					plusminus_clicked = true;
			}

			if (plusminus_clicked) {
				if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
					toggleVisibleTree(row_i, 0, false);
				}
			}
			else {
				// Normal selection
				s32 old_selected = m_selected;
				m_selected = row_i;
				autoScroll();

				if (m_selected != old_selected ||
						sel_column >= 1 ||
						sel_doubleclick) {
					sendTableEvent(sel_column, sel_doubleclick);
				}

				// Treeview: double click opens/closes trees
				if (m_has_tree_column && sel_doubleclick) {
					toggleVisibleTree(m_selected, 0, false);
				}
			}
		}
		return true;
	}
	if (event.EventType == EET_GUI_EVENT &&
			event.GUIEvent.EventType == gui::EGET_SCROLL_BAR_CHANGED &&
			event.GUIEvent.Caller == m_scrollbar) {
		// Don't pass events from our scrollbar to the parent
		return true;
	}

	return IGUIElement::OnEvent(event);
}

/******************************************************************************/
/* GUITable helper functions                                                  */
/******************************************************************************/

s32 GUITable::allocString(const std::string &text)
{
	auto it = m_alloc_strings.find(text);
	if (it == m_alloc_strings.end()) {
		s32 id = m_strings.size();
		std::wstring wtext = utf8_to_wide(text);
		m_strings.emplace_back(wtext.c_str());
		m_alloc_strings.insert(std::make_pair(text, id));
		return id;
	}

	return it->second;
}

s32 GUITable::allocImage(const std::string &imagename)
{
	auto it = m_alloc_images.find(imagename);
	if (it == m_alloc_images.end()) {
		s32 id = m_images.size();
		m_images.push_back(m_tsrc->getTexture(imagename));
		m_alloc_images.insert(std::make_pair(imagename, id));
		return id;
	}

	return it->second;
}

void GUITable::allocationComplete()
{
	// Called when done with creating rows and cells from table data,
	// i.e. when allocString and allocImage won't be called anymore
	m_alloc_strings.clear();
	m_alloc_images.clear();
}

const GUITable::Row* GUITable::getRow(s32 i) const
{
	if (i >= 0 && i < (s32) m_visible_rows.size())
		return &m_rows[m_visible_rows[i]];

	return NULL;
}

bool GUITable::doesRowStartWith(const Row *row, const core::stringw &str) const
{
	if (row == NULL)
		return false;

	for (s32 j = 0; j < row->cellcount; ++j) {
		Cell *cell = &row->cells[j];
		if (cell->content_type == COLUMN_TYPE_TEXT) {
			const core::stringw &cellstr = m_strings[cell->content_index];
			if (cellstr.size() >= str.size() &&
					str.equals_ignore_case(cellstr.subString(0, str.size())))
				return true;
		}
	}
	return false;
}

s32 GUITable::getRowAt(s32 y, bool &really_hovering) const
{
	really_hovering = false;

	s32 rowcount = m_visible_rows.size();
	if (rowcount == 0)
		return -1;

	// Use arithmetic to find row
	s32 rel_y = y - AbsoluteRect.UpperLeftCorner.Y - 1;
	s32 i = (rel_y + m_scrollbar->getPos()) / m_rowheight;

	if (i >= 0 && i < rowcount) {
		really_hovering = true;
		return i;
	}
	if (i < 0)
		return 0;

	return rowcount - 1;
}

s32 GUITable::getCellAt(s32 x, s32 row_i) const
{
	const Row *row = getRow(row_i);
	if (row == NULL)
		return -1;

	// Use binary search to find cell in row
	s32 rel_x = x - AbsoluteRect.UpperLeftCorner.X - 1;
	s32 jmin = 0;
	s32 jmax = row->cellcount - 1;
	while (jmin < jmax) {
		s32 pivot = jmin + (jmax - jmin) / 2;
		assert(pivot >= 0 && pivot < row->cellcount);
		const Cell *cell = &row->cells[pivot];

		if (rel_x >= cell->xmin && rel_x <= cell->xmax)
			return pivot;

		if (rel_x < cell->xmin)
			jmax = pivot - 1;
		else
			jmin = pivot + 1;
	}

	if (jmin >= 0 && jmin < row->cellcount &&
			rel_x >= row->cells[jmin].xmin &&
			rel_x <= row->cells[jmin].xmax)
		return jmin;

	return -1;
}

void GUITable::autoScroll()
{
	if (m_selected >= 0) {
		s32 pos = m_scrollbar->getPos();
		s32 maxpos = m_selected * m_rowheight;
		s32 minpos = maxpos - (AbsoluteRect.getHeight() - m_rowheight);
		if (pos > maxpos)
			m_scrollbar->setPos(maxpos);
		else if (pos < minpos)
			m_scrollbar->setPos(minpos);
	}
}

void GUITable::updateScrollBar()
{
	s32 totalheight = m_rowheight * m_visible_rows.size();
	s32 scrollmax = MYMAX(0, totalheight - AbsoluteRect.getHeight());
	m_scrollbar->setVisible(scrollmax > 0);
	m_scrollbar->setMax(scrollmax);
	m_scrollbar->setSmallStep(m_rowheight);
	m_scrollbar->setLargeStep(2 * m_rowheight);
	m_scrollbar->setPageSize(totalheight);
}

void GUITable::sendTableEvent(s32 column, bool doubleclick)
{
	m_sel_column = column;
	m_sel_doubleclick = doubleclick;
	if (Parent) {
		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = this;
		e.GUIEvent.Element = 0;
		e.GUIEvent.EventType = gui::EGET_TABLE_CHANGED;
		Parent->OnEvent(e);
	}
}

void GUITable::getOpenedTrees(std::set<s32> &opened_trees) const
{
	opened_trees.clear();
	s32 rowcount = m_rows.size();
	for (s32 i = 0; i < rowcount - 1; ++i) {
		if (m_rows[i].indent < m_rows[i+1].indent &&
				m_rows[i+1].visible_index != -2)
			opened_trees.insert(i);
	}
}

void GUITable::setOpenedTrees(const std::set<s32> &opened_trees)
{
	s32 old_selected = -1;
	if (m_selected >= 0)
		old_selected = m_visible_rows[m_selected];

	std::vector<s32> parents;
	std::vector<s32> closed_parents;

	m_visible_rows.clear();

	for (size_t i = 0; i < m_rows.size(); ++i) {
		Row *row = &m_rows[i];

		// Update list of ancestors
		while (!parents.empty() && m_rows[parents.back()].indent >= row->indent)
			parents.pop_back();
		while (!closed_parents.empty() &&
				m_rows[closed_parents.back()].indent >= row->indent)
			closed_parents.pop_back();

		assert(closed_parents.size() <= parents.size());

		if (closed_parents.empty()) {
			// Visible row
			row->visible_index = m_visible_rows.size();
			m_visible_rows.push_back(i);
		}
		else if (parents.back() == closed_parents.back()) {
			// Invisible row, direct parent is closed
			row->visible_index = -2;
		}
		else {
			// Invisible row, direct parent is open, some ancestor is closed
			row->visible_index = -1;
		}

		// If not a leaf, add to parents list
		if (i < m_rows.size()-1 && row->indent < m_rows[i+1].indent) {
			parents.push_back(i);

			s32 content_index = 0; // "-", open
			if (opened_trees.count(i) == 0) {
				closed_parents.push_back(i);
				content_index = 1; // "+", closed
			}

			// Update all cells of type "tree"
			for (s32 j = 0; j < row->cellcount; ++j)
				if (row->cells[j].content_type == COLUMN_TYPE_TREE)
					row->cells[j].content_index = content_index;
		}
	}

	updateScrollBar();

	// m_selected must be updated since it is a visible row index
	if (old_selected >= 0)
		m_selected = m_rows[old_selected].visible_index;
}

void GUITable::openTree(s32 to_open)
{
	std::set<s32> opened_trees;
	getOpenedTrees(opened_trees);
	opened_trees.insert(to_open);
	setOpenedTrees(opened_trees);
}

void GUITable::closeTree(s32 to_close)
{
	std::set<s32> opened_trees;
	getOpenedTrees(opened_trees);
	opened_trees.erase(to_close);
	setOpenedTrees(opened_trees);
}

// The following function takes a visible row index (hidden rows skipped)
// dir: -1 = left (close), 0 = auto (toggle), 1 = right (open)
void GUITable::toggleVisibleTree(s32 row_i, int dir, bool move_selection)
{
	// Check if the chosen tree is currently open
	const Row *row = getRow(row_i);
	if (row == NULL)
		return;

	bool was_open = false;
	for (s32 j = 0; j < row->cellcount; ++j) {
		if (row->cells[j].content_type == COLUMN_TYPE_TREE) {
			was_open = row->cells[j].content_index == 0;
			break;
		}
	}

	// Check if the chosen tree should be opened
	bool do_open = !was_open;
	if (dir < 0)
		do_open = false;
	else if (dir > 0)
		do_open = true;

	// Close or open the tree; the heavy lifting is done by setOpenedTrees
	if (was_open && !do_open)
		closeTree(m_visible_rows[row_i]);
	else if (!was_open && do_open)
		openTree(m_visible_rows[row_i]);

	// Change selected row if requested by caller,
	// this is useful for keyboard navigation
	if (move_selection) {
		s32 sel = row_i;
		if (was_open && do_open) {
			// Move selection to first child
			const Row *maybe_child = getRow(sel + 1);
			if (maybe_child && maybe_child->indent > row->indent)
				sel++;
		}
		else if (!was_open && !do_open) {
			// Move selection to parent
			assert(getRow(sel) != NULL);
			while (sel > 0 && getRow(sel - 1)->indent >= row->indent)
				sel--;
			sel--;
			if (sel < 0)  // was root already selected?
				sel = row_i;
		}
		if (sel != m_selected) {
			m_selected = sel;
			autoScroll();
			sendTableEvent(0, false);
		}
	}
}

void GUITable::alignContent(Cell *cell, s32 xmax, s32 content_width, s32 align)
{
	// requires that cell.xmin, cell.xmax are properly set
	// align = 0: left aligned, 1: centered, 2: right aligned, 3: inline
	if (align == 0) {
		cell->xpos = cell->xmin;
		cell->xmax = xmax;
	}
	else if (align == 1) {
		cell->xpos = (cell->xmin + xmax - content_width) / 2;
		cell->xmax = xmax;
	}
	else if (align == 2) {
		cell->xpos = xmax - content_width;
		cell->xmax = xmax;
	}
	else {
		// inline alignment: the cells of the column don't have an aligned
		// right border, the right border of each cell depends on the content
		cell->xpos = cell->xmin;
		cell->xmax = cell->xmin + content_width;
	}
}
