local S = core.get_translator("testtools")
local F = core.formspec_escape

testtools = {}

dofile(core.get_modpath("testtools") .. "/light.lua")
dofile(core.get_modpath("testtools") .. "/privatizer.lua")
dofile(core.get_modpath("testtools") .. "/particles.lua")
dofile(core.get_modpath("testtools") .. "/node_box_visualizer.lua")

local pointabilities_nodes = {
	nodes = {
		["group:blocking_pointable_test"] = true,
		["group:not_pointable_test"] = true,
	},
}

local pointabilities_objects = {
	objects = {
		["group:blocking_pointable_test"] = true,
		["group:not_pointable_test"] = true,
	},
}

core.register_tool("testtools:param2tool", {
	description = S("Param2 Tool") .."\n"..
		S("Modify param2 value of nodes") .."\n"..
		S("Punch: +1") .."\n"..
		S("Sneak+Punch: +8") .."\n"..
		S("Place: -1") .."\n"..
		S("Sneak+Place: -8"),
	inventory_image = "testtools_param2tool.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_nodes,
	on_use = function(itemstack, user, pointed_thing)
		local pos = core.get_pointed_thing_position(pointed_thing)
		if pointed_thing.type ~= "node" or (not pos) then
			return
		end
		local add = 1
		if user then
			local ctrl = user:get_player_control()
			if ctrl.sneak then
				add = 8
			end
		end
		local node = core.get_node(pos)
		node.param2 = node.param2 + add
		core.swap_node(pos, node)
	end,
	on_place = function(itemstack, user, pointed_thing)
		local pos = core.get_pointed_thing_position(pointed_thing)
		if pointed_thing.type ~= "node" or (not pos) then
			return
		end
		local add = -1
		if user then
			local ctrl = user:get_player_control()
			if ctrl.sneak then
				add = -8
			end
		end
		local node = core.get_node(pos)
		node.param2 = node.param2 + add
		core.swap_node(pos, node)
	end,
})

core.register_tool("testtools:node_setter", {
	description = S("Node Setter") .."\n"..
		S("Replace pointed node with something else") .."\n"..
		S("Punch: Select pointed node") .."\n"..
		S("Place on node: Replace node with selected node") .."\n"..
		S("Place in air: Manually select a node"),
	inventory_image = "testtools_node_setter.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_nodes,
	on_use = function(itemstack, user, pointed_thing)
		local pos = core.get_pointed_thing_position(pointed_thing)
		if pointed_thing.type == "nothing" then
			local meta = itemstack:get_meta()
			meta:set_string("node", "air")
			meta:set_int("node_param2", 0)
			if user and user:is_player() then
				core.chat_send_player(user:get_player_name(), S("Now placing: @1 (param2=@2)", "air", 0))
			end
			return itemstack
		elseif pointed_thing.type ~= "node" or (not pos) then
			return
		end
		local node = core.get_node(pos)
		local meta = itemstack:get_meta()
		meta:set_string("node", node.name)
		meta:set_int("node_param2", node.param2)
		if user and user:is_player() then
			core.chat_send_player(user:get_player_name(), S("Now placing: @1 (param2=@2)", node.name, node.param2))
		end
		return itemstack
	end,
	on_secondary_use = function(itemstack, user, pointed_thing)
		local meta = itemstack:get_meta()
		local nodename = meta:get_string("node") or ""
		local param2 = meta:get_int("node_param2") or 0

		core.show_formspec(user:get_player_name(), "testtools:node_setter",
			"size[4,4]"..
			"field[0.5,1;3,1;nodename;"..F(S("Node name (itemstring):"))..";"..F(nodename).."]"..
			"field[0.5,2;3,1;param2;"..F(S("param2:"))..";"..F(tostring(param2)).."]"..
			"button_exit[0.5,3;3,1;submit;"..F(S("Submit")).."]"
		)
	end,
	on_place = function(itemstack, user, pointed_thing)
		local pos = core.get_pointed_thing_position(pointed_thing)
		local meta = itemstack:get_meta()
		local nodename = meta:get_string("node")
		if nodename == "" and user and user:is_player() then
			core.chat_send_player(user:get_player_name(), S("Punch a node first!"))
			return
		end
		local param2 = meta:get_int("node_param2")
		if not param2 then
			param2 = 0
		end
		local node = { name = nodename, param2 = param2 }
		if not core.registered_nodes[nodename] then
			core.chat_send_player(user:get_player_name(), S("Cannot set unknown node: @1", nodename))
			return
		end
		core.set_node(pos, node)
	end,
})

