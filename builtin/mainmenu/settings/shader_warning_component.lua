-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later

return {
	query_text = "Shaders",
	requires = {
		shaders_support = true,
        shaders = false,
	},
    full_width = true,
	get_formspec = function(self, avail_w)
        local fs = {
            "box[0,0;", avail_w, ",1.2;", mt_color_orange, "]",
			"label[0.2,0.4;", fgettext("Shaders are disabled."), "]",
			"label[0.2,0.8;", fgettext("This is not a recommended configuration."), "]",
			"button[", avail_w - 2.2, ",0.2;2,0.8;fix_shader_warning;", fgettext("Enable"), "]",
        }
		return table.concat(fs, ""), 1.2
	end,
	on_submit = function(self, fields)
		if fields.fix_shader_warning then
			core.settings:remove("enable_shaders") -- default value is true
			return true
		end
	end,
}
