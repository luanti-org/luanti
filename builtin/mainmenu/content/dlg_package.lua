--Luanti
--Copyright (C) 2018-24 rubenwardy
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


local function get_info_formspec(size, padding, text)
	return table.concat({
		"formspec_version[6]",
		"size[", size.x, ",", size.y, "]",
		"padding[0,0]",
		"bgcolor[;true]",

		"label[4,4.35;", text, "]",
		"container[", padding.x, ",", size.y - 0.8 - padding.y, "]",
		"button[0,0;2,0.8;back;", fgettext("Back"), "]",
		"container_end[]",
	})
end


local function get_formspec(data)
	local window_padding =  contentdb.get_formspec_padding()
	local size = contentdb.get_formspec_size()
	size.x = math.min(size.x, 20)
	local W = size.x - window_padding.x * 2
	local H = size.y - window_padding.y * 2

	if not data.info then
		if not data.loading and not data.loading_error then
			data.loading = true

			contentdb.get_full_package_info(data.package, function(info)
				data.loading = false

				if info == nil then
					data.loading_error = true
					ui.update()
					return
				end

				assert(data.package.name == info.name)
				data.info = info
				ui.update()
			end)
		end

		-- get_full_package_info can return cached info immediately, so
		-- check to see if that happened
		if not data.info then
			if data.loading_error then
				return get_info_formspec(size, window_padding, fgettext("No packages could be retrieved"))
			end
			return get_info_formspec(size, window_padding, fgettext("Loading..."))
		end
	end

	-- Check installation status
	contentdb.update_paths()

	local info = data.info

	local info_line =
	fgettext("by $1  —  $2 downloads  —  +$3 / $4 / -$5",
			info.author, info.downloads,
			info.reviews.positive, info.reviews.neutral, info.reviews.negative)

	local bottom_buttons_y = H - 0.8

	local formspec = {
		"formspec_version[7]",
		"size[", size.x, ",",  size.y, "]",
		"padding[0,0]",
		"bgcolor[;true]",

		"container[", window_padding.x, ",", window_padding.y, "]",

		"button[0,", bottom_buttons_y, ";2,0.8;back;", fgettext("Back"), "]",
		"button[", W - 3, ",", bottom_buttons_y, ";3,0.8;open_contentdb;", fgettext("ContentDB page"), "]",

		"style_type[label;font_size=+24;font=bold]",
		"label[0,0.4;", core.formspec_escape(info.title), "]",
		"style_type[label;font_size=;font=]",

		"label[0,1.2;", core.formspec_escape(info_line), "]",
	}

	table.insert_all(formspec, {
		"container[", W - 6, ",0]"
	})

	local left_button_rect = "0,0;2.875,1"
	local right_button_rect = "3.125,0;2.875,1"
	if data.package.downloading then
		formspec[#formspec + 1] = "animated_image[5,0;1,1;downloading;"
		formspec[#formspec + 1] = core.formspec_escape(defaulttexturedir)
		formspec[#formspec + 1] = "cdb_downloading.png;3;400;]"
	elseif data.package.queued then
		formspec[#formspec + 1] = "style[queued;border=false]"
		formspec[#formspec + 1] = "image_button[5,0;1,1;" .. core.formspec_escape(defaulttexturedir)
		formspec[#formspec + 1] = "cdb_queued.png;queued;]"
	elseif not data.package.path then
		formspec[#formspec + 1] = "style[install;bgcolor=green]"
		formspec[#formspec + 1] = "button["
		formspec[#formspec + 1] = right_button_rect
		formspec[#formspec + 1] =";install;"
		formspec[#formspec + 1] = fgettext("Install [$1]", info.download_size)
		formspec[#formspec + 1] = "]"
	else
		if data.package.installed_release < data.package.release then
			-- The install_ action also handles updating
			formspec[#formspec + 1] = "style[install;bgcolor=#28ccdf]"
			formspec[#formspec + 1] = "button["
			formspec[#formspec + 1] = left_button_rect
			formspec[#formspec + 1] = ";install;"
			formspec[#formspec + 1] = fgettext("Update")
			formspec[#formspec + 1] = "]"
		end

		formspec[#formspec + 1] = "style[uninstall;bgcolor=#a93b3b]"
		formspec[#formspec + 1] = "button["
		formspec[#formspec + 1] = right_button_rect
		formspec[#formspec + 1] = ";uninstall;"
		formspec[#formspec + 1] = fgettext("Uninstall")
		formspec[#formspec + 1] = "]"
	end

	local current_tab = data.current_tab or 1
	local tab_titles = {
		fgettext("Description"),
		fgettext("Information"),
	}

	local tab_body_height = bottom_buttons_y - 2.8

	table.insert_all(formspec, {
		"container_end[]",

		"box[0,2.55;", W, ",", tab_body_height, ";#ffffff11]",

		"tabheader[0,2.55;", W, ",0.8;tabs;",
		table.concat(tab_titles, ","), ";", current_tab, ";true;true]",

		"container[0,2.8]",
	})

	if current_tab == 1 then
		-- Screenshots and description
		local hypertext = "<big><b>" .. core.hypertext_escape(info.short_description) .. "</b></big>\n"
		local winfo = core.get_window_info()
		local fs_to_px = winfo.size.x / winfo.max_formspec_size.x
		for i, ss in ipairs(info.screenshots) do
			local path = get_screenshot(data.package, ss.url, 2)
			hypertext = hypertext .. "<action name=\"ss_" .. i .. "\"><img name=\"" ..
					core.hypertext_escape(path) .. "\" width=" .. (3 * fs_to_px) ..
					" height=" .. (2 * fs_to_px) .. "></action>"
			if i ~= #info.screenshots then
				hypertext = hypertext .. "<img name=\"blank.png\" width=" .. (0.25 * fs_to_px) ..
						" height=" .. (2.25 * fs_to_px).. ">"
			end
		end
		hypertext = hypertext .. "\n" .. info.long_description.head

		local first = true
		local function add_link_button(label, name)
			if info[name] then
				if not first then
					hypertext = hypertext .. " | "
				end
				hypertext = hypertext .. "<action name=link_" .. name .. ">" .. core.hypertext_escape(label) .. "</action>"
				info.long_description.links["link_" .. name] = info[name]
				first = false
			end
		end

		add_link_button(fgettext("Donate"), "donate_url")
		add_link_button(fgettext("Website"), "website")
		add_link_button(fgettext("Source"), "repo")
		add_link_button(fgettext("Issue Tracker"), "issue_tracker")
		add_link_button(fgettext("Translate"), "translation_url")
		add_link_button(fgettext("Forum Topic"), "forum_url")

		hypertext = hypertext .. "\n\n" .. info.long_description.body

		hypertext = hypertext:gsub("<img name=\"?blank.png\"? ",
				"<img name=\"" .. core.hypertext_escape(defaulttexturedir) .. "blank.png\" ")

		table.insert_all(formspec, {
			"hypertext[0,0;", W, ",", tab_body_height - 0.375,
			";desc;", core.formspec_escape(hypertext), "]",

		})

	elseif current_tab == 2 then
		local hypertext = info.info_hypertext.head .. info.info_hypertext.body

		table.insert_all(formspec, {
			"hypertext[0,0;", W, ",", tab_body_height - 0.375,
			";info;", core.formspec_escape(hypertext), "]",
		})
	else
		error("Unknown tab " .. current_tab)
	end

	formspec[#formspec + 1] = "container_end[]"
	formspec[#formspec + 1] = "container_end[]"

	return table.concat(formspec)
end


local function handle_hypertext_event(this, event, hypertext_object)
	if not (event and event:sub(1, 7) == "action:") then
		return
	end

	for i, ss in ipairs(this.data.info.screenshots) do
		if event == "action:ss_" .. i then
			core.open_url(ss.url)
			return true
		end
	end

	local base_url = core.settings:get("contentdb_url"):gsub("(%W)", "%%%1")
	for key, url in pairs(hypertext_object.links) do
		if event == "action:" .. key then
			local author, name = url:match("^" .. base_url .. "/?packages/([A-Za-z0-9 _-]+)/([a-z0-9_]+)/?$")
			if author and name then
				local package2 = contentdb.get_package_by_info(author, name)
				if package2 then
					local dlg = create_package_dialog(package2)
					dlg:set_parent(this)
					this:hide()
					dlg:show()
					return true
				end
			end

			core.open_url_dialog(url)
			return true
		end
	end
end


local function handle_submit(this, fields)
	local info = this.data.info
	local package = this.data.package

	if fields.back then
		this:delete()
		return true
	end

	if not info then
		return false
	end

	if fields.open_contentdb then
		local url = ("%s/packages/%s/?protocol_version=%d"):format(
			core.settings:get("contentdb_url"), package.url_part,
			core.get_max_supp_proto())
		core.open_url(url)
		return true
	end

	if fields.install then
		install_or_update_package(this, package)
		return true
	end

	if fields.uninstall then
		local dlg = create_delete_content_dlg(package)
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		return true
	end

	if fields.tabs then
		this.data.current_tab = tonumber(fields.tabs)
		return true
	end

	if handle_hypertext_event(this, fields.desc, info.long_description) or
			handle_hypertext_event(this, fields.info, info.info_hypertext) then
		return true
	end
end


local function handle_events(event)
	if event == "WindowInfoChange" then
		ui.update()
		return true
	end

	return false
end


function create_package_dialog(package)
	assert(package)

	local dlg = dialog_create("package_dialog_" .. package.id,
			get_formspec,
			handle_submit,
			handle_events)
	local data = dlg.data

	data.package = package
	data.info = nil
	data.loading = false
	data.loading_error = nil
	data.current_tab = 1
	return dlg
end