core.register_tool("testtools:remover", {
	description = S("Remover") .."\n"..
		S("Punch: Remove pointed node or object"),
	inventory_image = "testtools_remover.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = {
		nodes = pointabilities_nodes.nodes,
		objects = pointabilities_objects.objects,
	},
	on_use = function(itemstack, user, pointed_thing)
		local pos = core.get_pointed_thing_position(pointed_thing)
		if pointed_thing.type == "node" and pos ~= nil then
			core.remove_node(pos)
		elseif pointed_thing.type == "object" then
			local obj = pointed_thing.ref
			if not obj:is_player() then
				obj:remove()
			else
				core.chat_send_player(user:get_player_name(), S("Can't remove players!"))
			end
		end
	end,
})

core.register_tool("testtools:falling_node_tool", {
	description = S("Falling Node Tool") .."\n"..
		S("Punch: Make pointed node fall") .."\n"..
		S("Place: Move pointed node 2 units upwards, then make it fall"),
	inventory_image = "testtools_falling_node_tool.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_nodes,
	on_place = function(itemstack, user, pointed_thing)
		-- Teleport node 1-2 units upwards (if possible) and make it fall
		local pos = core.get_pointed_thing_position(pointed_thing)
		if pointed_thing.type ~= "node" or (not pos) then
			return
		end
		local ok = false
		local highest
		for i=1,2 do
			local above = {x=pos.x,y=pos.y+i,z=pos.z}
			local n2 = core.get_node(above)
			local def2 = core.registered_nodes[n2.name]
			if def2 and (not def2.walkable) then
				highest = above
			else
				break
			end
		end
		if highest then
			local node = core.get_node(pos)
			local metatable = core.get_meta(pos):to_table()
			core.remove_node(pos)
			core.set_node(highest, node)
			local meta_highest = core.get_meta(highest)
			meta_highest:from_table(metatable)
			ok = core.spawn_falling_node(highest)
		else
			ok = core.spawn_falling_node(pos)
		end
		if not ok and user and user:is_player() then
			core.chat_send_player(user:get_player_name(), S("Falling node could not be spawned!"))
		end
	end,
	on_use = function(itemstack, user, pointed_thing)
		local pos = core.get_pointed_thing_position(pointed_thing)
		if pointed_thing.type ~= "node" or (not pos) then
			return
		end
		local ok = core.spawn_falling_node(pos)
		if not ok and user and user:is_player() then
			core.chat_send_player(user:get_player_name(), S("Falling node could not be spawned!"))
		end
	end,
})

core.register_tool("testtools:rotator", {
	description = S("Entity Rotator") .. "\n" ..
		S("Rotate pointed entity") .."\n"..
		S("Punch: Yaw") .."\n"..
		S("Sneak+Punch: Pitch") .."\n"..
		S("Aux1+Punch: Roll"),
	inventory_image = "testtools_entity_rotator.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_objects,
	on_use = function(itemstack, user, pointed_thing)
		if pointed_thing.type ~= "object" then
			return
		end
		local obj = pointed_thing.ref
		if obj:is_player() then
			-- No player rotation
			return
		else
			local axis = "y"
			if user and user:is_player() then
				local ctrl = user:get_player_control()
				if ctrl.sneak then
					axis = "x"
				elseif ctrl.aux1 then
					axis = "z"
				end
			end
			local rot = obj:get_rotation()
			rot[axis] = rot[axis] + math.pi/8
			if rot[axis] > math.pi*2 then
				rot[axis] = rot[axis] - math.pi*2
			end
			obj:set_rotation(rot)
		end
	end,
})

local mover_config = function(itemstack, user, pointed_thing)
	if not (user and user:is_player()) then
		return
	end
	local name = user:get_player_name()
	local ctrl = user:get_player_control()
	local meta = itemstack:get_meta()
	local dist = 1.0
	if meta:contains("distance") then
		dist = meta:get_int("distance")
	end
	if ctrl.sneak then
		dist = dist - 1
	else
		dist = dist + 1
	end
	meta:set_int("distance", dist)
	core.chat_send_player(user:get_player_name(), S("distance=@1/10", dist*2))
	return itemstack
