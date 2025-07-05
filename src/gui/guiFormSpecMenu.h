// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <optional>
#include <utility>
#include <stack>
#include <unordered_set>

#include "irrlichttypes_bloated.h"
#include "irr_ptr.h"
#include "inventory.h"
#include "inventorymanager.h"
#include "modalMenu.h"
#include "guiInventoryList.h"
#include "guiScrollBar.h"
#include "guiTable.h"
#include "util/string.h"
#include "util/enriched_string.h"
#include "StyleSpec.h"
#include <ICursorControl.h> // gui::ECURSOR_ICON
#include <IGUIStaticText.h>

class InventoryManager;
class ISimpleTextureSource;
class Client;
class GUIScrollContainer;
class ISoundManager;
class JoystickController;

enum FormspecFieldType {
	f_Button,
	f_Table,
	f_TabHeader,
	f_CheckBox,
	f_DropDown,
	f_ScrollBar,
	f_Box,
	f_ItemImage,
	f_HyperText,
	f_AnimatedImage,
	f_Unknown
};

enum FormspecQuitMode {
	quit_mode_no,
	quit_mode_accept,
	quit_mode_cancel,
	quit_mode_try,
};

enum ButtonEventType : u8
{
	BET_LEFT,
	BET_RIGHT,
	BET_MIDDLE,
	BET_WHEEL_UP,
	BET_WHEEL_DOWN,
	BET_UP,
	BET_DOWN,
	BET_MOVE,
	BET_OTHER
};

struct TextDest
{
	virtual ~TextDest() = default;

	virtual void gotText(const StringMap &fields) = 0;

	std::string m_formname;
};

class IFormSource
{
public:
	virtual ~IFormSource() = default;
	virtual const std::string &getForm() const = 0;
	// Fill in variables in field text
	virtual std::string resolveText(const std::string &str) { return str; }
};

class GUIFormSpecMenu : public GUIModalMenu
{
	struct ListRingSpec
	{
		ListRingSpec() = default;

		ListRingSpec(const InventoryLocation &a_inventoryloc,
				const std::string &a_listname):
			inventoryloc(a_inventoryloc),
			listname(a_listname)
		{
		}

		InventoryLocation inventoryloc;
		std::string listname;
	};

	struct FieldSpec
	{
		FieldSpec() = default;

		FieldSpec(const std::string &name, const std::wstring &label,
				const std::wstring &default_text, s32 id, int priority = 0,
				gui::ECURSOR_ICON cursor_icon = ECI_NORMAL) :
			fname(name),
			flabel(label),
			fdefault(unescape_enriched(translate_string(default_text))),
			fid(id),
			send(false),
			ftype(f_Unknown),
			is_exit(false),
			priority(priority),
			fcursor_icon(cursor_icon)
		{
		}

		std::string fname;
		std::wstring flabel;
		std::wstring fdefault;
		std::string url;
		s32 fid;
		bool send;
		FormspecFieldType ftype;
		bool is_exit;
		// Draw priority for formspec version < 3
		int priority;
		gui::ECURSOR_ICON fcursor_icon;
		std::string sound;
		f32 aux_f32 = 0;
	};

	struct TooltipSpec
	{
		TooltipSpec() = default;
		TooltipSpec(const std::wstring &a_tooltip, video::SColor a_bgcolor,
				video::SColor a_color):
			tooltip(translate_string(a_tooltip)),
			bgcolor(a_bgcolor),
			color(a_color)
		{
		}

		std::wstring tooltip;
		video::SColor bgcolor;
		video::SColor color;
	};

public:
	GUIFormSpecMenu(JoystickController *joystick,
			gui::IGUIElement* parent, s32 id,
			IMenuManager *menumgr,
			Client *client,
			gui::IGUIEnvironment *guienv,
			ISimpleTextureSource *tsrc,
			ISoundManager *sound_manager,
			IFormSource* fs_src,
			TextDest* txt_dst,
			const std::string &formspecPrepend,
			bool remap_dbl_click = true);

	~GUIFormSpecMenu();

	static void create(GUIFormSpecMenu *&cur_formspec, Client *client,
		gui::IGUIEnvironment *guienv, JoystickController *joystick, IFormSource *fs_src,
		TextDest *txt_dest, const std::string &formspecPrepend,
		ISoundManager *sound_manager);

