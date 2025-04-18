-- Luanti
-- Copyright (C) 2022 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later


local path = core.get_builtin_path() .. "common" .. DIR_DELIM .. "settings" .. DIR_DELIM

local component_funcs =  dofile(path .. "components.lua")
local shadows_component =  dofile(path .. "shadows_component.lua")

local loaded = false
local info_icon_path = core.formspec_escape(defaulttexturedir .. "settings_info.png")
local reset_icon_path = core.formspec_escape(defaulttexturedir .. "settings_reset.png")
local all_pages = {}
local page_by_id = {}
local filtered_pages = all_pages
local filtered_page_by_id = page_by_id


local function get_setting_info(name)
	for _, entry in ipairs(core.full_settingtypes) do
		if entry.type ~= "category" and entry.name == name then
			return entry
		end
	end

	return nil
end


local function add_page(page)
	assert(type(page.id) == "string")
	assert(type(page.title) == "string")
	assert(page.section == nil or type(page.section) == "string")
	assert(type(page.content) == "table")

	assert(not page_by_id[page.id], "Page " .. page.id .. " already registered")

	all_pages[#all_pages + 1] = page
	page_by_id[page.id] = page
	return page
end


local function load_settingtypes()
	local page = nil
	local section = nil
	local function ensure_page_started()
		if not page then
			page = add_page({
				id = (section or "general"):lower():gsub(" ", "_"),
				title = section or fgettext_ne("General"),
				section = section,
				content = {},
			})
		end
	end

	for _, entry in ipairs(core.full_settingtypes) do
		if entry.type == "category" then
			if entry.level == 0 then
				section = entry.name
				page = nil
			elseif entry.level == 1 then
				page = {
					id = ((section and section .. "_" or "") .. entry.name):lower():gsub(" ", "_"),
					title = entry.readable_name or entry.name,
					section = section,
					content = {},
				}

				page = add_page(page)
			elseif entry.level == 2 then
				ensure_page_started()
				page.content[#page.content + 1] = {
					heading = fgettext_ne(entry.readable_name or entry.name),
				}
			end
		else
			ensure_page_started()
			page.content[#page.content + 1] = entry.name
		end
	end
end


local function load()
	if loaded then
		return
	end
	loaded = true

	core.full_settingtypes = settingtypes.parse_config_file(false, true)

	local touchscreen_layout = {
		query_text = "Touchscreen layout",
		requires = {
			touchscreen = true,
		},
		context = "client",
		get_formspec = function(self, avail_w)
			local btn_w = math.min(avail_w, 6)
			return ("button[0,0;%f,0.8;btn_touch_layout;%s]"):format(btn_w, fgettext("Touchscreen layout")), 0.8
		end,
		on_submit = function(self, fields)
			if fields.btn_touch_layout then
				core.show_touchscreen_layout()
			end
		end,
	}

	add_page({
		id = "accessibility",
		title = fgettext_ne("Accessibility"),
		content = {
			"language",
			{ heading = fgettext_ne("General") },
			"font_size",
			"chat_font_size",
			"gui_scaling",
			"hud_scaling",
			"show_nametag_backgrounds",
			{ heading = fgettext_ne("Chat") },
			"console_height",
			"console_alpha",
			"console_color",
			{ heading = fgettext_ne("Controls") },
			"autojump",
			"safe_dig_and_place",
			{ heading = fgettext_ne("Movement") },
			"arm_inertia",
			"view_bobbing_amount",
		},
	})

	load_settingtypes()

	-- insert after "touch_controls"
	table.insert(page_by_id.controls_touchscreen.content, 2, touchscreen_layout)
	do
		local content = page_by_id.graphics_and_audio_effects.content
		local idx = table.indexof(content, "enable_dynamic_shadows")
		table.insert(content, idx, shadows_component)

		idx = table.indexof(content, "enable_auto_exposure") + 1
		local setting_info = get_setting_info("enable_auto_exposure")
		local note = component_funcs.note(fgettext_ne("(The game will need to enable automatic exposure as well)"))
		note.requires = setting_info.requires
		note.context = setting_info.context
		table.insert(content, idx, note)

		idx = table.indexof(content, "enable_bloom") + 1
		setting_info = get_setting_info("enable_bloom")
		note = component_funcs.note(fgettext_ne("(The game will need to enable bloom as well)"))
		note.requires = setting_info.requires
		note.context = setting_info.context
		table.insert(content, idx, note)

		idx = table.indexof(content, "enable_volumetric_lighting") + 1
		setting_info = get_setting_info("enable_volumetric_lighting")
		note = component_funcs.note(fgettext_ne("(The game will need to enable volumetric lighting as well)"))
		note.requires = setting_info.requires
		note.context = setting_info.context
		table.insert(content, idx, note)
	end

	-- These must not be translated, as they need to show in the local
	-- language no matter the user's current language.
	-- This list must be kept in sync with src/unsupported_language_list.txt.
	get_setting_info("language").option_labels = {
		[""] = fgettext_ne("(Use system language)"),
		--ar = " [ar]", blacklisted
		be = "Беларуская [be]",
		bg = "Български [bg]",
		ca = "Català [ca]",
		cs = "Česky [cs]",
		cy = "Cymraeg [cy]",
		da = "Dansk [da]",
		de = "Deutsch [de]",
		--dv = " [dv]", blacklisted
		el = "Ελληνικά [el]",
		en = "English [en]",
		eo = "Esperanto [eo]",
		es = "Español [es]",
		et = "Eesti [et]",
		eu = "Euskara [eu]",
		fi = "Suomi [fi]",
		fil = "Wikang Filipino [fil]",
		fr = "Français [fr]",
		gd = "Gàidhlig [gd]",
		gl = "Galego [gl]",
		--he = " [he]", blacklisted
		--hi = " [hi]", blacklisted
		hu = "Magyar [hu]",
		id = "Bahasa Indonesia [id]",
		it = "Italiano [it]",
		ja = "日本語 [ja]",
		jbo = "Lojban [jbo]",
		kk = "Қазақша [kk]",
		--kn = " [kn]", blacklisted
		ko = "한국어 [ko]",
		ky = "Kırgızca / Кыргызча [ky]",
		lt = "Lietuvių [lt]",
		lv = "Latviešu [lv]",
		mn = "Монгол [mn]",
		mr = "मराठी [mr]",
		ms = "Bahasa Melayu [ms]",
		--ms_Arab = " [ms_Arab]", blacklisted
		nb = "Norsk Bokmål [nb]",
		nl = "Nederlands [nl]",
		nn = "Norsk Nynorsk [nn]",
		oc = "Occitan [oc]",
		pl = "Polski [pl]",
		pt = "Português [pt]",
		pt_BR = "Português do Brasil [pt_BR]",
		ro = "Română [ro]",
		ru = "Русский [ru]",
		sk = "Slovenčina [sk]",
		sl = "Slovenščina [sl]",
		sr_Cyrl = "Српски [sr_Cyrl]",
		sr_Latn = "Srpski (Latinica) [sr_Latn]",
		sv = "Svenska [sv]",
		sw = "Kiswahili [sw]",
		--th = " [th]", blacklisted
		tr = "Türkçe [tr]",
		tt = "Tatarça [tt]",
		uk = "Українська [uk]",
		vi = "Tiếng Việt [vi]",
		zh_CN = "中文 (简体) [zh_CN]",
		zh_TW = "正體中文 (繁體) [zh_TW]",
	}

	get_setting_info("touch_controls").option_labels = {
		["auto"] = fgettext_ne("Auto"),
		["true"] = fgettext_ne("Enabled"),
		["false"] = fgettext_ne("Disabled"),
	}

	get_setting_info("touch_interaction_style").option_labels = {
		["tap"] = fgettext_ne("Tap"),
		["tap_crosshair"] = fgettext_ne("Tap with crosshair"),
		["buttons_crosshair"] = fgettext("Buttons with crosshair"),
	}

	get_setting_info("touch_punch_gesture").option_labels = {
		["short_tap"] = fgettext_ne("Short tap"),
		["long_tap"] = fgettext_ne("Long tap"),
	}
end


-- See if setting matches keywords
local function get_setting_match_weight(entry, query_keywords)
	local setting_score = 0
	for _, keyword in ipairs(query_keywords) do
		if string.find(entry.name:lower(), keyword, 1, true) then
			setting_score = setting_score + 1
		end

		if entry.readable_name and
				string.find(fgettext(entry.readable_name):lower(), keyword, 1, true) then
			setting_score = setting_score + 1
		end

		if entry.comment and
				string.find(fgettext_ne(entry.comment):lower(), keyword, 1, true) then
			setting_score = setting_score + 1
		end
	end

	return setting_score
end


local function filter_page_content(page, query_keywords)
	if #query_keywords == 0 then
		return page.content, 0
	end

	local retval = {}
	local i = 1
	local max_weight = 0
	for _, content in ipairs(page.content) do
		if type(content) == "string" then
			local setting = get_setting_info(content)
			assert(setting, "Unknown setting: " .. content)

			local weight = get_setting_match_weight(setting, query_keywords)
			if weight > 0 then
				max_weight = math.max(max_weight, weight)
				retval[i] = content
				i = i + 1
			end
		elseif type(content) == "table" and content.query_text then
			for _, keyword in ipairs(query_keywords) do
				if string.find(fgettext(content.query_text), keyword, 1, true) then
					max_weight = math.max(max_weight, 1)
					retval[i] = content
					i = i + 1
					break
				end
			end
		end
	end
	return retval, max_weight
end


local function update_filtered_pages(query)
	filtered_pages = {}
	filtered_page_by_id = {}

	local query_keywords = {}
	for word in query:lower():gmatch("%S+") do
		table.insert(query_keywords, word)
	end

	local best_page = nil
	local best_page_weight = -1

	for _, page in ipairs(all_pages) do
		local content, page_weight = filter_page_content(page, query_keywords)
		if page_has_contents(page, content) then
			local new_page = table.copy(page)
			new_page.content = content

			filtered_pages[#filtered_pages + 1] = new_page
			filtered_page_by_id[new_page.id] = new_page

			if page_weight > best_page_weight then
				best_page = new_page
				best_page_weight = page_weight
			end
		end
	end

	return best_page and best_page.id or nil
end


local shown_contexts = {
	common = true,
	client = true,
	server = INIT ~= "pause_menu" or core.is_internal_server(),
	world_creation = INIT ~= "pause_menu",
}

local function check_requirements(name, requires, context)
	if context and not shown_contexts[context] then
		return false
	end

	if requires == nil then
		return true
	end

	local video_driver = core.get_active_driver()
	local touch_support = core.irrlicht_device_supports_touch()
	local touch_controls = core.settings:get("touch_controls")
	local touch_interaction_style = core.settings:get("touch_interaction_style")
	local special = {
		android = PLATFORM == "Android",
		desktop = PLATFORM ~= "Android",
		touch_support = touch_support,
		-- When touch_controls is "auto", we don't know which input method will
		-- be used, so we show settings for both.
		touchscreen = touch_support and (touch_controls == "auto" or core.is_yes(touch_controls)),
		keyboard_mouse = not touch_support or (touch_controls == "auto" or not core.is_yes(touch_controls)),
		opengl = (video_driver == "opengl" or video_driver == "opengl3"),
		gles = video_driver:sub(1, 5) == "ogles",
		touch_interaction_style_tap = touch_interaction_style ~= "buttons_crosshair",
	}

	for req_key, req_value in pairs(requires) do
		if special[req_key] == nil then
			local required_setting = get_setting_info(req_key)
			if required_setting == nil then
				core.log("warning", "Unknown setting " .. req_key .. " required by " .. (name or "???"))
			elseif required_setting.type ~= "bool" then
				core.log("warning", "Setting " .. req_key .. " of type " .. required_setting.type ..
					" used as requirement by " .. (name or "???") .. ", only bool is allowed")
			end
			local actual_value = core.settings:get_bool(req_key,
				required_setting and core.is_yes(required_setting.default))
			if actual_value ~= req_value  then
				return false
			end
		elseif special[req_key] ~= req_value then
			return false
		end
	end

	return true
end


function page_has_contents(page, actual_content)
	local is_advanced =
			page.id:sub(1, #"client_and_server") == "client_and_server" or
			page.id:sub(1, #"mapgen") == "mapgen" or
			page.id:sub(1, #"advanced") == "advanced"
	local show_advanced = core.settings:get_bool("show_advanced")
	if is_advanced and not show_advanced then
		return false
	end

	for _, item in ipairs(actual_content) do
		if item == false or item.heading then --luacheck: ignore
			-- skip
		elseif type(item) == "string" then
			local setting = get_setting_info(item)
			assert(setting, "Unknown setting: " .. item)
			if check_requirements(setting.name, setting.requires, setting.context) then
				return true
			end
		elseif item.get_formspec then
			if check_requirements(item.id, item.requires, item.context) then
				return true
			end
		else
			error("Unknown content in page: " .. dump(item))
		end
	end

	return false
end


local function build_page_components(page)
	-- Filter settings based on requirements
	local content = {}
	local last_heading
	for _, item in ipairs(page.content) do
		if item == false then --luacheck: ignore
			-- skip
		elseif item.heading then
			last_heading = item
		else
			local name, requires, context
			if type(item) == "string" then
				local setting = get_setting_info(item)
				assert(setting, "Unknown setting: " .. item)
				name = setting.name
				requires = setting.requires
				context = setting.context
			elseif item.get_formspec then
				name = item.id
				requires = item.requires
				context = item.context
			else
				error("Unknown content in page: " .. dump(item))
			end

			if check_requirements(name, requires, context) then
				if last_heading then
					content[#content + 1] = last_heading
					last_heading = nil
				end
				content[#content + 1] = item
			end
		end
	end

	-- Create components
	local retval = {}
	for i, item in ipairs(content) do
		if type(item) == "string" then
			local setting = get_setting_info(item)
			local component_func = component_funcs[setting.type]
			assert(component_func, "Unknown setting type: " .. setting.type)
			retval[i] = component_func(setting)
		elseif item.get_formspec then
			retval[i] = item
		elseif item.heading then
			retval[i] = component_funcs.heading(item.heading)
		end
	end
	return retval
end


local formspec_show_hack = false


local function get_formspec(dialogdata)
	local page_id = dialogdata.page_id or "accessibility"
	local page = filtered_page_by_id[page_id]

	local extra_h = 1 -- not included in tabsize.height
	local tabsize = {
		width = core.settings:get_bool("touch_gui") and 16.5 or 15.5,
		height = core.settings:get_bool("touch_gui") and (10 - extra_h) or 12,
	}

	local scrollbar_w = core.settings:get_bool("touch_gui") and 0.6 or 0.4

	local left_pane_width = core.settings:get_bool("touch_gui") and 4.5 or 4.25
	local left_pane_padding = 0.25
	local search_width = left_pane_width + scrollbar_w - (0.75 * 2)

	local back_w = 3
	local checkbox_w = (tabsize.width - back_w - 2*0.2) / 2
	local show_technical_names = core.settings:get_bool("show_technical_names")
	local show_advanced = core.settings:get_bool("show_advanced")

	formspec_show_hack = not formspec_show_hack

	local fs = {
		"formspec_version[6]",
		"size[", tostring(tabsize.width), ",", tostring(tabsize.height + extra_h), "]",
		core.settings:get_bool("touch_gui") and "padding[0.01,0.01]" or "",
		"bgcolor[#0000]",

		-- HACK: this is needed to allow resubmitting the same formspec
		formspec_show_hack and " " or "",

		"box[0,0;", tostring(tabsize.width), ",", tostring(tabsize.height), ";#0000008C]",

		("button[0,%f;%f,0.8;back;%s]"):format(
				tabsize.height + 0.2, back_w,
				fgettext("Back")),

		("box[%f,%f;%f,0.8;#0000008C]"):format(
			back_w + 0.2, tabsize.height + 0.2, checkbox_w),
		("checkbox[%f,%f;show_technical_names;%s;%s]"):format(
			back_w + 2*0.2, tabsize.height + 0.6,
			fgettext("Show technical names"), tostring(show_technical_names)),

		("box[%f,%f;%f,0.8;#0000008C]"):format(
			back_w + 2*0.2 + checkbox_w, tabsize.height + 0.2, checkbox_w),
		("checkbox[%f,%f;show_advanced;%s;%s]"):format(
			back_w + 3*0.2 + checkbox_w, tabsize.height + 0.6,
			fgettext("Show advanced settings"), tostring(show_advanced)),

		"field[0.25,0.25;", tostring(search_width), ",0.75;search_query;;",
			core.formspec_escape(dialogdata.query or ""), "]",
		"field_enter_after_edit[search_query;true]",
		"field_close_on_enter[search_query;false]", -- for pause menu env
		"container[", tostring(search_width + 0.25), ", 0.25]",
			"image_button[0,0;0.75,0.75;", core.formspec_escape(defaulttexturedir .. "search.png"), ";search;]",
			"image_button[0.75,0;0.75,0.75;", core.formspec_escape(defaulttexturedir .. "clear.png"), ";search_clear;]",
			"tooltip[search;", fgettext("Search"), "]",
			"tooltip[search_clear;", fgettext("Clear"), "]",
		"container_end[]",
		("scroll_container[0.25,1.25;%f,%f;leftscroll;vertical;0.1;0]"):format(
			left_pane_width, tabsize.height - 1.5),
		"style_type[button;border=false;bgcolor=#3333]",
		"style_type[button:hover;border=false;bgcolor=#6663]",
	}

	local y = 0
	local last_section = nil
	for _, other_page in ipairs(filtered_pages) do
		if other_page.section ~= last_section then
			fs[#fs + 1] = ("label[0.1,%f;%s]"):format(
				y + 0.41, core.colorize("#ff0", fgettext(other_page.section)))
			last_section = other_page.section
			y = y + 0.82
		end
		fs[#fs + 1] = ("box[0,%f;%f,0.8;%s]"):format(
			y, left_pane_width-left_pane_padding, other_page.id == page_id and "#467832FF" or "#3339")
		fs[#fs + 1] = ("button[0,%f;%f,0.8;page_%s;%s]")
			:format(y, left_pane_width-left_pane_padding, other_page.id, fgettext(other_page.title))
		y = y + 0.82
	end

	if #filtered_pages == 0 then
		fs[#fs + 1] = "label[0.1,0.41;"
		fs[#fs + 1] = fgettext("No results")
		fs[#fs + 1] = "]"
	end

	fs[#fs + 1] = "scroll_container_end[]"

	if y >= tabsize.height - 1.25 then
		fs[#fs + 1] = ("scrollbar[%f,1.25;%f,%f;vertical;leftscroll;%f]"):format(
				left_pane_width + 0.25, scrollbar_w, tabsize.height - 1.5, dialogdata.leftscroll or 0)
	end

	fs[#fs + 1] = "style_type[button;border=;bgcolor=]"

	if not dialogdata.components then
		dialogdata.components = page and build_page_components(page) or {}
	end

	local right_pane_width = tabsize.width - left_pane_width - 0.375 - 2*scrollbar_w - 0.25
	fs[#fs + 1] = ("scroll_container[%f,0;%f,%f;rightscroll;vertical;0.1;0.25]"):format(
			tabsize.width - right_pane_width - scrollbar_w, right_pane_width, tabsize.height)

	y = 0.25
	for i, comp in ipairs(dialogdata.components) do
		fs[#fs + 1] = ("container[0,%f]"):format(y)

		local avail_w = right_pane_width - 0.25
		if not comp.full_width then
			avail_w = avail_w - 1.4
		end
		if comp.max_w then
			avail_w = math.min(avail_w, comp.max_w)
		end

		local comp_fs, used_h = comp:get_formspec(avail_w)
		fs[#fs + 1] = comp_fs

		fs[#fs + 1] = "style_type[image_button;border=false;padding=]"

		local show_reset = comp.resettable and comp.setting
		local show_info = comp.info_text and comp.info_text ~= ""
		if show_reset or show_info then
			-- ensure there's enough space for reset/info
			used_h = math.max(used_h, 0.5)
		end
		local info_reset_y = used_h / 2 - 0.25

		if show_reset then
			local default = comp.setting.default
			local reset_tooltip = default and
					fgettext("Reset setting to default ($1)", tostring(default)) or
					fgettext("Reset setting to default")
			fs[#fs + 1] = ("image_button[%f,%f;0.5,0.5;%s;%s;]"):format(
					right_pane_width - 1.4, info_reset_y, reset_icon_path, "reset_" .. i)
			fs[#fs + 1] = ("tooltip[%s;%s]"):format("reset_" .. i, reset_tooltip)
		end

		if show_info then
			local info_x = right_pane_width - 0.75
			fs[#fs + 1] = ("image[%f,%f;0.5,0.5;%s]"):format(info_x, info_reset_y, info_icon_path)
			fs[#fs + 1] = ("tooltip[%f,%f;0.5,0.5;%s]"):format(info_x, info_reset_y, fgettext(comp.info_text))
		end

		fs[#fs + 1] = "style_type[image_button;border=;padding=]"

		fs[#fs + 1] = "container_end[]"

		if used_h > 0 then
			local spacing = 0.25
			local next_comp = dialogdata.components[i + 1]
			if next_comp and next_comp.spacing then
				spacing = next_comp.spacing
			end

			y = y + used_h + spacing
		end
	end

	fs[#fs + 1] = "scroll_container_end[]"

	if y >= tabsize.height then
		fs[#fs + 1] = ("scrollbar[%f,0;%f,%f;vertical;rightscroll;%f]"):format(
				tabsize.width - scrollbar_w, scrollbar_w, tabsize.height, dialogdata.rightscroll or 0)
	end

	return table.concat(fs, "")
end


-- On Android, closing the app via the "Recents screen" won't result in a clean
-- exit, discarding any setting changes made by the user.
-- To avoid that, we write the settings file in more cases on Android.
function write_settings_early()
	if PLATFORM == "Android" then
		core.settings:write()
	end
end

local function regenerate_page_list(dialogdata)
	local suggested_page_id = update_filtered_pages(dialogdata.query)

	dialogdata.components = nil

	if not filtered_page_by_id[dialogdata.page_id] then
		dialogdata.leftscroll = 0
		dialogdata.rightscroll = 0

		dialogdata.page_id = suggested_page_id
	end
end

local function buttonhandler(this, fields)
	local dialogdata = this.data
	dialogdata.leftscroll = core.explode_scrollbar_event(fields.leftscroll).value or dialogdata.leftscroll
	dialogdata.rightscroll = core.explode_scrollbar_event(fields.rightscroll).value or dialogdata.rightscroll
	dialogdata.query = fields.search_query

	-- "fields.quit" is for the pause menu env
	if fields.back or fields.quit then
		this:delete()
		return true
	end

	if fields.show_technical_names ~= nil then
		local value = core.is_yes(fields.show_technical_names)
		core.settings:set_bool("show_technical_names", value)
		write_settings_early()

		return true
	end

	if fields.show_advanced ~= nil then
		local value = core.is_yes(fields.show_advanced)
		core.settings:set_bool("show_advanced", value)
		write_settings_early()
		regenerate_page_list(dialogdata)

		return true
	end

	if fields.search or fields.key_enter_field == "search_query" then
		dialogdata.components = nil
		dialogdata.leftscroll = 0
		dialogdata.rightscroll = 0

		dialogdata.page_id = update_filtered_pages(dialogdata.query)

		return true
	end
	if fields.search_clear then
		dialogdata.query = ""
		dialogdata.components = nil
		dialogdata.leftscroll = 0
		dialogdata.rightscroll = 0

		dialogdata.page_id = update_filtered_pages("")
		return true
	end

	for _, page in ipairs(all_pages) do
		if fields["page_" .. page.id] then
			dialogdata.page_id = page.id
			dialogdata.components = nil
			dialogdata.rightscroll = 0
			return true
		end
	end

	local function after_setting_change(comp)
		write_settings_early()
		if comp.setting and comp.setting.name == "touch_controls" then
			-- Changing the "touch_controls" setting may result in a different
			-- page list.
			regenerate_page_list(dialogdata)
		else
			-- Clear components so they regenerate
			dialogdata.components = nil
		end
	end

	for i, comp in ipairs(dialogdata.components) do
		if comp.on_submit and comp:on_submit(fields, this) then
			after_setting_change(comp)
			return true
		end
		if comp.setting and fields["reset_" .. i] then
			core.settings:remove(comp.setting.name)
			after_setting_change(comp)
			return true
		end
	end

	return false
end


local function eventhandler(event)
	if event == "DialogShow" then
		-- Don't show the header image behind the dialog.
		mm_game_theme.set_engine(true)
		return true
	end
	if event == "FullscreenChange" then
		-- Refresh the formspec to keep the fullscreen checkbox up to date.
		ui.update()
		return true
	end

	return false
end


if INIT == "mainmenu" then
	function create_settings_dlg(page_id)
		load()
		local dlg = dialog_create("dlg_settings", get_formspec, buttonhandler, eventhandler)

		dlg.data.page_id = page_id or update_filtered_pages("")

		return dlg
	end

else
	assert(INIT == "pause_menu")

	local dialog

	core.register_on_formspec_input(function(formname, fields)
		if dialog and formname == "__builtin:settings" then
			-- buttonhandler returning true means we should update the formspec.
			-- dialog is re-checked since the buttonhandler may have closed it.
			if buttonhandler(dialog, fields) and dialog then
				core.show_formspec("__builtin:settings", get_formspec(dialog.data))
			end
			return true
		end
	end)

	core.open_settings = function()
		load()
		dialog = {}
		dialog.data = {}
		dialog.data.page_id = update_filtered_pages("")
		dialog.delete = function()
			dialog = nil
			-- only needed for the "fields.back" case, in the "fields.quit"
			-- case it's a no-op
			core.show_formspec("__builtin:settings", "")
		end

		core.show_formspec("__builtin:settings", get_formspec(dialog.data))
	end
end
