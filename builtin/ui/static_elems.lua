-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2024 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

ui.Group = ui.derive_elem(ui.Elem, "group")
ui.Label = ui.derive_elem(ui.Elem, "label")
ui.Image = ui.derive_elem(ui.Elem, "image")

ui.Root = ui._new_type(ui.Elem, "root", 0x01, false)

function ui.Root:_init(props)
	ui.Elem._init(self, props)

	self._boxes.backdrop = true
end

function ui.Root:_encode_fields()
	local fl = ui._make_flags()

	self:_encode_box(fl, self._boxes.backdrop)

	return ui._encode("SZ", ui.Elem._encode_fields(self), ui._encode_flags(fl))
end
