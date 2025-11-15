-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

local prelude_theme = ui.Style {
	ui.Style "root" {
		pos = {1/2},
		size = {0},
		anchor = {1/2},

		clip = "overflow",

		ui.Style "@backdrop" {
			truncate = "both",
			hidden = true,
		},
		ui.Style "@backdrop$focused" {
			hidden = false,
		},
	},
}

function ui.get_prelude_theme()
	return prelude_theme
end

local default_theme = prelude_theme

function ui.get_default_theme()
	return default_theme
end

function ui.set_default_theme(theme)
	default_theme = ui._req(theme, ui.Style)
end
