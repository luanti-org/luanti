-- Test nodes for text_face feature (#1367)
-- Tests text rendering on various node drawtypes.

testtextface = {}

local ESC = string.char(0x1b) -- needed for parsing functions below

local default_text = function(label)
	return core.get_background_escape_sequence("#00000080")
		.. core.colorize("#ffffff", label)
end

-- Shared right-click text editor
local FORMNAME = "testtextface:edit"

-- Strip escape sequences for display in formspec
local function strip_escapes(s)
	return core.strip_escapes(s)
end

-- Extract style prefix (all leading escape sequences before first printable char)
local function get_style_prefix(s)
	local prefix = ""
	local i = 1
	while i <= #s do
		if s:byte(i) == 0x1b and s:sub(i+1, i+1) == "(" then
			local j = s:find(")", i+2, true)
			if j then
				prefix = prefix .. s:sub(i, j)
				i = j + 1
			else
				break
			end
		else
			break
		end
	end
	return prefix
end

-- Parse style values from the escape prefix
local function parse_style(s)
	local style = { size = "", font = "normal", weight = "normal",
		color = "#ffffff", bgcolor = "", align = "left" }
	for key, val in s:gmatch(ESC .. "%((%w)@([^%)]+)%)") do
		if key == "s" then style.size = val
		elseif key == "f" then style.font = val
		elseif key == "w" then style.weight = val
		elseif key == "c" then style.color = val
		elseif key == "b" then style.bgcolor = val
		elseif key == "a" then style.align = val
		end
	end
	return style
end

-- Build escape prefix from style values
local function build_style(style)
	local s = ""
	if style.size ~= "" then
		s = s .. core.get_font_size_escape_sequence(style.size)
	end
	if style.font == "mono" then
		s = s .. core.get_font_escape_sequence("mono")
	end
	if style.weight ~= "normal" and style.weight ~= "" then
		s = s .. core.get_font_weight_escape_sequence(style.weight)
	end
	if style.align ~= "left" and style.align ~= "" then
		s = s .. core.get_alignment_escape_sequence(style.align)
	end
	if style.bgcolor ~= "" then
		s = s .. core.get_background_escape_sequence(style.bgcolor)
	end
	if style.color ~= "" then
		s = s .. core.get_color_escape_sequence(style.color)
	end
	return s
end

local function tf_on_rightclick(pos, node, clicker)
	local meta = core.get_meta(pos)
	local name = clicker:get_player_name()
	local raw_text = meta:get_string("text")
	local display_text = strip_escapes(raw_text)
	local style = parse_style(raw_text)

	core.show_formspec(name, FORMNAME,
		"formspec_version[4]" ..
		"size[10,9]" ..

		-- Text area
		"textarea[0.5,0.3;9,3.5;text;Text;" ..
			core.formspec_escape(display_text) .. "]" ..

		-- Font size (empty = auto)
		"label[0.5,4.3;Size (default):]" ..
		"field[2.5,4;2,0.8;font_size;;" .. style.size .. "]" ..

		-- Font face
		"label[5,4.3;Font:]" ..
		"dropdown[6,4;3.5,0.8;font;normal,mono;" ..
			(style.font == "mono" and "2" or "1") .. "]" ..

		-- Weight
		"label[0.5,5.3;Weight:]" ..
		"dropdown[2.5,5;3,0.8;weight;normal,bold,italic,bolditalic;" ..
			(({normal=1, bold=2, italic=3, bolditalic=4})[style.weight] or 1) ..
			"]" ..

		-- Alignment
		"label[5.5,5.3;Align:]" ..
		"dropdown[6.5,5;3,0.8;align;left,center,right;" ..
			(({left=1, center=2, right=3})[style.align] or 1) ..
			"]" ..

		-- Text color
		"label[0.5,6.3;Color:]" ..
		"field[2.5,6;3,0.8;color;;" ..
			core.formspec_escape(style.color) .. "]" ..

		-- Background color
		"label[5.5,6.3;BG color:]" ..
		"field[7,6;2.5,0.8;bgcolor;;" ..
			core.formspec_escape(style.bgcolor) .. "]" ..

		-- Submit
		"button_exit[3.5,7.5;3,0.8;submit;Set Text]")

	clicker:get_meta():set_string("editing_sign", core.pos_to_string(pos))
end

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname ~= FORMNAME or not fields.submit then
		return
	end
	local pos_str = player:get_meta():get_string("editing_sign")
	if pos_str == "" then return end
	local pos = core.string_to_pos(pos_str)
	if not pos then return end

	local style = {
		size = fields.font_size or "16",
		font = fields.font or "normal",
		weight = fields.weight or "normal",
		align = fields.align or "left",
		color = fields.color or "#ffffff",
		bgcolor = fields.bgcolor or "",
	}

	local prefix = build_style(style)
	core.get_meta(pos):set_string("text", prefix .. (fields.text or ""))
	player:get_meta():set_string("editing_sign", "")
end)

