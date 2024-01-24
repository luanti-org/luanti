-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

ui = {}

local UI_PATH = core.get_builtin_path() .. "ui" .. DIR_DELIM

dofile(UI_PATH .. "util.lua")
dofile(UI_PATH .. "selector.lua")
dofile(UI_PATH .. "style.lua")
dofile(UI_PATH .. "elem.lua")

dofile(UI_PATH .. "static_elems.lua")

dofile(UI_PATH .. "window.lua")
dofile(UI_PATH .. "theme.lua")
