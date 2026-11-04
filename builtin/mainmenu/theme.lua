-- Luanti main menu visual theme.
-- Keep the palette here so main menu forms and shared FSTK components use the
-- same design tokens.

local theme = {}

local accent_palettes = {
	blue = { primary = "#89B4FA", secondary = "#B4BEFE" },
	lavender = { primary = "#B4BEFE", secondary = "#89B4FA" },
	cyan = { primary = "#89DCEB", secondary = "#89B4FA" },
	green = { primary = "#A6E3A1", secondary = "#94E2D5" },
	peach = { primary = "#FAB387", secondary = "#F9E2AF" },
	rose = { primary = "#F38BA8", secondary = "#F5C2E7" },
}

theme.colors = {
	base = "#1E1E2E",
	mantle = "#181825",
	surface = "#313244",
	elevated = "#45475A",
	text = "#CDD6F4",
	white = "#FFFFFF",
	subtext = "#A6ADC8",
	blue = "#89B4FA",
	lavender = "#B4BEFE",
	success = "#A6E3A1",
	warning = "#F9E2AF",
	error = "#F38BA8",
	info = "#89DCEB",
}

local function accent_surface(color)
	local red, green, blue = color:match("^#(%x%x)(%x%x)(%x%x)$")
	if not red then
		return "#313244"
	end
	local base_red, base_green, base_blue = 30, 30, 46
	local factor = 0.36
	return ("#%02X%02X%02X"):format(
		math.floor(base_red + (tonumber(red, 16) - base_red) * factor),
		math.floor(base_green + (tonumber(green, 16) - base_green) * factor),
		math.floor(base_blue + (tonumber(blue, 16) - base_blue) * factor))
end

function theme.refresh()
	local selected_accent = core.settings:get("mainmenu_accent_color") or "blue"
	local palette = accent_palettes[selected_accent] or accent_palettes.blue
	if selected_accent == "custom" then
		local custom_accent = core.settings:get("mainmenu_custom_accent_color") or "#89B4FA"
		if custom_accent:match("^#%x%x%x%x%x%x$") then
			palette = { primary = custom_accent, secondary = accent_palettes.blue.secondary }
		end
	end
	theme.colors.blue = palette.primary
	theme.colors.lavender = palette.secondary
	theme.colors.input = accent_surface(palette.primary)

	if core.settings:get("mainmenu_surface_contrast") == "high" then
		theme.colors.surface = theme.colors.elevated
	else
		theme.colors.surface = "#313244"
	end

	if mt_color_blue ~= nil then
		mt_color_blue = theme.colors.blue
		mt_color_lightblue = theme.colors.lavender
	end
end

local function texture(name)
	return core.formspec_escape(defaulttexturedir .. name)
end

function theme.formspec_styles()
	local c = theme.colors
	local button = texture("mm_button.png")
	local tab_button = texture("mm_tab.png")
	local styles = {
		("style_type[button;bgimg=%s;bgimg_middle=16;padding=-16,-16,16,16;bgcolor=%s;textcolor=%s;border=false]"):
			format(button, c.elevated, c.text),
		("style_type[button:hovered;bgcolor=%s;textcolor=%s]"):format(c.blue, c.base),
		("style_type[button:pressed;bgcolor=%s;textcolor=%s]"):format(c.lavender, c.base),
		("style_type[button:focused;bgcolor=%s;textcolor=%s]"):format(c.blue, c.base),
		("style_type[image_button;bgimg=%s;bgimg_middle=16;padding=-16,-16,16,16;bgcolor=%s;border=false]"):
			format(button, c.surface),
		("style_type[image_button:hovered;bgcolor=%s]"):format(c.blue),
		("style_type[image_button:pressed;bgcolor=%s]"):format(c.lavender),
		("style_type[field,pwdfield;textcolor=%s;bgcolor=%s]"):format(c.white, c.input),
		("style_type[textarea;textcolor=%s;border=false]"):format(c.white),
		("style_type[tabheader;bgimg=%s;bgimg_middle=16;bgcolor=%s;textcolor=%s;bgcolor_selected=%s;textcolor_selected=%s]"):
			format(tab_button, c.surface, c.white, c.blue, c.base),
		("style_type[label;textcolor=%s]"):format(c.white),
		("style_type[tooltip;bgimg=%s;bgimg_middle=16;bgcolor=%s;textcolor=%s;border=false]"):
			format(texture("mm_button.png"), c.surface, c.text),
		("style_type[hypertip;bgimg=%s;bgimg_middle=16;bgcolor=#0000;textcolor=%s;border=false]"):
			format(texture("mm_panel_elevated.png"), c.text),
	}

	-- Primary actions are visually distinct while preserving their field names.
	styles[#styles + 1] = ("style[play,world_create,world_create_confirm,world_create_open_cdb,btn_config_world_save,btn_contentdb,btn_mp_login,install,install_all;bgcolor=%s;textcolor=%s]"):
		format(c.blue, c.base)
	styles[#styles + 1] = ("style[play:hovered,world_create:hovered,world_create_confirm:hovered,world_create_open_cdb:hovered,btn_config_world_save:hovered,btn_contentdb:hovered,btn_mp_login:hovered,install:hovered,install_all:hovered;bgcolor=%s;textcolor=%s]"):
		format(c.lavender, c.base)
	styles[#styles + 1] = ("tableoptions[color=%s;background=%s;border=false;highlight=%s;highlight_text=%s]"):
		format(c.text, c.surface, c.blue, c.base)
	styles[#styles + 1] = ("listcolors[%s;%s;%s;%s;%s]"):format(
		c.surface, c.elevated, c.blue, c.surface, c.text)

	return table.concat(styles)
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

function theme.panel(x, y, width, height, tone)
	local panel_texture = (tone == "base" and "mm_panel_base.png") or
		(tone == "elevated" and "mm_panel_elevated.png") or "mm_panel.png"
	if tone == "surface" and core.settings:get("mainmenu_surface_contrast") == "high" then
		panel_texture = "mm_panel_elevated.png"
	end
	return ("background9[%f,%f;%f,%f;%s;false;16]"):format(
		x, y, width, height, texture(panel_texture))
end

return theme