-- 1. Normal (top face)
core.register_node("testtextface:normal", {
	description = "TextFace: Normal",
	tiles = {"default_stone.png"},
	drawtype = "normal",
	paramtype2 = "facedir",
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.05}, {x = 0.95, y = 0.5}}, resolution_x = 512, resolution_y = 512 },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_background_escape_sequence("#00000080") ..
			core.get_color_escape_sequence("#ffcc00") .. "WELCOME" ..
			core.get_color_escape_sequence("#ffffff") .. "\nTo the test area\n" ..
			core.get_color_escape_sequence("#88ff88") .. "Have fun!")
	end,
})

-- 2. Normal with text on side face (tile 2 = +X)
core.register_node("testtextface:normal_side", {
	description = "TextFace: Normal Side",
	tiles = {"default_stone.png", "default_stone.png", "default_cobble.png",
		"default_stone.png", "default_stone.png", "default_stone.png"},
	drawtype = "normal",
	paramtype2 = "facedir",
	text_face = { tile = 2, rect = {{x = 0.05, y = 0.1}, {x = 0.95, y = 0.6}}, resolution_x = 512, resolution_y = 512 },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_background_escape_sequence("#1a1a2e") ..
			core.get_color_escape_sequence("#e94560") .. "DANGER" ..
			core.get_color_escape_sequence("#ffffff") .. "\nKeep out!")
	end,
})

-- 3. Nodebox (slab)
core.register_node("testtextface:nodebox", {
	description = "TextFace: Nodebox Slab",
	tiles = {"default_stone.png"},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "facedir",
	node_box = {
		type = "fixed",
		fixed = {-0.5, -0.5, -0.5, 0.5, 0, 0.5},
	},
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.05}, {x = 0.95, y = 0.95}}, resolution_x = 128, resolution_y = 128 },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_font_size_escape_sequence(12) ..
			core.get_color_escape_sequence("#333333") .. "Floor plaque\n" ..
			core.get_color_escape_sequence("#886644") .. "Est. 2026")
	end,
})

-- 4. Signlike
core.register_node("testtextface:signlike", {
	description = "TextFace: Signlike",
	tiles = {"default_stone.png"},
	drawtype = "signlike",
	paramtype = "light",
	paramtype2 = "wallmounted",
	walkable = false,
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.1}, {x = 0.95, y = 0.9}}, resolution_x = 256, resolution_y = 256 },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_font_size_escape_sequence(20) ..
			core.get_background_escape_sequence("#ffffffd0") ..
			core.get_color_escape_sequence("#000000") .. "Wall sign\n" ..
			core.get_color_escape_sequence("#0066cc") .. "Room 42")
	end,
})


-- 8. Mesh node (cube)
core.register_node("testtextface:mesh", {
	description = "TextFace: Mesh Cube",
	tiles = {"default_stone.png"},
	drawtype = "mesh",
	mesh = "textface_cube.obj",
	paramtype = "light",
	paramtype2 = "facedir",
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.1}, {x = 0.95, y = 0.6}} },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_background_escape_sequence("#002244e0") ..
			core.get_color_escape_sequence("#00ffff") .. "MESH\n" ..
			core.get_color_escape_sequence("#ffffff") .. "Cube node\n" ..
			core.get_color_escape_sequence("#888888") .. "tile 0")
	end,
})

-- 9. Mesh node with slanted display (2 groups: display + body)
core.register_node("testtextface:mesh_slanted", {
	description = "TextFace: Slanted Mesh",
	tiles = {"default_cobble.png", "default_stone.png"},
	drawtype = "mesh",
	mesh = "textface_slanted.obj",
	paramtype = "light",
	paramtype2 = "facedir",
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.1}, {x = 0.95, y = 0.9}} },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_background_escape_sequence("#2d132c") ..
			core.get_color_escape_sequence("#ee4540") .. "NOTICE\n" ..
			core.get_color_escape_sequence("#c72c41") .. "Slanted display\n" ..
			core.get_color_escape_sequence("#801336") .. "with angled face")
	end,
})