end

core.register_tool("testtools:object_mover", {
	description = S("Object Mover") .."\n"..
		S("Move pointed object towards or away from you") .."\n"..
		S("Punch: Move by distance").."\n"..
		S("Sneak+Punch: Move by negative distance").."\n"..
		S("Place: Increase distance").."\n"..
		S("Sneak+Place: Decrease distance"),
	inventory_image = "testtools_object_mover.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_objects,
	on_place = mover_config,
	on_secondary_use = mover_config,
	on_use = function(itemstack, user, pointed_thing)
		if pointed_thing.type ~= "object" then
			return
		end
		local obj = pointed_thing.ref
		if not (user and user:is_player()) then
			return
		end
		local yaw = user:get_look_horizontal()
		local dir = core.yaw_to_dir(yaw)
		local pos = obj:get_pos()
		local pitch = user:get_look_vertical()
		if pitch > 0.25 * math.pi then
			dir.y = -1
			dir.x = 0
			dir.z = 0
		elseif pitch < -0.25 * math.pi then
			dir.y = 1
			dir.x = 0
			dir.z = 0
		end
		local ctrl = user:get_player_control()
		if ctrl.sneak then
			dir = vector.multiply(dir, -1)
		end
		local meta = itemstack:get_meta()
		if meta:contains("distance") then
			local dist = meta:get_int("distance")
			dir = vector.multiply(dir, dist*0.2)
		end
		pos = vector.add(pos, dir)
		obj:set_pos(pos)
	end,
})



core.register_tool("testtools:entity_scaler", {
	description = S("Entity Visual Scaler") .."\n"..
		S("Scale visual size of entities") .."\n"..
		S("Punch: Increase size") .."\n"..
		S("Sneak+Punch: Decrease scale"),
	inventory_image = "testtools_entity_scaler.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_objects,
	on_use = function(itemstack, user, pointed_thing)
		if pointed_thing.type ~= "object" then
			return
		end
		local obj = pointed_thing.ref
		if obj:is_player() then
			-- No player scaling
			return
		else
			local diff = 0.1
			if user and user:is_player() then
				local ctrl = user:get_player_control()
				if ctrl.sneak then
					diff = -0.1
				end
			end
			local prop = obj:get_properties()
			if not prop.visual_size then
				prop.visual_size = { x=1, y=1, z=1 }
			else
				prop.visual_size = { x=prop.visual_size.x+diff, y=prop.visual_size.y+diff, z=prop.visual_size.z+diff }
				if prop.visual_size.x <= 0.1 then
					prop.visual_size.x = 0.1
				end
				if prop.visual_size.y <= 0.1 then
					prop.visual_size.y = 0.1
				end
				if prop.visual_size.z <= 0.1 then
					prop.visual_size.z = 0.1
				end
			end
			obj:set_properties(prop)
		end
	end,
})


-- value-weak tables, because we don't want to keep the objrefs of unloaded objects
local branded_objects = setmetatable({}, {__mode = "v"})
local next_brand_num = 1

function testtools.get_branded_object(name)
	if name:sub(1, 7) == "player:" then
		return core.get_player_by_name(name:sub(8))
	elseif name:sub(1, 4) == "obj:" then
		return branded_objects[tonumber(name:sub(5)) or 0]
	end
	return nil
end

core.register_tool("testtools:branding_iron", {
	description = S("Branding Iron") .."\n"..
		S("Give an object a temporary name.") .."\n"..
		S("Punch object: Brand the object") .."\n"..
		S("Punch air: Brand yourself") .."\n"..
		S("The name is valid until the object unloads.") .."\n"..
		S("Devices that accept the returned name also accept \"player:<playername>\" for players."),
	inventory_image = "testtools_branding_iron.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_objects,
	on_use = function(_itemstack, user, pointed_thing)
		local obj
		local msg
		if pointed_thing.type == "object" then
			obj = pointed_thing.ref
			msg = "You can now refer to this object with: \"@1\""
		elseif pointed_thing.type == "nothing" then
			obj = user
			msg = "You can now refer to yourself with: \"@1\""
		else
			return
		end

		local brand_num = next_brand_num
		next_brand_num = next_brand_num + 1
		branded_objects[brand_num] = obj

		core.chat_send_player(user:get_player_name(), S(msg, "obj:"..brand_num))
	end,
})


