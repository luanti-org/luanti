-- Luanti
-- Copyright (C) 2018-24 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later


local function get_description_hypertext(package, info, loading_error)
	-- Screenshots and description
	local hypertext = "<big><b>" .. core.hypertext_escape(package.short_description) .. "</b></big>\n"

	local screenshots = info and info.screenshots or {{url = package.thumbnail}}

	local winfo = core.get_window_info()
	local fs_to_px = winfo.size.x / winfo.max_formspec_size.x
	for i, ss in ipairs(screenshots) do
		local path = get_screenshot(package, ss.url, 2)
		hypertext = hypertext .. "<action name=\"ss_".. i .. "\"><img name=\"" ..
				core.hypertext_escape(path) .. "\" width=" .. (3 * fs_to_px) ..
				" height=" .. (2 * fs_to_px) .. "></action>"
		if i ~= #screenshots then
			hypertext = hypertext .. "<img name=\"blank.png\" width=" .. (0.25 * fs_to_px) ..
					" height=" .. (2.25 * fs_to_px).. ">"
		end
	end

	if info then
		hypertext = hypertext .. "\n" .. info.long_description.head

		local first = true
		local function add_link_button(label, name)
			if info[name] then
				if not first then
					hypertext = hypertext .. " | "
				end
				hypertext = hypertext .. "<action name=link_" .. name .. ">" .. label .. "</action>"
				info.long_description.links["link_" .. name] = info[name]
				first = false
			end
		end

		add_link_button(hgettext("Donate"), "donate_url")
		add_link_button(hgettext("Website"), "website")
		add_link_button(hgettext("Source"), "repo")
		add_link_button(hgettext("Issue Tracker"), "issue_tracker")
		add_link_button(hgettext("Translate"), "translation_url")
		add_link_button(hgettext("Forum Topic"), "forum_url")

		hypertext = hypertext .. "\n\n" .. info.long_description.body

	elseif loading_error then
		hypertext = hypertext .. "\n\n" .. hgettext("Error loading package information")
	else
		hypertext = hypertext .. "\n\n" .. hgettext("Loading...")
	end

	-- Fix the path to blank.png. This is needed for bullet indentation,
	-- and also used for screenshot spacing.
	hypertext = hypertext:gsub("<img name=\"?blank.png\"? ",
			"<img name=\"" .. core.hypertext_escape(defaulttexturedir) .. "blank.png\" ")

	return hypertext
end