	void setFormSpec(const std::string &formspec_string,
			const InventoryLocation &current_inventory_location)
	{
		m_formspec_string = formspec_string;
		m_current_inventory_location = current_inventory_location;
		m_is_form_regenerated = false;
		regenerateGui(m_screensize_old);
	}

	const InventoryLocation &getFormspecLocation()
	{
		return m_current_inventory_location;
	}

	void setFormspecPrepend(const std::string &formspecPrepend)
	{
		m_formspec_prepend = formspecPrepend;
	}

	// form_src is deleted by this GUIFormSpecMenu
	void setFormSource(IFormSource *form_src)
	{
		delete m_form_src;
		m_form_src = form_src;
	}

	// text_dst is deleted by this GUIFormSpecMenu
	void setTextDest(TextDest *text_dst)
	{
		delete m_text_dst;
		m_text_dst = text_dst;
	}

	void defaultAllowClose(bool value)
	{
		// Also set m_allowclose here in order to have the correct value if
		// escape is pressed before regenerateGui() is called.
		m_default_allowclose = value;
		m_allowclose = value;
	}

	void setDebugView(bool value)
	{
		m_show_debug = value;
	}

	void lockSize(bool lock,v2u32 basescreensize=v2u32(0,0))
	{
		m_lock = lock;
		m_lockscreensize = basescreensize;
	}

	void removeTooltip();
	void setInitialFocus();

	void setFocus(const std::string &elementname)
	{
		m_focused_element = elementname;
	}

	Client *getClient() const
	{
		return m_client;
	}

	const GUIInventoryList::ItemSpec *getSelectedItem() const
	{
		return m_selected_item.get();
	}

	u16 getSelectedAmount() const
	{
		return m_selected_amount;
	}

	bool doTooltipAppendItemname() const
	{
		return m_tooltip_append_itemname;
	}

	void addHoveredItemTooltip(const std::string &name)
	{
		m_hovered_item_tooltips.emplace_back(name);
	}

	/*
		Remove and re-add (or reposition) stuff
	*/
	void regenerateGui(v2u32 screensize);

	GUIInventoryList::ItemSpec getItemAtPos(v2s32 p) const;
	void drawSelectedItem();
	void drawMenu();
	void updateSelectedItem();
	ItemStack verifySelectedItem();

	s16 getNextInventoryRing(const InventoryLocation &inventoryloc, const std::string &listname);

	void acceptInput(FormspecQuitMode quitmode=quit_mode_no);
	bool preprocessEvent(const SEvent& event);
	bool OnEvent(const SEvent& event);
	bool doPause;
	bool pausesGame() { return doPause; }

	GUITable* getTable(const std::string &tablename);
	std::vector<std::string>* getDropDownValues(const std::string &name);

	// This will only return a meaningful value if called after drawMenu().
	core::rect<s32> getAbsoluteRect();

#ifdef __ANDROID__
	void getAndroidUIInput();
#endif

	// Returns the fixed formspec coordinate size for the given parameters.
	static double getFixedImgsize(double screen_dpi, double gui_scaling);
	// Returns the preferred non-fixed formspec coordinate size for the given parameters.
	static double getImgsize(v2u32 avail_screensize, double screen_dpi, double gui_scaling);

protected:
	v2s32 getBasePos() const
	{
			return padding + offset + AbsoluteRect.UpperLeftCorner;
	}
	std::wstring getLabelByID(s32 id);
	std::string getNameByID(s32 id);
	const FieldSpec *getSpecByID(s32 id);
	v2s32 getElementBasePos(const std::vector<std::string> *v_pos);
	v2s32 getRealCoordinateBasePos(const std::vector<std::string> &v_pos);
	v2s32 getRealCoordinateGeometry(const std::vector<std::string> &v_geom);
	bool precheckElement(const std::string &name, const std::string &element,
		size_t args_min, size_t args_max, std::vector<std::string> &parts);

	std::unordered_map<std::string, std::vector<StyleSpec>> theme_by_type;
	std::unordered_map<std::string, std::vector<StyleSpec>> theme_by_name;
	std::unordered_set<std::string> property_warned;

	StyleSpec getDefaultStyleForElement(const std::string &type,
			const std::string &name="", const std::string &parent_type="");
	std::array<StyleSpec, StyleSpec::NUM_STATES> getStyleForElement(const std::string &type,
			const std::string &name="", const std::string &parent_type="");

	v2s32 padding;
	v2f32 spacing;
	v2s32 imgsize;
	v2s32 offset;
	v2f32 pos_offset;
	std::stack<v2f32> container_stack;

