// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "guiScrollBar.h"

class ISimpleTextureSource;

/*
	A table GUI element for GUIFormSpecMenu.

	Sends a EGET_TABLE_CHANGED event to the parent when
	an item is selected or double-clicked.
	Call checkEvent() to get info.

	Credits: The interface and implementation of this class are (very)
	loosely based on the Irrlicht classes CGUITable and CGUIListBox.
	CGUITable and CGUIListBox are licensed under the Irrlicht license;
	they are Copyright (C) 2002-2012 Nikolaus Gebhardt
*/
class GUITable : public gui::IGUIElement
{
public:
	/*
		Stores dynamic data that should be preserved
		when updating a formspec
	*/
	struct DynamicData
	{
		s32 selected = 0;
		s32 scrollpos = 0;
		s32 keynav_time = 0;
		core::stringw keynav_buffer;
		std::set<s32> opened_trees;
	};

	/*
		An option of the form <name>=<value>
	*/
	struct Option
	{
		std::string name;
		std::string value;

		Option(const std::string &name_, const std::string &value_) :
			name(name_),
			value(value_)
		{}
	};

	/*
		A list of options that concern the entire table
	*/
	typedef std::vector<Option> TableOptions;

	/*
		A column with options
	*/
	struct TableColumn
	{
		std::string type;
		std::vector<Option> options;
	};
	typedef std::vector<TableColumn> TableColumns;


	GUITable(gui::IGUIEnvironment *env,
			gui::IGUIElement *parent, s32 id,
			core::rect<s32> rectangle,
			ISimpleTextureSource *tsrc);

	virtual ~GUITable();

	/* Split a string of the form "name=value" into name and value */
	static Option splitOption(const std::string &str);

	/* Set textlist-like options, columns and data */
	void setTextList(const std::vector<std::string> &content,
			bool transparent);

	/** Set generic table options, columns and content, calculate cell sizes */
	// Adds empty strings to end of content if there is an incomplete row
	void setTable(const TableOptions &options,
			const TableColumns &columns,
			std::vector<std::string> &content);

	/* Clear the table */
	void clear();

	/* Get info about last event (string such as "CHG:1:2") */
	// Call this after EGET_TABLE_CHANGED
	std::string checkEvent();

	/* Get index of currently selected row (first=1; 0 if none selected) */
	s32 getSelected() const;

	/* Set currently selected row (first=1; 0 if none selected) */
	// If given index is not visible at the moment, select its parent
	// Autoscroll to make the selected row fully visible
	void setSelected(s32 index);

	//! Sets another skin independent font. If this is set to zero, the button uses the font of the skin.
	virtual void setOverrideFont(gui::IGUIFont *font = nullptr);

	//! Gets the override font (if any)
	virtual gui::IGUIFont *getOverrideFont() const;

	/* Get selection, scroll position and opened (sub)trees */
	DynamicData getDynamicData() const;

	/* Set selection, scroll position and opened (sub)trees */
	void setDynamicData(const DynamicData &dyndata);

	/* Must be called when position or size changes */
	virtual void updateAbsolutePosition();

	/* Irrlicht draw method */
	virtual void draw();

	/* Irrlicht event handler */
	virtual bool OnEvent(const SEvent &event);

protected:
	enum ColumnType {
		COLUMN_TYPE_TEXT,
		COLUMN_TYPE_IMAGE,
		COLUMN_TYPE_COLOR,
		COLUMN_TYPE_INDENT,
		COLUMN_TYPE_TREE,
	};

	struct Cell {
		s32 xmin;
		s32 xmax;
		s32 xpos;
		ColumnType content_type;
		s32 content_index;
		s32 tooltip_index;
		video::SColor color;
		bool color_defined;
		s32 reported_column;
		f32 image_scale; // only for "image" type columns
	};

	struct Row {
		Cell *cells;
		s32 cellcount;
		s32 indent;
		// visible_index >= 0: is index of row in m_visible_rows
		// visible_index == -1: parent open but other ancestor closed
		// visible_index == -2: parent closed
		s32 visible_index;
	};

	// Texture source
	ISimpleTextureSource *m_tsrc;

	// Table content (including hidden rows)
	std::vector<Row> m_rows;
	// Table content (only visible; indices into m_rows)
	std::vector<s32> m_visible_rows;
	bool m_is_textlist = false;
	bool m_has_tree_column = false;

	// Selection status
	s32 m_selected = -1; // index of row (1...n), or 0 if none selected
	s32 m_sel_column = 0;
	bool m_sel_doubleclick = false;

	// Keyboard navigation stuff
	u64 m_keynav_time = 0;
	core::stringw m_keynav_buffer = L"";

	// Drawing and geometry information
	bool m_border = true;
	video::SColor m_color = video::SColor(255, 255, 255, 255);
	video::SColor m_background = video::SColor(255, 0, 0, 0);
	video::SColor m_highlight = video::SColor(255, 70, 100, 50);
	video::SColor m_highlight_text = video::SColor(255, 255, 255, 255);
	s32 m_rowheight = 1;
	gui::IGUIFont *m_font = nullptr;
	GUIScrollBar *m_scrollbar = nullptr;

	// Allocated strings and images
	std::vector<core::stringw> m_strings;
	std::vector<video::ITexture*> m_images;
	std::map<std::string, s32> m_alloc_strings;
	std::map<std::string, s32> m_alloc_images;

	s32 allocString(const std::string &text);
	s32 allocImage(const std::string &imagename);
	void allocationComplete();

	// Helper for draw() that draws a single cell
	void drawCell(const Cell *cell, video::SColor color,
			const core::rect<s32> &rowrect,
			const core::rect<s32> &client_clip);

	// Returns the i-th visible row (NULL if i is invalid)
	const Row *getRow(s32 i) const;

	// Key navigation helper
	bool doesRowStartWith(const Row *row, const core::stringw &str) const;

	// Returns the row at a given screen Y coordinate
	// Returns index i such that m_rows[i] is valid (or -1 on error)
	s32 getRowAt(s32 y, bool &really_hovering) const;

	// Returns the cell at a given screen X coordinate within m_rows[row_i]
	// Returns index j such that m_rows[row_i].cells[j] is valid
	// (or -1 on error)
	s32 getCellAt(s32 x, s32 row_i) const;

	// Make the selected row fully visible
	void autoScroll();

	// Should be called when m_rowcount or m_rowheight changes
	void updateScrollBar();

	// Sends EET_GUI_EVENT / EGET_TABLE_CHANGED to parent
	void sendTableEvent(s32 column, bool doubleclick);

	// Functions that help deal with hidden rows
	// The following functions take raw row indices (hidden rows not skipped)
	void getOpenedTrees(std::set<s32> &opened_trees) const;
	void setOpenedTrees(const std::set<s32> &opened_trees);
	void openTree(s32 to_open);
	void closeTree(s32 to_close);
	// The following function takes a visible row index (hidden rows skipped)
	// dir: -1 = left (close), 0 = auto (toggle), 1 = right (open)
	void toggleVisibleTree(s32 row_i, int dir, bool move_selection);

	// Aligns cell content in column according to alignment specification
	// align = 0: left aligned, 1: centered, 2: right aligned, 3: inline
	static void alignContent(Cell *cell, s32 xmax, s32 content_width,
			s32 align);
};
