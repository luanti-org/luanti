local theme = {}

local default_theme = "catppuccin_mocha"
local default_accent = "blue"

local themes = {
	catppuccin_mocha = {
		base = "#1E1E2E",
		mantle = "#181825",
		surface = "#313244",
		elevated = "#45475A",
		text = "#CDD6F4",
		subtext = "#A6ADC8",
		accents = {
			blue = "#89B4FA",
			lavender = "#B4BEFE",
			cyan = "#89DCEB",
			green = "#A6E3A1",
			peach = "#FAB387",
			rose = "#F38BA8",
		},
		success = "#A6E3A1",
		warning = "#F9E2AF",
		error = "#F38BA8",
		info = "#89DCEB",
	},
	catppuccin_macchiato = {
		base = "#24273A",
		mantle = "#1E2030",
		surface = "#363A4F",
		elevated = "#494D64",
		text = "#CAD3F5",
		subtext = "#A5ADCB",
		accents = {
			blue = "#8AADF4",
			lavender = "#B7BDF8",
			cyan = "#91D7E3",
			green = "#A6DA95",
			peach = "#F5A97F",
			rose = "#ED8796",
		},
		success = "#A6DA95",
		warning = "#EED49F",
		error = "#ED8796",
		info = "#91D7E3",
	},
	catppuccin_frappe = {
		base = "#303446",
		mantle = "#292C3C",
		surface = "#414559",
		elevated = "#51576D",
		text = "#C6D0F5",
		subtext = "#A5ADCE",
		accents = {
			blue = "#8CAAEE",
			lavender = "#BABBF1",
			cyan = "#81C8BE",
			green = "#A6D189",
			peach = "#EF9F76",
			rose = "#E78284",
		},
		success = "#A6D189",
		warning = "#E5C890",
		error = "#E78284",
		info = "#81C8BE",
	},
	catppuccin_latte = {
		base = "#EFF1F5",
		mantle = "#E6E9EF",
		surface = "#DCE0E8",
		elevated = "#CCD0DA",
		text = "#4C4F69",
		subtext = "#6C6F85",
		accents = {
			blue = "#1E66F5",
			lavender = "#7287FD",
			cyan = "#179299",
			green = "#40A02B",
			peach = "#FE640B",
			rose = "#D20F39",
		},
		success = "#40A02B",
		warning = "#DF8E1D",
		error = "#D20F39",
		info = "#179299",
	},
	dracula = {
		base = "#282A36",
		mantle = "#21222C",
		surface = "#44475A",
		elevated = "#6272A4",
		text = "#F8F8F2",
		subtext = "#BFBFBF",
		accents = {
			blue = "#8BE9FD",
			lavender = "#BD93F9",
			cyan = "#8BE9FD",
			green = "#50FA7B",
			peach = "#FFB86C",
			rose = "#FF79C6",
		},
		success = "#50FA7B",
		warning = "#F1FA8C",
		error = "#FF5555",
		info = "#8BE9FD",
	},
	nord = {
		base = "#2E3440",
		mantle = "#242933",
		surface = "#3B4252",
		elevated = "#434C5E",
		text = "#ECEFF4",
		subtext = "#D8DEE9",
		accents = {
			blue = "#88C0D0",
			lavender = "#B48EAD",
			cyan = "#8FBCBB",
			green = "#A3BE8C",
			peach = "#D08770",
			rose = "#BF616A",
		},
		success = "#A3BE8C",
		warning = "#EBCB8B",
		error = "#BF616A",
		info = "#88C0D0",
	},
	gruvbox_dark = {
		base = "#282828",
		mantle = "#1D2021",
		surface = "#3C3836",
		elevated = "#504945",
		text = "#EBDBB2",
		subtext = "#BDAE93",
		accents = {
			blue = "#83A598",
			lavender = "#D3869B",
			cyan = "#8EC07C",
			green = "#B8BB26",
			peach = "#FE8019",
			rose = "#FB4934",
		},
		success = "#B8BB26",
		warning = "#FABD2F",
		error = "#FB4934",
		info = "#83A598",
	},
	tokyo_night = {
		base = "#1A1B26",
		mantle = "#16161E",
		surface = "#292E42",
		elevated = "#3B4261",
		text = "#C0CAF5",
		subtext = "#A9B1D6",
		accents = {
			blue = "#7AA2F7",
			lavender = "#BB9AF7",
			cyan = "#7DCFFF",
			green = "#9ECE6A",
			peach = "#FF9E64",
			rose = "#F7768E",
		},
		success = "#9ECE6A",
		warning = "#E0AF68",
		error = "#F7768E",
		info = "#7DCFFF",
	},
	one_dark = {
		base = "#282C34",
		mantle = "#21252B",
		surface = "#3A404C",
		elevated = "#4B5263",
		text = "#ABB2BF",
		subtext = "#818896",
		accents = {
			blue = "#61AFEF",
			lavender = "#C678DD",
			cyan = "#56B6C2",
			green = "#98C379",
			peach = "#D19A66",
			rose = "#E06C75",
		},
		success = "#98C379",
		warning = "#E5C07B",
		error = "#E06C75",
		info = "#56B6C2",
	},
}

theme.themes = themes
theme.colors = {}

local function valid_color(value)
	if type(value) ~= "string" then
		return false
	end
	value = value:match("^%s*(.-)%s*$")
	return value:match("^#%x%x%x%x%x%x$") ~= nil
end

local function trim(value)
	if type(value) ~= "string" then
		return ""
	end
	return value:match("^%s*(.-)%s*$") or ""
end

