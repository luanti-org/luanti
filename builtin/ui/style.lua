-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

ui.Style = core.class()

function ui.Style:new(param)
	local function make_style(props)
		self._props = ui._cascade_props(props.props or props, {})
		self._nested = table.merge(props.nested or props)
		self._reset = props.reset

		return self
	end

	if type(param) == "string" then
		self._sel = ui._parse_sel(param)
		return make_style
	end

	self._sel = ui._universal_sel
	return make_style(param)
end

function ui.Style:_get_flat()
	local flat_styles = {}
	self:_get_flat_impl(flat_styles, ui._universal_sel)
	return flat_styles
end

function ui.Style:_get_flat_impl(flat_styles, parent_sel)
	-- Intersect our selector with our parent selector, resulting in a fully
	-- qualified selector.
	local full_sel = ui._intersect_sels({parent_sel, self._sel})

	-- Copy this style's properties into a new style with the full selector.
	local flat = ui.Style {
		props = self._props,
		reset = self._reset,
	}
	flat._sel = full_sel

	table.insert(flat_styles, flat)

	-- For each sub-style of this style, cascade it with our full selector and
	-- add it to the list of flat styles.
	for _, nested in ipairs(self._nested) do
		nested:_get_flat_impl(flat_styles, full_sel)
	end
end

local function cascade_layout(new, add, props)
	new.layout = add.layout or props.layout
	new.clip = add.clip or props.clip

	new.scale = add.scale or props.scale
end

local function cascade_sizing(new, add, props)
	new.size = add.size or props.size
	new.span = add.span or props.span

	new.pos = add.pos or props.pos
	new.anchor = add.anchor or props.anchor

	new.margin = add.margin or props.margin
	new.padding = add.padding or props.padding
end

local function cascade_layer(new, add, props, p)
	new[p.."_image"] = add[p.."_image"] or props[p.."_image"]
	new[p.."_fill"] = add[p.."_fill"] or props[p.."_fill"]
	new[p.."_tint"] = add[p.."_tint"] or props[p.."_tint"]

	new[p.."_scale"] = add[p.."_scale"] or props[p.."_scale"]
	new[p.."_source"] = add[p.."_source"] or props[p.."_source"]

	new[p.."_frames"] = add[p.."_frames"] or props[p.."_frames"]
	new[p.."_frame_time"] = add[p.."_frame_time"] or props[p.."_frame_time"]
end

function ui._cascade_props(add, props)
	local new = {}

	cascade_layout(new, add, props)
	cascade_sizing(new, add, props)

	new.display = add.display or props.display

	cascade_layer(new, add, props, "box")
	cascade_layer(new, add, props, "icon")

	new.box_middle = add.box_middle or props.box_middle
	new.box_tile = add.box_tile or props.box_tile

	new.icon_place = add.icon_place or props.icon_place
	new.icon_gutter = add.icon_gutter or props.icon_gutter
	new.icon_overlap = ui._apply_bool(add.icon_overlap, props.icon_overlap)

	return new
end

local function unpack_vec2d(vec)
	if #vec == 2 then
		return vec[1], vec[2]
	elseif #vec == 1 then
		return vec[1], vec[1]
	end
end

local function unpack_rect(rect)
	if #rect == 4 then
		return rect[1], rect[2], rect[3], rect[4]
	elseif #rect == 2 then
		return rect[1], rect[2], rect[1], rect[2]
	elseif #rect == 1 then
		return rect[1], rect[1], rect[1], rect[1]
	end
end

local layout_type_map = {
	place = 0,
}

local dir_flags_map = {
	none = 0,
	x = 1,
	y = 2,
	both = 3,
}

local display_mode_map = {
	visible = 0,
	overflow = 1,
	hidden = 2,
	clipped = 3,
}

local icon_place_map = {
	center = 0,
	left = 1,
	top = 2,
	right = 3,
	bottom = 4,
}

local function encode_layout(props)
	local fl = ui._make_flags()

	if ui._shift_flag(fl, props.layout) then
		ui._encode_flag(fl, "B", layout_type_map[props.layout])
	end
	if ui._shift_flag(fl, props.clip) then
		ui._encode_flag(fl, "B", dir_flags_map[props.clip])
	end

	if ui._shift_flag(fl, props.scale) then
		ui._encode_flag(fl, "f", props.scale)
	end

	return fl
end

local function encode_sizing(props)
	local fl = ui._make_flags()

	if ui._shift_flag(fl, props.size) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.size))
	end
	if ui._shift_flag(fl, props.span) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.span))
	end

	if ui._shift_flag(fl, props.pos) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.pos))
	end
	if ui._shift_flag(fl, props.anchor) then
		ui._encode_flag(fl, "ff", unpack_vec2d(props.anchor))
	end

	if ui._shift_flag(fl, props.margin) then
		ui._encode_flag(fl, "ffff", unpack_rect(props.margin))
	end
	if ui._shift_flag(fl, props.padding) then
		ui._encode_flag(fl, "ffff", unpack_rect(props.padding))
	end

	return fl
end

local function encode_layer(props, p)
	local fl = ui._make_flags()

	if ui._shift_flag(fl, props[p.."_image"]) then
		ui._encode_flag(fl, "z", props[p.."_image"])
	end
	if ui._shift_flag(fl, props[p.."_fill"]) then
		ui._encode_flag(fl, "I", core.colorspec_to_int(props[p.."_fill"]))
	end
	if ui._shift_flag(fl, props[p.."_tint"]) then
		ui._encode_flag(fl, "I", core.colorspec_to_int(props[p.."_tint"]))
	end

	if ui._shift_flag(fl, props[p.."_scale"]) then
		ui._encode_flag(fl, "f", props[p.."_scale"])
	end
	if ui._shift_flag(fl, props[p.."_source"]) then
		ui._encode_flag(fl, "ffff", unpack_rect(props[p.."_source"]))
	end

	if ui._shift_flag(fl, props[p.."_frames"]) then
		ui._encode_flag(fl, "I", props[p.."_frames"])
	end
	if ui._shift_flag(fl, props[p.."_frame_time"]) then
		ui._encode_flag(fl, "I", props[p.."_frame_time"])
	end

	return fl
end

local function encode_subflags(fl, sub_fl)
	if ui._shift_flag(fl, sub_fl.flags ~= 0) then
		ui._encode_flag(fl, "s", ui._encode_flags(sub_fl))
	end
end

function ui._encode_props(props)
	local fl = ui._make_flags()

	encode_subflags(fl, encode_layout(props))
	encode_subflags(fl, encode_sizing(props))

	if ui._shift_flag(fl, props.display) then
		ui._encode_flag(fl, "B", display_mode_map[props.display])
	end

	encode_subflags(fl, encode_layer(props, "box"))
	encode_subflags(fl, encode_layer(props, "icon"))

	if ui._shift_flag(fl, props.box_middle) then
		ui._encode_flag(fl, "ffff", unpack_rect(props.box_middle))
	end
	if ui._shift_flag(fl, props.box_tile) then
		ui._encode_flag(fl, "B", dir_flags_map[props.box_tile])
	end

	if ui._shift_flag(fl, props.icon_place) then
		ui._encode_flag(fl, "B", icon_place_map[props.icon_place])
	end
	if ui._shift_flag(fl, props.icon_gutter) then
		ui._encode_flag(fl, "f", props.icon_gutter)
	end
	ui._shift_flag_bool(fl, props.icon_overlap)

	return ui._encode("s", ui._encode_flags(fl))
end