local selections = {}
local entity_list
local function get_entity_list()
	if entity_list then
		return entity_list
	end
	local ents = core.registered_entities
	local list = {}
	for k,_ in pairs(ents) do
		table.insert(list, k)
	end
	table.sort(list)
	entity_list = list
	return entity_list
end
core.register_tool("testtools:entity_spawner", {
	description = S("Entity Spawner") .."\n"..
		S("Spawns entities") .."\n"..
		S("Punch: Select entity to spawn") .."\n"..
		S("Place: Spawn selected entity"),
	inventory_image = "testtools_entity_spawner.png",
	groups = { testtool = 1, disable_repair = 1 },
	on_place = function(itemstack, user, pointed_thing)
		local name = user:get_player_name()
		if pointed_thing.type == "node" then
			if selections[name] then
				local pos = pointed_thing.above
				core.add_entity(pos, get_entity_list()[selections[name]])
			else
				core.chat_send_player(name, S("Select an entity first (with punch key)!"))
			end
		end
	end,
	on_use = function(itemstack, user, pointed_thing)
		if pointed_thing.type == "object" then
			return
		end
		if user and user:is_player() then
			local list = table.concat(get_entity_list(), ",")
			local name = user:get_player_name()
			local sel = selections[name] or ""
			core.show_formspec(name, "testtools:entity_list",
				"size[9,9]"..
				"textlist[0,0;9,8;entity_list;"..list..";"..sel..";false]"..
				"button[0,8;4,1;spawn;Spawn entity]"
			)
		end
	end,
})

local function prop_to_string(property)
	if type(property) == "string" then
		return "\"" .. property .. "\""
	elseif type(property) == "table" then
		return tostring(dump(property)):gsub("\n", "")
	else
		return tostring(property)
	end
end

local property_formspec_data = {}
local property_formspec_index = {}
local selected_objects = {}
local function get_object_properties_form(obj, playername)
	if not playername then return "" end
	local props = obj:get_properties()
	local str = ""
	property_formspec_data[playername] = {}
	local proplist = {}
	for k,_ in pairs(props) do
		table.insert(proplist, k)
	end
	table.sort(proplist)
	for p=1, #proplist do
		local k = proplist[p]
		local v = props[k]
		local newline = ""
		newline = k .. " = "
		newline = newline .. prop_to_string(v)
		str = str .. F(newline)
		if p < #proplist then
			str = str .. ","
		end
		table.insert(property_formspec_data[playername], k)
	end
	return str
end

local editor_formspec_selindex = {}

local editor_formspec = function(playername, obj, value, sel)
	if not value then
		value = ""
	end
	if not sel then
		sel = ""
	end
	local list = get_object_properties_form(obj, playername)
	local title
	if obj:is_player() then
		title = S("Object properties of player “@1”", obj:get_player_name())
	else
		local ent = obj:get_luaentity()
		title = S("Object properties of @1", ent.name)
	end
	core.show_formspec(playername, "testtools:object_editor",
		"size[9,9]"..
		"label[0,0;"..F(title).."]"..
		"textlist[0,0.5;9,7.5;object_props;"..list..";"..sel..";false]"..
		"field[0.2,8.75;8,1;value;"..F(S("Value"))..";"..F(value).."]"..
		"field_close_on_enter[value;false]"..
		"button[8,8.5;1,1;submit;"..F(S("Submit")).."]"
	)
end

