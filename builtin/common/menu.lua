-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later

-- These colors are used by the main menu and the settings menu
mt_color_grey  = "#AAAAAA"
mt_color_blue  = "#6389FF"
mt_color_lightblue  = "#99CCFF"
mt_color_green = "#72FF63"
mt_color_dark_green = "#25C191"
mt_color_orange  = "#FF8800"
mt_color_red = "#FF3300"

function core.are_keycodes_equal(k1, k2)
	return core.normalize_keycode(k1) == core.normalize_keycode(k2)
end