function theme.refresh()
	local radius = tonumber(core.settings:get("mainmenu_corner_radius")) or 6
	theme.corner_radius = math.max(0, math.min(20, math.floor(radius)))

	local padding = tonumber(core.settings:get("mainmenu_padding")) or 2
	theme.padding = math.max(0, math.min(20, math.floor(padding)))

	local theme_name = core.settings:get("mainmenu_theme") or default_theme
	local preset = themes[theme_name] or themes[default_theme]
	local accent = core.settings:get("mainmenu_accent_color") or default_accent
	local primary = preset.accents[accent] or preset.accents[default_accent]
	local secondary = preset.accents.lavender

	if accent == "custom" then
		local custom = trim(core.settings:get("mainmenu_custom_accent_color"))
		if valid_color(custom) then
			primary = custom:upper()
		end
	end

	local text = preset.text
	local custom_text = trim(core.settings:get("mainmenu_text_color"))
	if valid_color(custom_text) then
		text = custom_text:upper()
	end

	local surface = preset.surface
	if core.settings:get("mainmenu_surface_contrast") == "high" then
		surface = preset.elevated
	end

	local colors = {
		base = preset.base,
		mantle = preset.mantle,
		surface = surface,
		elevated = preset.elevated,
		text = text,
		subtext = preset.subtext,
		blue = primary,
		lavender = secondary,
		success = preset.success,
		warning = preset.warning,
		error = preset.error,
		info = preset.info,
	}

	theme.colors = colors
	theme.theme_name = theme_name
	theme.accent_name = accent

	mt_color_grey = colors.subtext
	mt_color_blue = colors.blue
	mt_color_lightblue = colors.lavender
	mt_color_green = colors.success
	mt_color_dark_green = colors.success
	mt_color_orange = colors.warning
	mt_color_red = colors.error

	theme.apply_skin_colors(colors)
	if core.set_gui_skin_rounding then
		core.set_gui_skin_rounding(theme.corner_radius or 0)
	end
end

function theme.apply_skin_colors(colors)
	if not core.set_gui_skin_colors then
		return
	end

	core.set_gui_skin_colors({
		Window = colors.base,
		WindowSymbol = colors.text,
		ButtonText = colors.text,
		GrayText = colors.subtext,
		Highlight = colors.blue,
		HighlightText = colors.text,
		Editable = colors.surface,
		GrayEditable = colors.mantle,
		FocusedEditable = colors.elevated,
		ScrollBar = colors.surface,
		["3DFace"] = colors.surface,
		["3DHighlight"] = colors.elevated,
		["3DShadow"] = colors.mantle,
		["3DDarkShadow"] = colors.mantle,
		ToolTip = colors.text,
		ToolTipBackground = colors.surface,
		Icon = colors.text,
		IconHighlight = colors.text,
		GrayWindowSymbol = colors.subtext,
	})
end

function theme.formspec_styles()
	local c = theme.colors
	local r = theme.corner_radius or 0
	local p = theme.padding or 0
	return table.concat({
		("style_type[box;borderradius=%d]"):format(r),
		("style_type[button;bgcolor=%s;textcolor=%s;borderradius=%d]"):
			format(c.surface, c.text, r),
		("style_type[button:hovered;border=true;bgcolor=%s;textcolor=%s]"):
			format(c.blue, c.text),
		("style_type[button:pressed;border=true;bgcolor=%s;textcolor=%s]"):
			format(c.lavender, c.text),
		("style_type[button:focused;border=true;bgcolor=%s;textcolor=%s]"):
			format(c.blue, c.text),
		("style_type[image_button;bgcolor=%s;borderradius=%d]"):format(c.surface, r),
		("style_type[image_button:hovered;border=true;bgcolor=%s]"):format(c.blue),
		("style_type[image_button:pressed;border=true;bgcolor=%s]"):format(c.lavender),
		("style_type[tabheader;bgcolor=%s;textcolor=%s;bgcolor_selected=%s;textcolor_selected=%s;bgcolor_hovered=%s;borderradius=%d;padding=%d]"):
			format(c.surface, c.text, c.blue, c.base, c.elevated, r, p),
		("style_type[scrollbar;bgcolor=%s;fgcolor=%s;borderradius=%d]"):
			format(c.surface, c.blue, r),
		("style_type[checkbox;borderradius=%d]"):format(r),
		("style_type[dropdown;borderradius=%d]"):format(r),
		("style_type[table,textlist;borderradius=%d]"):format(r),
		("style_type[field,pwdfield,textarea;textcolor=%s;bgcolor=%s;borderradius=%d]"):
			format(c.text, c.surface, r),
		("style_type[label;textcolor=%s]"):format(c.text),
		("style_type[tooltip;bgcolor=%s;textcolor=%s;borderradius=%d]"):
			format(c.surface, c.text, r),
		("style_type[hypertip;bgcolor=%s;textcolor=%s;border=false]"):format(c.surface, c.text),
		("listcolors[%s;%s;%s;%s;%s]"):format(
			c.surface, c.elevated, c.blue, c.surface, c.text),
	})
end

--! Spacing between main menu controls, in formspec units.
function theme.button_gap()
	return (theme.padding or 0) * 0.05
end

function theme.inject_formspec(formspec)
	local styles = theme.formspec_styles()
	local cursor = 1
	local insertion = 0
	local header_tags = {
		formspec_version = true,
		size = true,
		anchor = true,
		real_coordinates = true,
		padding = true,
		position = true,
		bgcolor = true,
	}

	while cursor <= #formspec do
		local tag = formspec:sub(cursor):match("^([%w_]+%[[^%]]*%])")
		if not tag then
			break
		end
		local name = tag:match("^([%w_]+)%[")
		if not header_tags[name] then
			break
		end
		insertion = cursor + #tag - 1
		cursor = insertion + 1
	end

	if insertion == 0 then
		return styles .. formspec
	end
	return formspec:sub(1, insertion) .. styles .. formspec:sub(insertion + 1)
end

return theme