core.register_tool("testtools:object_editor", {
	description = S("Object Property Editor") .."\n"..
		S("Edit properties of objects") .."\n"..
		S("Punch object: Edit object") .."\n"..
		S("Punch air: Edit yourself"),
	inventory_image = "testtools_object_editor.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_objects,
	on_use = function(itemstack, user, pointed_thing)
		if user and user:is_player() then
			local name = user:get_player_name()

			if pointed_thing.type == "object" then
				selected_objects[name] = pointed_thing.ref
			elseif pointed_thing.type == "nothing" then
				-- Use on yourself if pointing nothing
				selected_objects[name] = user
			else
				-- Unsupported pointed thing
				return
			end

			local sel = editor_formspec_selindex[name]
			local val
			if selected_objects[name] and selected_objects[name]:get_properties() then
				local props = selected_objects[name]:get_properties()
				local keys = property_formspec_data[name]
				if property_formspec_index[name] and props then
					local key = keys[property_formspec_index[name]]
					val = prop_to_string(props[key])
				end
			end

			editor_formspec(name, selected_objects[name], val, sel)
		end
	end,
})

local ent_parent = {}
local ent_child = {}
local DEFAULT_ATTACH_OFFSET_Y = 11

local attacher_config = function(itemstack, user, pointed_thing)
	if not (user and user:is_player()) then
		return
	end
	if pointed_thing.type == "object" then
		return
	end
	local name = user:get_player_name()
	local ctrl = user:get_player_control()
	local meta = itemstack:get_meta()
	if ctrl.aux1 then
		local rot_x = meta:get_float("rot_x")
		if ctrl.sneak then
			rot_x = rot_x - math.pi/8
		else
			rot_x = rot_x + math.pi/8
		end
		if rot_x > 6.2 then
			rot_x = 0
		elseif rot_x < 0 then
			rot_x = math.pi * (15/8)
		end
		core.chat_send_player(name, S("rotation=@1", core.pos_to_string({x=rot_x,y=0,z=0})))
		meta:set_float("rot_x", rot_x)
	else
		local pos_y
		if meta:contains("pos_y") then
			pos_y = meta:get_int("pos_y")
		else
			pos_y = DEFAULT_ATTACH_OFFSET_Y
		end
		if ctrl.sneak then
			pos_y = pos_y - 1
		else
			pos_y = pos_y + 1
		end
		core.chat_send_player(name, S("position=@1", core.pos_to_string({x=0,y=pos_y,z=0})))
		meta:set_int("pos_y", pos_y)
	end
	return itemstack
end

core.register_tool("testtools:object_attacher", {
	description = S("Object Attacher") .."\n"..
		S("Attach object to another") .."\n"..
		S("Punch objects to first select parent object, then the child object to attach") .."\n"..
		S("Punch air to select yourself") .."\n"..
		S("Place: Incease attachment Y offset") .."\n"..
		S("Sneak+Place: Decease attachment Y offset") .."\n"..
		S("Aux1+Place: Incease attachment rotation") .."\n"..
		S("Aux1+Sneak+Place: Decrease attachment rotation"),
	inventory_image = "testtools_object_attacher.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_objects,
	on_place = attacher_config,
	on_secondary_use = attacher_config,
	on_use = function(itemstack, user, pointed_thing)
		if user and user:is_player() then
			local name = user:get_player_name()
			local selected_object
			if pointed_thing.type == "object" then
				selected_object = pointed_thing.ref
			elseif pointed_thing.type == "nothing" then
				selected_object = user
			else
				return
			end
			local ctrl = user:get_player_control()
			if ctrl.sneak then
				if selected_object:get_attach() then
					selected_object:set_detach()
					core.chat_send_player(name, S("Object detached!"))
				else
					core.chat_send_player(name, S("Object is not attached!"))
				end
				return
			end
			local parent = ent_parent[name]
			local child = ent_child[name]
			local ename = S("<unknown>")
			if not parent then
				parent = selected_object
				ent_parent[name] = parent
			elseif not child then
				child = selected_object
				ent_child[name] = child
			end
			local entity = selected_object:get_luaentity()
			if entity then
				ename = entity.name
			elseif selected_object:is_player() then
				ename = selected_object:get_player_name()
			end
			if selected_object == parent then
				core.chat_send_player(name, S("Parent object selected: @1", ename))
			elseif selected_object == child then
				core.chat_send_player(name, S("Child object selected: @1", ename))
			end
			if parent and child then
				if parent == child then
					core.chat_send_player(name, S("Can't attach an object to itself!"))
					ent_parent[name] = nil
					ent_child[name] = nil
					return
				end
				local meta = itemstack:get_meta()
				local y
				if meta:contains("pos_y") then
					y = meta:get_int("pos_y")
				else
					y = DEFAULT_ATTACH_OFFSET_Y
				end
				local rx = meta:get_float("rot_x") or 0
				local offset = {x=0,y=y,z=0}
				local angle = {x=rx,y=0,z=0}
				child:set_attach(parent, "", offset, angle)
				local check_parent = child:get_attach()
				if check_parent then
					core.chat_send_player(name, S("Object attached! position=@1, rotation=@2",
						core.pos_to_string(offset), core.pos_to_string(angle)))
				else
					core.chat_send_player(name, S("Attachment failed!"))
				end
				ent_parent[name] = nil
				ent_child[name] = nil
			end
		end
	end,
})