-- 10. Auto-wrap test (long text, no newlines)
core.register_node("testtextface:wrap", {
	description = "TextFace: Wrap Test",
	tiles = {"default_stone.png"},
	drawtype = "normal",
	paramtype2 = "facedir",
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.05}, {x = 0.95, y = 0.95}} },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_background_escape_sequence("#f5f5dcc0") ..
			core.get_color_escape_sequence("#333333") ..
			"Lorem ipsum dolor sit amet, consectetur adipiscing elit. " ..
			"Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.")
	end,
})

-- 11. Color test — rainbow + mixed styles
core.register_node("testtextface:colors", {
	description = "TextFace: Color Test",
	tiles = {"default_stone.png"},
	drawtype = "normal",
	paramtype2 = "facedir",
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.05}, {x = 0.95, y = 0.7}} },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_background_escape_sequence("#ffffffc0") ..
			core.get_color_escape_sequence("#ff0000") .. "Red " ..
			core.get_color_escape_sequence("#ff8800") .. "Orange " ..
			core.get_color_escape_sequence("#ffff00") .. "Yellow\n" ..
			core.get_color_escape_sequence("#00ff00") .. "Green " ..
			core.get_color_escape_sequence("#0088ff") .. "Blue " ..
			core.get_color_escape_sequence("#8800ff") .. "Purple\n" ..
			core.get_color_escape_sequence("#000000") .. "Black on white bg")
	end,
})

-- 12. Mono font
core.register_node("testtextface:mono", {
	description = "TextFace: Mono Font",
	tiles = {"default_stone.png"},
	drawtype = "normal",
	paramtype2 = "facedir",
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.05}, {x = 0.95, y = 0.95}}, resolution_x = 512, resolution_y = 512 },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_font_size_escape_sequence(14) .. core.get_font_escape_sequence("mono") ..
			core.get_background_escape_sequence("#000000e0") ..
			core.get_color_escape_sequence("#00ff00") .. "$ ls -la\n" ..
			core.get_color_escape_sequence("#aaaaaa") .. "drwxr-xr-x 2 user\n" ..
			"-rw-r--r-- 1 user\n" ..
			core.get_color_escape_sequence("#00ff00") .. "$ _")
	end,
})

-- 13. Bold font
core.register_node("testtextface:bold", {
	description = "TextFace: Bold Font",
	tiles = {"default_stone.png"},
	drawtype = "normal",
	paramtype2 = "facedir",
	text_face = { tile = 0, rect = {{x = 0.05, y = 0.05}, {x = 0.95, y = 0.5}}, resolution_x = 512, resolution_y = 512 },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_font_size_escape_sequence(28) .. core.get_font_weight_escape_sequence("bold") ..
			core.get_background_escape_sequence("#cc0000") ..
			core.get_color_escape_sequence("#ffffff") .. "BOLD TEXT\n" ..
			core.get_color_escape_sequence("#ffcccc") .. "Important!")
	end,
})

-- Full-face signlike (1x1, default settings for calibration)
core.register_node("testtextface:signlike_full", {
	description = "TextFace: Signlike Full Face",
	tiles = {"default_cobble.png"},
	drawtype = "signlike",
	paramtype = "light",
	paramtype2 = "wallmounted",
	walkable = false,
	text_face = { tile = 0, rect = {{x = 0, y = 0}, {x = 1, y = 1}} },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_color_escape_sequence("#000000") ..
			"The quick brown fox\njumps over the\nlazy dog.\nLine 4\nLine 5")
	end,
})

-- Wide sign (3 nodes wide, mesh node)
core.register_node("testtextface:wide_sign", {
	description = "TextFace: Wide Sign (3 nodes)",
	tiles = {"default_cobble.png", "default_stone.png"},
	drawtype = "mesh",
	mesh = "textface_wide.obj",
	paramtype = "light",
	paramtype2 = "facedir",
	selection_box = {
		type = "fixed",
		fixed = {-1.5, -0.5, -0.05, 1.5, 0.5, 0.05},
	},
	collision_box = {
		type = "fixed",
		fixed = {-1.5, -0.5, -0.05, 1.5, 0.5, 0.05},
	},
	text_face = { tile = 0, rect = {{x = 0.02, y = 0.05}, {x = 0.98, y = 0.95}}, resolution_x = 1024, resolution_y = 341 },
	groups = {dig_immediate = 2},
	on_rightclick = tf_on_rightclick,
	on_construct = function(pos)
		core.get_meta(pos):set_string("text",
			core.get_font_size_escape_sequence(48) .. core.get_alignment_escape_sequence("center") ..
			core.get_background_escape_sequence("#1a1a2ee0") ..
			core.get_color_escape_sequence("#ffffff") .. "WIDE SIGN\n" ..
			core.get_color_escape_sequence("#aaaaaa") .. "3 nodes wide, high resolution")
	end,
})