	InventoryManager *m_invmgr;
	ISimpleTextureSource *m_tsrc;
	ISoundManager *m_sound_manager;
	Client *m_client;

	std::string m_formspec_string;
	std::string m_formspec_prepend;
	InventoryLocation m_current_inventory_location;

	// Default true because we can't control regeneration on resizing, but
	// we can control cases when the formspec is shown intentionally.
	bool m_is_form_regenerated = true;

	std::vector<GUIInventoryList *> m_inventorylists;
	std::vector<ListRingSpec> m_inventory_rings;
	std::unordered_map<std::string, bool> field_enter_after_edit;
	std::unordered_map<std::string, bool> field_close_on_enter;
	std::unordered_map<std::string, bool> m_dropdown_index_event;
	std::vector<FieldSpec> m_fields;
	std::vector<std::pair<FieldSpec, GUITable *>> m_tables;
	std::vector<std::pair<FieldSpec, gui::IGUICheckBox *>> m_checkboxes;
	std::map<std::string, TooltipSpec> m_tooltips;
	std::vector<std::pair<gui::IGUIElement *, TooltipSpec>> m_tooltip_rects;
	std::vector<std::pair<FieldSpec, GUIScrollBar *>> m_scrollbars;
	std::vector<std::pair<FieldSpec, std::vector<std::string>>> m_dropdowns;
	std::vector<gui::IGUIElement *> m_clickthrough_elements;
	std::vector<std::pair<std::string, GUIScrollContainer *>> m_scroll_containers;

	std::unique_ptr<GUIInventoryList::ItemSpec> m_selected_item;
	u16 m_selected_amount = 0;
	bool m_selected_dragging = false;
	ItemStack m_selected_swap;
	ButtonEventType m_held_mouse_button = BET_OTHER;
	bool m_shift_move_after_craft = false;

	u16 m_left_drag_amount = 0;
	ItemStack m_left_drag_stack;
	std::vector<std::pair<GUIInventoryList::ItemSpec, ItemStack>> m_left_drag_stacks;
	bool m_left_dragging = false;

	gui::IGUIStaticText *m_tooltip_element = nullptr;

	u64 m_tooltip_show_delay;
	bool m_tooltip_append_itemname;
	u64 m_hovered_time = 0;
	s32 m_old_tooltip_id = -1;

	bool m_default_allowclose = true;
	bool m_allowclose = true;
	bool m_lock = false;
	v2u32 m_lockscreensize;

	bool m_bgnonfullscreen;
	bool m_bgfullscreen;
	video::SColor m_bgcolor;
	video::SColor m_fullscreen_bgcolor;
	video::SColor m_default_tooltip_bgcolor;
	video::SColor m_default_tooltip_color;

private:
	IFormSource               *m_form_src;
	TextDest                  *m_text_dst;
	std::string                m_last_formname;
	u16                        m_formspec_version = 1;
	std::optional<std::string> m_focused_element = std::nullopt;
	JoystickController        *m_joystick;
	bool                       m_show_debug = false;

	struct parserData {
		bool explicit_size;
		bool real_coordinates;
		u8 simple_field_count;
		v2f invsize;
		v2s32 size;
		v2f32 offset;
		v2f32 anchor;
		v2f32 padding;
		core::rect<s32> rect;
		v2s32 basepos;
		v2u32 screensize;
		GUITable::TableOptions table_options;
		GUITable::TableColumns table_columns;
		gui::IGUIElement *current_parent = nullptr;
		irr_ptr<gui::IGUIElement> background_parent;

		GUIInventoryList::Options inventorylist_options;

		struct {
			s32 max = 1000;
			s32 min = 0;
			s32 small_step = 10;
			s32 large_step = 100;
			s32 thumb_size = 1;
			GUIScrollBar::ArrowVisibility arrow_visiblity = GUIScrollBar::DEFAULT;
		} scrollbar_options;

		// used to restore table selection/scroll/treeview state
		std::unordered_map<std::string, GUITable::DynamicData> table_dyndata;
		std::string type;
	};

	static const std::unordered_map<std::string, std::function<void(GUIFormSpecMenu*, GUIFormSpecMenu::parserData *data, const std::string &description)>> element_parsers;

	struct fs_key_pending {
		bool key_up;
		bool key_down;
		bool key_enter;
	};