local function print_object(obj)
	if obj:is_player() then
		return "player '"..obj:get_player_name().."'"
	elseif obj:get_luaentity() then
		return "LuaEntity '"..obj:get_luaentity().name.."'"
	else
		return "object"
	end
end

core.register_tool("testtools:children_getter", {
	description = S("Children Getter") .."\n"..
		S("Shows list of objects attached to object") .."\n"..
		S("Punch object to show its 'children'") .."\n"..
		S("Punch air to show your own 'children'"),
	inventory_image = "testtools_children_getter.png",
	groups = { testtool = 1, disable_repair = 1 },
	pointabilities = pointabilities_objects,
	on_use = function(itemstack, user, pointed_thing)
		if user and user:is_player() then
			local name = user:get_player_name()
			local selected_object
			local self_name
			if pointed_thing.type == "object" then
				selected_object = pointed_thing.ref
			elseif pointed_thing.type == "nothing" then
				selected_object = user
			else
				return
			end
			self_name = print_object(selected_object)
			local children = selected_object:get_children()
			local ret = ""
			for c=1, #children do
				ret = ret .. "* " .. print_object(children[c])
				if c < #children then
					ret = ret .. "\n"
				end
			end
			if ret == "" then
				ret = S("No children attached to @1.", self_name)
			else
				ret = S("Children of @1:", self_name) .. "\n" .. ret
			end
			core.chat_send_player(user:get_player_name(), ret)
		end
	end,
})

-- Use loadstring to parse param as a Lua value
local function use_loadstring(param, player)
	-- For security reasons, require 'server' priv, just in case
	-- someone is actually crazy enough to run this on a public server.
	local privs = core.get_player_privs(player:get_player_name())
	if not privs.server then
		return false, "You need 'server' privilege to change object properties!"
	end
	if not param then
		return false, "Failed: parameter is nil"
	end
	--[[ DANGER ZONE ]]
	-- Interpret string as Lua value
	local func, errormsg = loadstring("return (" .. param .. ")")
	if not func then
		return false, "Failed: " .. errormsg
	end

	-- Apply sandbox here using setfenv
	setfenv(func, {})

	-- Run it
	local good, errOrResult = pcall(func)
	if not good then
		-- A Lua error was thrown
		return false, "Failed: " .. errOrResult
	end

	-- errOrResult will be the value
	return true, errOrResult
end

-- Item Meta Editor + Node Meta Editor
local node_meta_posses = {}
local meta_latest_keylist = {}

local function show_meta_formspec(user, metatype, pos_or_item, key, value, keylist)
	local textlist
	if keylist then
		textlist = "textlist[0,0.5;2.5,6.5;keylist;"..keylist.."]"
	else
		textlist = ""
	end

	local form = "size[15,9]"..
		"label[0,0;"..F(S("Current keys:")).."]"..
		textlist..
		"field[3,0.5;12,1;key;"..F(S("Key"))..";"..F(key).."]"..
		"textarea[3,1.5;12,6;value;"..F(S("Value (use empty value to delete key)"))..";"..F(value).."]"..
		"button[4,8;3,1;set;"..F(S("Set value")).."]"

	local extra_label
	local formname
	if metatype == "node" then
		formname = "testtools:node_meta_editor"
		extra_label = S("pos = @1", core.pos_to_string(pos_or_item))
	else
		formname = "testtools:item_meta_editor"
		extra_label = S("item = @1", pos_or_item:get_name())
	end
	form = form .. "label[0,7.2;"..F(extra_label).."]"

	core.show_formspec(user:get_player_name(), formname, form)
end