-- Helper: build a [text: texture string for use on entities
function testtextface.make_text_texture(base_tex, text, rect, res_x, res_y)
	rect = rect or {0, 0, 1, 1}
	res_x = res_x or 256
	res_y = res_y or 256
	return base_tex ..
		"^[text:" .. core.encode_base64(text) ..
		":" .. table.concat(rect, ",") ..
		":" .. res_x .. "," .. res_y
end

-- Test entity with text face
core.register_entity("testtextface:sign_entity", {
	initial_properties = {
		visual = "cube",
		visual_size = {x = 1, y = 1, z = 0.1},
		physical = false,
		collisionbox = {-0.5, -0.5, -0.05, 0.5, 0.5, 0.05},
		textures = {"default_stone.png", "default_stone.png",
			"default_stone.png", "default_stone.png",
			"default_stone.png", "default_stone.png"},
	},
	_text = "",
	on_activate = function(self, staticdata)
		if staticdata and staticdata ~= "" then
			self._text = staticdata
		else
			self._text = core.get_font_size_escape_sequence(24) .. core.get_alignment_escape_sequence("center") ..
				core.get_background_escape_sequence("#000000c0") ..
				core.get_color_escape_sequence("#00ffcc") .. "ENTITY\n" ..
				core.get_color_escape_sequence("#ffffff") .. "Right-click to edit"
		end
		self:_update_texture()
	end,
	get_staticdata = function(self)
		return self._text
	end,
	on_rightclick = function(self, clicker)
		local name = clicker:get_player_name()
		local display = strip_escapes(self._text)
		local style = parse_style(self._text)
		core.show_formspec(name, "testtextface:entity_edit",
			"formspec_version[4]" ..
			"size[8,5]" ..
			"textarea[0.5,0.3;7,2.5;text;Entity text;" ..
				core.formspec_escape(display) .. "]" ..
			"field[0.5,3.3;3,0.8;font_size;Size (default);" .. style.size .. "]" ..
			"dropdown[4,3.3;3.5,0.8;align;left,center,right;" ..
				(({left=1, center=2, right=3})[style.align] or 1) .. "]" ..
			"button_exit[2.5,4.2;3,0.8;submit;Set Text]")
		-- Store entity ref via object ID
		clicker:get_meta():set_string("editing_entity",
			tostring(self.object:get_pos()))
		clicker:get_meta():set_string("editing_entity_style",
			get_style_prefix(self._text))
	end,
	_update_texture = function(self)
		local tex = testtextface.make_text_texture(
			"default_cobble.png", self._text,
			{0.02, 0.02, 0.98, 0.98}, 256, 256, 0)
		local props = self.object:get_properties()
		-- Front face gets text, rest keeps stone
		props.textures = {
			"default_stone.png", "default_stone.png",
			"default_stone.png", "default_stone.png",
			tex, "default_stone.png",
		}
		self.object:set_properties(props)
	end,
})

-- Spawner item
core.register_craftitem("testtextface:sign_entity_spawner", {
	description = "TextFace: Spawn Sign Entity",
	inventory_image = "default_cobble.png",
	on_place = function(itemstack, placer, pointed_thing)
		if pointed_thing.type == "node" then
			local pos = pointed_thing.above
			core.add_entity(pos, "testtextface:sign_entity")
		end
		return itemstack
	end,
})

-- Handle entity text editing
core.register_on_player_receive_fields(function(player, formname, fields)
	if formname ~= "testtextface:entity_edit" or not fields.submit then
		return
	end
	local pos_str = player:get_meta():get_string("editing_entity")
	if pos_str == "" then return end
	local pos = core.string_to_pos(pos_str)
	if not pos then return end

	-- Find the entity near this position
	local objects = core.get_objects_inside_radius(pos, 0.5)
	for _, obj in ipairs(objects) do
		local ent = obj:get_luaentity()
		if ent and ent.name == "testtextface:sign_entity" then
			local style = {
				size = fields.font_size or "",
				font = "normal",
				weight = "normal",
				align = fields.align or "left",
				color = "#ffffff",
				bgcolor = "",
			}
			ent._text = build_style(style) .. (fields.text or "")
			ent:_update_texture()
			break
		end
	end
	player:get_meta():set_string("editing_entity", "")
	player:get_meta():set_string("editing_entity_style", "")
end)