	fs_key_pending current_keys_pending;
	std::string current_field_enter_pending = "";
	std::vector<std::string> m_hovered_item_tooltips;

	void removeAll();

	void parseElement(parserData* data, const std::string &element);

	void parseSize(parserData* data, const std::string &element);
	void parseContainer(parserData* data, const std::string &element);
	void parseContainerEnd(parserData* data, const std::string &element);
	void parseScrollContainer(parserData *data, const std::string &element);
	void parseScrollContainerEnd(parserData *data, const std::string &element);
	void parseList(parserData* data, const std::string &element);
	void parseListRing(parserData* data, const std::string &element);
	void parseCheckbox(parserData* data, const std::string &element);
	void parseImage(parserData* data, const std::string &element);
	void parseAnimatedImage(parserData *data, const std::string &element);
	void parseItemImage(parserData* data, const std::string &element);
	void parseButton(parserData* data, const std::string &element);
	void parseBackground(parserData* data, const std::string &element);
	void parseTableOptions(parserData* data, const std::string &element);
	void parseTableColumns(parserData* data, const std::string &element);
	void parseTable(parserData* data, const std::string &element);
	void parseTextList(parserData* data, const std::string &element);
	void parseDropDown(parserData* data, const std::string &element);
	void parseFieldEnterAfterEdit(parserData *data, const std::string &element);
	void parseFieldCloseOnEnter(parserData *data, const std::string &element);
	void parsePwdField(parserData* data, const std::string &element);
	void parseField(parserData* data, const std::string &element);
	void createTextField(parserData *data, FieldSpec &spec,
		core::rect<s32> &rect, bool is_multiline);
	void parseSimpleField(parserData* data,std::vector<std::string> &parts);
	void parseTextArea(parserData* data,std::vector<std::string>& parts,
			const std::string &type);
	void parseHyperText(parserData *data, const std::string &element);
	void parseLabel(parserData* data, const std::string &element);
	void parseVertLabel(parserData* data, const std::string &element);
	void parseImageButton(parserData* data, const std::string &element);
	void parseItemImageButton(parserData* data, const std::string &element);
	void parseTabHeader(parserData* data, const std::string &element);
	void parseBox(parserData* data, const std::string &element);
	void parseBackgroundColor(parserData* data, const std::string &element);
	void parseListColors(parserData* data, const std::string &element);
	void parseTooltip(parserData* data, const std::string &element);
	bool parseVersionDirect(const std::string &data);
	bool parseSizeDirect(parserData* data, const std::string &element);
	void parseRealCoordinates(parserData* data, const std::string &element);
	void parseScrollBar(parserData* data, const std::string &element);
	void parseScrollBarOptions(parserData *data, const std::string &element);
	bool parsePositionDirect(parserData *data, const std::string &element);
	void parsePosition(parserData *data, const std::string &element);
	bool parseAnchorDirect(parserData *data, const std::string &element);
	void parseAnchor(parserData *data, const std::string &element);
	bool parsePaddingDirect(parserData *data, const std::string &element);
	void parsePadding(parserData *data, const std::string &element);
	void parseStyle(parserData *data, const std::string &element);
	void parseSetFocus(parserData *, const std::string &element);
	void parseModel(parserData *data, const std::string &element);
	void parseAllowClose(parserData *data, const std::string &element);

	bool parseMiddleRect(const std::string &value, core::rect<s32> *parsed_rect);

	void tryClose();
	void trySubmitClose();

	void showTooltip(const std::wstring &text, const video::SColor &color,
		const video::SColor &bgcolor);

	/**
	 * In formspec version < 2 the elements were not ordered properly. Some element
	 * types were drawn before others.
	 * This function sorts the elements in the old order for backwards compatibility.
	 */
	void legacySortElements(std::list<IGUIElement *>::iterator from);

	int m_btn_height;
	gui::IGUIFont *m_font = nullptr;

	// used by getAbsoluteRect
	s32 m_tabheader_upper_edge = 0;

	// Determines the size (in pixels) of formspec coordinate units.
	double calculateImgsize(const parserData &data);
};

class FormspecFormSource: public IFormSource
{
public:
	FormspecFormSource(const std::string &formspec):
		m_formspec(formspec)
	{
	}

	~FormspecFormSource() = default;

	void setForm(const std::string &formspec)
	{
		m_formspec = formspec;
	}

	const std::string &getForm() const
	{
		return m_formspec;
	}

	std::string m_formspec;
};