local function get_meta_keylist(meta, playername, escaped)
	local keys = {}
	local ekeys = {}
	local mtable = meta:to_table()
	for k,_ in pairs(mtable.fields) do
		table.insert(keys, k)
		if escaped then
			table.insert(ekeys, F(k))
		else
			table.insert(ekeys, k)
		end
	end
	if playername then
		meta_latest_keylist[playername] = keys
	end
	return table.concat(ekeys, ",")
end

core.register_tool("testtools:node_meta_editor", {
	description = S("Node Meta Editor") .. "\n" ..
		S("Place: Edit node metadata"),
	inventory_image = "testtools_node_meta_editor.png",
	groups = { testtool = 1, disable_repair = 1 },
	on_place = function(itemstack, user, pointed_thing)
		if pointed_thing.type ~= "node" then
			return itemstack
		end
		if not user:is_player() then
			return itemstack
		end
		local pos = pointed_thing.under
		node_meta_posses[user:get_player_name()] = pos
		local meta = core.get_meta(pos)
		local inv = meta:get_inventory()
		show_meta_formspec(user, "node", pos, "", "", get_meta_keylist(meta, user:get_player_name(), true))
		return itemstack
	end,
})

local function get_item_next_to_wielded_item(player)
	local inv = player:get_inventory()
	local wield = player:get_wield_index()
	local itemstack = inv:get_stack("main", wield+1)
	return itemstack
end
local function set_item_next_to_wielded_item(player, itemstack)
	local inv = player:get_inventory()
	local wield = player:get_wield_index()
	inv:set_stack("main", wield+1, itemstack)
end

local function use_item_meta_editor(itemstack, user, pointed_thing)
	if not user:is_player() then
		return itemstack
	end
	local item_to_edit = get_item_next_to_wielded_item(user)
	if item_to_edit:is_empty() then
		core.chat_send_player(user:get_player_name(), S("Place an item next to the Item Meta Editor in your inventory first!"))
		return itemstack
	end
	local meta = item_to_edit:get_meta()
	show_meta_formspec(user, "item", item_to_edit, "", "", get_meta_keylist(meta, user:get_player_name(), true))
	return itemstack
end

core.register_tool("testtools:item_meta_editor", {
	description = S("Item Meta Editor") .. "\n" ..
		S("Punch/Place: Edit item metadata of item in the next inventory slot"),
	inventory_image = "testtools_item_meta_editor.png",
	groups = { testtool = 1, disable_repair = 1 },
	on_use = use_item_meta_editor,
	on_secondary_use = use_item_meta_editor,
	on_place = use_item_meta_editor,
})