local function get_formspec(data)
	local package = data.package
	local window_padding =  contentdb.get_formspec_padding()
	local size = contentdb.get_formspec_size()
	size.x = math.min(size.x, 20)
	local W = size.x - window_padding.x * 2
	local H = size.y - window_padding.y * 2

	if not data.info then
		if not data.loading and not data.loading_error then
			data.loading = true

			contentdb.get_full_package_info(package, function(info)
				data.loading = false

				if info == nil then
					data.loading_error = true
					ui.update()
					return
				end

				assert(data.package.name == info.name)
				data.info = info
				-- note: get_full_package_info can also return cached info immediately
				ui.update()
			end)
		end
	end

	-- Check installation status
	contentdb.update_paths()

	local info = data.info

	local info_line
	if info then
		info_line = fgettext_ne("by $1  —  $2 downloads  —  +$3 / $4 / -$5",
			info.author, info.downloads,
			info.reviews.positive, info.reviews.neutral, info.reviews.negative)
	else
		info_line = fgettext_ne("by $1", package.author)
	end

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
		"label[0,0.4;", core.formspec_escape(package.title), "]",
		"style_type[label;font_size=;font=]",

		"label[0,1.2;", core.formspec_escape(info_line), "]",
	}

	table.insert_all(formspec, {
		"container[", W - 6, ",0]"
	})

	local left_button_rect = "0,0;2.875,1"
	local right_button_rect = "3.125,0;2.875,1"
	if package.downloading then
		formspec[#formspec + 1] = "animated_image[5,0;1,1;downloading;"
		formspec[#formspec + 1] = core.formspec_escape(defaulttexturedir)
		formspec[#formspec + 1] = "cdb_downloading.png;3;400;]"
	elseif package.queued then
		formspec[#formspec + 1] = "style[queued;border=false]"
		formspec[#formspec + 1] = "image_button[5,0;1,1;" .. core.formspec_escape(defaulttexturedir)
		formspec[#formspec + 1] = "cdb_queued.png;queued;]"
	elseif not package.path then
		local label = info and fgettext("Install [$1]", info.download_size) or
			fgettext("Install")
		formspec[#formspec + 1] = "style[install;bgcolor=green]"
		formspec[#formspec + 1] = "button["
		formspec[#formspec + 1] = right_button_rect
		formspec[#formspec + 1] =";install;"
		formspec[#formspec + 1] = label
		formspec[#formspec + 1] = "]"
	else
		if package.installed_release < package.release then
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
	}
	if info then
		local review_count = info.reviews.positive + info.reviews.neutral + info.reviews.negative
		table.insert(tab_titles, fgettext("Information"))
		table.insert(tab_titles, fgettext("Reviews") .. core.formspec_escape(" [" .. review_count .. "]"))
	end

	local tab_body_height = bottom_buttons_y - 2.8

	table.insert_all(formspec, {
		"container_end[]",

		"box[0,2.55;", W, ",", tab_body_height, ";#ffffff11]",

		"tabheader[0,2.55;", W, ",0.8;tabs;",
		table.concat(tab_titles, ","), ";", current_tab, ";true;true]",

		"container[0,2.8]",
	})

	if current_tab == 1 then
		local hypertext = get_description_hypertext(package, info, data.loading_error)
		table.insert_all(formspec, {
			"hypertext[0,0;", W, ",", tab_body_height - 0.375,
			";desc;", core.formspec_escape(hypertext), "]",
		})

	elseif current_tab == 2 then
		assert(info)
		local hypertext = info.info_hypertext.head .. info.info_hypertext.body
		table.insert_all(formspec, {
			"hypertext[0,0;", W, ",", tab_body_height - 0.375,
			";info;", core.formspec_escape(hypertext), "]",
		})
	elseif current_tab == 3 then
		assert(info)
		if not package.reviews and not data.reviews_error and not data.reviews_loading then
			data.reviews_loading = true

			contentdb.get_package_reviews(package, function(reviews)
				if not reviews then
					data.reviews_error = true
				end
				ui.update()
			end)
		end

		if package.reviews then
			local hypertext = package.reviews.head .. package.reviews.body
			-- Provide correct path to blank.png image. This is needed for bullet indentation.
			hypertext = hypertext:gsub("<img name=\"?blank.png\"? ",
					"<img name=\"" .. core.hypertext_escape(defaulttexturedir) .. "blank.png\" ")
			-- Placeholders in reviews hypertext for icons
			hypertext = hypertext:gsub("<thumbsup>",
					"<img name=\"" .. core.hypertext_escape(defaulttexturedir) .. "contentdb_thumb_up.png\" width=24>")
			hypertext = hypertext:gsub("<thumbsdown>",
					"<img name=\"" .. core.hypertext_escape(defaulttexturedir) .. "contentdb_thumb_down.png\" width=24>")
			hypertext = hypertext:gsub("<neutral>",
					"<img name=\"" .. core.hypertext_escape(defaulttexturedir) .. "contentdb_neutral.png\" width=24>")
			table.insert_all(formspec, {
				"hypertext[0,0;", W, ",", tab_body_height - 0.375,
				";reviews;", core.formspec_escape(hypertext), "]",
			})
		elseif data.reviews_error then
			table.insert_all(formspec, {"label[2,2;", fgettext("Error loading reviews"), "]"} )
		else
			table.insert_all(formspec, {"label[2,2;", fgettext("Loading..."), "]"} )
		end
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

	if fields.open_contentdb then
		local version = core.get_version()
		local url = core.settings:get("contentdb_url") .. "/packages/" .. package.url_part ..
				"/?protocol_version=" .. core.urlencode(core.get_max_supp_proto()) ..
				"&engine_version=" .. core.urlencode(version.string)
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

	-- The events handled below are only valid if the package info has finished
	-- loading.
	if not info then
		return false
	end

	if fields.tabs then
		this.data.current_tab = tonumber(fields.tabs)
		return true
	end

	if handle_hypertext_event(this, fields.desc, info.long_description) or
			handle_hypertext_event(this, fields.info, info.info_hypertext) or
			(package.reviews and handle_hypertext_event(this, fields.reviews, package.reviews)) then
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
