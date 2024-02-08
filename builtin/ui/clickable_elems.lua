-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2024 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

ui.Button = ui._new_type(ui.Elem, "button", 0x02, true)

function ui.Button:_init(props)
	ui.Elem._init(self, props)

	self._disabled = props.disabled
	self._on_press = props.on_press
end

function ui.Button:_encode_fields()
	local fl = ui._make_flags()

	ui._shift_flag(fl, self._disabled)
	ui._shift_flag(fl, self._on_press)

	return ui._encode("SZ", ui.Elem._encode_fields(self), ui._encode_flags(fl))
end

ui.Button._handlers[0x00] = function(self, ev, data)
	return self._on_press
end

ui.Toggle = ui._new_type(ui.Elem, "toggle", 0x03, true)

ui.Check  = ui.derive_elem(ui.Toggle, "check")
ui.Switch = ui.derive_elem(ui.Toggle, "switch")

function ui.Toggle:_init(props)
	ui.Elem._init(self, props)

	self._disabled = props.disabled
	self._selected = props.selected

	self._on_press = props.on_press
	self._on_change = props.on_change
end

function ui.Toggle:_encode_fields()
	local fl = ui._make_flags()

	ui._shift_flag(fl, self._disabled)
	ui._shift_flag_bool(fl, self._selected)

	ui._shift_flag(fl, self._on_press)
	ui._shift_flag(fl, self._on_change)

	return ui._encode("SZ", ui.Elem._encode_fields(self), ui._encode_flags(fl))
end

ui.Toggle._handlers[0x00] = function(self, ev, data)
	return self._on_press
end

ui.Toggle._handlers[0x01] = function(self, ev, data)
	local selected = ui._decode("B", data)
	ev.selected = selected ~= 0

	return self._on_change
end

ui.Option = ui._new_type(ui.Elem, "option", 0x04, true)

ui.Radio = ui.derive_elem(ui.Option, "radio")

function ui.Option:_init(props)
	ui.Elem._init(self, props)

	self._disabled = props.disabled
	self._family = props.family

	self._selected = props.selected

	self._on_press = props.on_press
	self._on_change = props.on_change

	if self._family then
		assert(ui.is_id(self._family), "Element family must be an ID string")
	end
end

function ui.Option:_encode_fields()
	local fl = ui._make_flags()

	ui._shift_flag(fl, self._disabled)
	ui._shift_flag_bool(fl, self._selected)

	if ui._shift_flag(fl, self._family) then
		ui._encode_flag(fl, "z", self._family)
	end

	ui._shift_flag(fl, self._on_press)
	ui._shift_flag(fl, self._on_change)

	return ui._encode("SZ", ui.Elem._encode_fields(self), ui._encode_flags(fl))
end

ui.Option._handlers[0x00] = function(self, ev, data)
	return self._on_press
end

ui.Option._handlers[0x01] = function(self, ev, data)
	local selected = ui._decode("B", data)
	ev.selected = selected ~= 0

	return self._on_change
end