core.register_on_player_receive_fields(function(player, formname, fields)
	if not (player and player:is_player()) then
		return
	end
	if formname == "testtools:entity_list" then
		local name = player:get_player_name()
		if fields.entity_list then
			local expl = core.explode_textlist_event(fields.entity_list)
			if expl.type == "DCL" then
				local pos = vector.add(player:get_pos(), {x=0,y=1,z=0})
				selections[name] = expl.index
				core.add_entity(pos, get_entity_list()[expl.index])
				return
			elseif expl.type == "CHG" then
				selections[name] = expl.index
				return
			end
		elseif fields.spawn and selections[name] then
			local pos = vector.add(player:get_pos(), {x=0,y=1,z=0})
			core.add_entity(pos, get_entity_list()[selections[name]])
			return
		end
	elseif formname == "testtools:object_editor" then
		local name = player:get_player_name()
		if fields.object_props then
			local expl = core.explode_textlist_event(fields.object_props)
			if expl.type == "DCL" or expl.type == "CHG" then
				property_formspec_index[name] = expl.index

				local props = selected_objects[name]:get_properties()
				local keys = property_formspec_data[name]
				if (not property_formspec_index[name]) or (not props) then
					return
				end
				local key = keys[property_formspec_index[name]]
				editor_formspec_selindex[name] = expl.index
				editor_formspec(name, selected_objects[name], prop_to_string(props[key]), expl.index)
				return
			end
		end
		if fields.key_enter_field == "value" or fields.submit then
			local props = selected_objects[name]:get_properties()
			local keys = property_formspec_data[name]
			if (not property_formspec_index[name]) or (not props) then
				return
			end
			local key = keys[property_formspec_index[name]]
			if not key then
				return
			end
			local success, str = use_loadstring(fields.value, player)
			if success then
				props[key] = str
			else
				core.chat_send_player(name, str)
				return
			end
			selected_objects[name]:set_properties(props)
			local sel = editor_formspec_selindex[name]
			editor_formspec(name, selected_objects[name], prop_to_string(props[key]), sel)
			return
		end
	elseif formname == "testtools:node_setter" then
		local playername = player:get_player_name()
		local witem = player:get_wielded_item()
		if witem:get_name() == "testtools:node_setter" then
			if fields.nodename and fields.param2 then
				local param2 = tonumber(fields.param2)
				if not param2 then
					return
				end
				local meta = witem:get_meta()
				meta:set_string("node", fields.nodename)
				meta:set_int("node_param2", param2)
				player:set_wielded_item(witem)
			end
		end
	elseif formname == "testtools:node_meta_editor" or formname == "testtools:item_meta_editor" then
		local name = player:get_player_name()
		local metatype
		local pos_or_item
		if formname == "testtools:node_meta_editor" then
			metatype = "node"
			pos_or_item = node_meta_posses[name]
		else
			metatype = "item"
			pos_or_item = get_item_next_to_wielded_item(player)
		end
		if fields.keylist then
			local evnt = core.explode_textlist_event(fields.keylist)
			if evnt.type == "DCL" or evnt.type == "CHG" then
				local keylist_table = meta_latest_keylist[name]
				if metatype == "node" and not pos_or_item then
					return
				end
				local meta
				if metatype == "node" then
					meta = core.get_meta(pos_or_item)
				else
					meta = pos_or_item:get_meta()
				end
				if not keylist_table then
					return
				end
				if #keylist_table == 0 then
					return
				end
				local key = keylist_table[evnt.index]
				local value = meta:get_string(key)
				local keylist_escaped = {}
				for k,v in pairs(keylist_table) do
					keylist_escaped[k] = F(v)
				end
				local keylist = table.concat(keylist_escaped, ",")
				show_meta_formspec(player, metatype, pos_or_item, key, value, keylist)
				return
			end
		elseif fields.key and fields.key ~= "" and fields.value then
			if metatype == "node" and not pos_or_item then
				return
			end
			local meta
			if metatype == "node" then
				meta = core.get_meta(pos_or_item)
			elseif metatype == "item" then
				if pos_or_item:is_empty() then
					return
				end
				meta = pos_or_item:get_meta()
			end
			if fields.set then
				meta:set_string(fields.key, fields.value)
				if metatype == "item" then
					set_item_next_to_wielded_item(player, pos_or_item)
				end
				show_meta_formspec(player, metatype, pos_or_item, fields.key, fields.value,
						get_meta_keylist(meta, name, true))
			end
			return
		end
	end
end)

core.register_on_leaveplayer(function(player)
	local name = player:get_player_name()
	meta_latest_keylist[name] = nil
	node_meta_posses[name] = nil
end)

-- Pointing Staffs

core.register_tool("testtools:blocked_pointing_staff", {
	description = S("Blocked Pointing Staff").."\n"..
			S("Can point the Blocking Pointable Node/Object and "..
			"the Pointable Node/Object is point blocking."),
	inventory_image = "testtools_blocked_pointing_staff.png",
	pointabilities = {
		nodes = {
			["testnodes:blocking_pointable"] = true,
			["group:pointable_test"] = "blocking"
		},
		objects = {
			["testentities:blocking_pointable"] = true,
			["group:pointable_test"] = "blocking"
		}
	}
})

core.register_tool("testtools:ultimate_pointing_staff", {
	description = S("Ultimate Pointing Staff").."\n"..
			S("Can point all pointable test nodes, objects and liquids."),
	inventory_image = "testtools_ultimate_pointing_staff.png",
	liquids_pointable = true,
	pointabilities = {
		nodes = {
			["group:blocking_pointable_test"] = true,
			["group:pointable_test"] = true,
			["testnodes:not_pointable"] = true
		},
		objects = {
			["group:blocking_pointable_test"] = true,
			["group:pointable_test"] = true,
			["testentities:not_pointable"] = true
		}
	}
})
