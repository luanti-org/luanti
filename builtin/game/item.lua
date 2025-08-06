local builtin_shared = ...

local function copy_pointed_thing(pointed_thing)
	return {
		type  = pointed_thing.type,
		above = pointed_thing.above and vector.copy(pointed_thing.above),
		under = pointed_thing.under and vector.copy(pointed_thing.under),
		ref   = pointed_thing.ref,
	}
end

--
-- Item definition helpers
--

function core.get_pointed_thing_position(pointed_thing, above)
	if pointed_thing.type == "node" then
		if above then
			-- The position where a node would be placed
			return pointed_thing.above
		end
		-- The position where a node would be dug
		return pointed_thing.under
	elseif pointed_thing.type == "object" then
		return pointed_thing.ref and pointed_thing.ref:get_pos()
	end
end

local function has_all_groups(tbl, required_groups)
	if type(required_groups) == "string" then
		return (tbl[required_groups] or 0) ~= 0
	end
	for _, group in ipairs(required_groups) do
		if (tbl[group] or 0) == 0 then
			return false
		end
	end
	return true
end

function core.get_node_drops(node, toolname)
	-- Compatibility, if node is string
	local nodename = node
	local param2 = 0
	-- New format, if node is table
	if (type(node) == "table") then
		nodename = node.name
		param2 = node.param2
	end
	local def = core.registered_nodes[nodename]
	local drop = def and def.drop
	local ptype = def and def.paramtype2
	-- get color, if there is color (otherwise nil)
	local palette_index = core.strip_param2_color(param2, ptype)
	if drop == nil then
		-- default drop
		if palette_index then
			local stack = ItemStack(nodename)
			stack:get_meta():set_int("palette_index", palette_index)
			return {stack:to_string()}
		end
		return {nodename}
	elseif type(drop) == "string" then
		-- itemstring drop
		return drop ~= "" and {drop} or {}
	elseif drop.items == nil then
		-- drop = {} to disable default drop
		return {}
	end

	-- Extended drop table
	local got_items = {}
	local got_count = 0
	for _, item in ipairs(drop.items) do
		local good_rarity = true
		local good_tool = true
		if item.rarity ~= nil then
			good_rarity = item.rarity < 1 or math.random(item.rarity) == 1
		end
		if item.tools ~= nil or item.tool_groups ~= nil then
			good_tool = false
		end
		if item.tools ~= nil and toolname then
			for _, tool in ipairs(item.tools) do
				if tool:sub(1, 1) == '~' then
					good_tool = toolname:find(tool:sub(2)) ~= nil
				else
					good_tool = toolname == tool
				end
				if good_tool then
					break
				end
			end
		end
		if item.tool_groups ~= nil and toolname then
			local tooldef = core.registered_items[toolname]
			if tooldef ~= nil and type(tooldef.groups) == "table" then
				if type(item.tool_groups) == "string" then
					-- tool_groups can be a string which specifies the required group
					good_tool = core.get_item_group(toolname, item.tool_groups) ~= 0
				else
					-- tool_groups can be a list of sufficient requirements.
					-- i.e. if any item in the list can be satisfied then the tool is good
					assert(type(item.tool_groups) == "table")
					for _, required_groups in ipairs(item.tool_groups) do
						-- required_groups can be either a string (a single group),
						-- or an array of strings where all must be in tooldef.groups
						good_tool = has_all_groups(tooldef.groups, required_groups)
						if good_tool then
							break
						end
					end
				end
			end
		end
		if good_rarity and good_tool then
			got_count = got_count + 1
			for _, add_item in ipairs(item.items) do
				-- add color, if necessary
				if item.inherit_color and palette_index then
					local stack = ItemStack(add_item)
					stack:get_meta():set_int("palette_index", palette_index)
					add_item = stack:to_string()
				end
				got_items[#got_items+1] = add_item
			end
			if drop.max_items ~= nil and got_count == drop.max_items then
				break
			end
		end
	end
	return got_items
end

local function user_name(user)
	return user and user:get_player_name() or ""
end

-- Returns a logging function. For empty names, does not log.
local function make_log(name)
	return name ~= "" and core.log or function() end
end

function core.item_place_node(itemstack, placer, pointed_thing, param2,
		prevent_after_place)
	local def = itemstack:get_definition()
	if def.type ~= "node" or pointed_thing.type ~= "node" then
		return itemstack, nil
	end

	local under = pointed_thing.under
	local oldnode_under = core.get_node_or_nil(under)
	local above = pointed_thing.above
	local oldnode_above = core.get_node_or_nil(above)
	local playername = user_name(placer)
	local log = make_log(playername)

	if not oldnode_under or not oldnode_above then
		log("info", playername .. " tried to place"
			.. " node in unloaded position " .. core.pos_to_string(above))
		return itemstack, nil
	end

	local olddef_under = core.registered_nodes[oldnode_under.name]
	olddef_under = olddef_under or core.nodedef_default
	local olddef_above = core.registered_nodes[oldnode_above.name]
	olddef_above = olddef_above or core.nodedef_default

	if not olddef_above.buildable_to and not olddef_under.buildable_to then
		log("info", playername .. " tried to place"
			.. " node in invalid position " .. core.pos_to_string(above)
			.. ", replacing " .. oldnode_above.name)
		return itemstack, nil
	end

	-- Place above pointed node
	local place_to = vector.copy(above)

	-- If node under is buildable_to, place into it instead (eg. snow)
	if olddef_under.buildable_to then
		log("info", "node under is buildable to")
		place_to = vector.copy(under)
	end

	if core.is_protected(place_to, playername) then
		log("action", playername
				.. " tried to place " .. def.name
				.. " at protected position "
				.. core.pos_to_string(place_to))
		core.record_protection_violation(place_to, playername)
		return itemstack, nil
	end

	local oldnode = core.get_node(place_to)
	local newnode = {name = def.name, param1 = 0, param2 = param2 or 0}

	-- Calculate direction for wall mounted stuff like torches and signs
	if def.place_param2 ~= nil then
		newnode.param2 = def.place_param2
	elseif (def.paramtype2 == "wallmounted" or
			def.paramtype2 == "colorwallmounted") and not param2 then
		local dir = vector.subtract(under, above)
		-- If you change this code, also change src/client/game.cpp
		newnode.param2 = core.dir_to_wallmounted(dir)
		if def.wallmounted_rotate_vertical and
				(newnode.param2 == 0 or newnode.param2 == 1) then
			local placer_pos = placer and placer:get_pos()
			if placer_pos then
				local pdir = {
					x = above.x - placer_pos.x,
					y = dir.y,
					z = above.z - placer_pos.z
				}
				local rotate = false
				if def.drawtype == "torchlike" then
					if not ((pdir.x < 0 and pdir.z > 0) or
							(pdir.x > 0 and pdir.z < 0)) then
						rotate = true
					end
					if pdir.y > 0 then
						rotate = not rotate
					end
				elseif def.drawtype == "signlike" then
					if math.abs(pdir.x) < math.abs(pdir.z) then
						rotate = true
					end
				else
					if math.abs(pdir.x) > math.abs(pdir.z) then
						rotate = true
					end
				end
				if rotate then
					newnode.param2 = newnode.param2 + 6
				end
			end
		end
	-- Calculate the direction for furnaces and chests and stuff
	elseif (def.paramtype2 == "facedir" or
			def.paramtype2 == "colorfacedir" or
			def.paramtype2 == "4dir" or
			def.paramtype2 == "color4dir") and not param2 then
		local placer_pos = placer and placer:get_pos()
		if placer_pos then
			local dir = vector.subtract(above, placer_pos)
			newnode.param2 = core.dir_to_facedir(dir)
			log("info", "facedir: " .. newnode.param2)
		end
	end

	local metatable = itemstack:get_meta():to_table().fields

	-- Transfer color information
	if metatable.palette_index and not def.place_param2 then
		local color_divisor = nil
		if def.paramtype2 == "color" then
			color_divisor = 1
		elseif def.paramtype2 == "colorwallmounted" then
			color_divisor = 8
		elseif def.paramtype2 == "colorfacedir" then
			color_divisor = 32
		elseif def.paramtype2 == "color4dir" then
			color_divisor = 4
		elseif def.paramtype2 == "colordegrotate" then
			color_divisor = 32
		end
		if color_divisor then
			local color = math.floor(metatable.palette_index / color_divisor)
			local other = newnode.param2 % color_divisor
			newnode.param2 = color * color_divisor + other
		end
	end

	-- Check if the node is attached and if it can be placed there
	local an = core.get_item_group(def.name, "attached_node")
	if an ~= 0 and
		not builtin_shared.check_attached_node(place_to, newnode, an) then
		log("action", "attached node " .. def.name ..
			" cannot be placed at " .. core.pos_to_string(place_to))
		return itemstack, nil
	end

	log("action", playername .. " places node "
		.. def.name .. " at " .. core.pos_to_string(place_to))

	-- Add node and update
	core.add_node(place_to, newnode)

	-- Play sound if it was done by a player
	if playername ~= "" and def.sounds and def.sounds.place then
		core.sound_play(def.sounds.place, {
			pos = place_to,
			exclude_player = playername,
		}, true)
	end

	local take_item = true

	-- Run callback
	if def.after_place_node and not prevent_after_place then
		-- Deepcopy place_to and pointed_thing because callback can modify it
		local place_to_copy = vector.copy(place_to)
		local pointed_thing_copy = copy_pointed_thing(pointed_thing)
		if def.after_place_node(place_to_copy, placer, itemstack,
				pointed_thing_copy) then
			take_item = false
		end
	end

	-- Run script hook
	for _, callback in ipairs(core.registered_on_placenodes) do
		-- Deepcopy pos, node and pointed_thing because callback can modify them
		local place_to_copy = vector.copy(place_to)
		local newnode_copy = {name=newnode.name, param1=newnode.param1, param2=newnode.param2}
		local oldnode_copy = {name=oldnode.name, param1=oldnode.param1, param2=oldnode.param2}
		local pointed_thing_copy = copy_pointed_thing(pointed_thing)
		if callback(place_to_copy, newnode_copy, placer, oldnode_copy, itemstack, pointed_thing_copy) then
			take_item = false
		end
	end

	if take_item then
		itemstack:take_item()
	end
	return itemstack, place_to
end

-- deprecated, item_place does not call this
function core.item_place_object(itemstack, placer, pointed_thing)
	local pos = core.get_pointed_thing_position(pointed_thing, true)
	if pos ~= nil then
		local item = itemstack:take_item()
		core.add_item(pos, item)
	end
	return itemstack
end

function core.item_place(itemstack, placer, pointed_thing, param2)
	-- Call on_rightclick if the pointed node defines it
	if pointed_thing.type == "node" and placer and
			not placer:get_player_control().sneak then
		local n = core.get_node(pointed_thing.under)
		local nn = n.name
		if core.registered_nodes[nn] and core.registered_nodes[nn].on_rightclick then
			return core.registered_nodes[nn].on_rightclick(pointed_thing.under, n,
					placer, itemstack, pointed_thing) or itemstack, nil
		end
	end

	-- Place if node, otherwise do nothing
	if itemstack:get_definition().type == "node" then
		return core.item_place_node(itemstack, placer, pointed_thing, param2)
	end
	return itemstack, nil
end

function core.item_secondary_use(itemstack, user)
	return itemstack
end

function core.item_drop(itemstack, dropper, pos)
	local dropper_is_player = dropper and dropper:is_player()
	local p = table.copy(pos)
	if dropper_is_player then
		p.y = p.y + 1.2
	end
	local obj = core.add_item(p, ItemStack(itemstack))
	if obj then
		itemstack:clear()
		if dropper_is_player then
			local dir = dropper:get_look_dir()
			dir.x = dir.x * 2.9
			dir.y = dir.y * 2.9 + 2
			dir.z = dir.z * 2.9
			obj:set_velocity(dir)
			obj:get_luaentity().dropped_by = dropper:get_player_name()
		end
		return itemstack, obj
	end
	-- If we reach this, adding the object to the
	-- environment failed
end

function core.item_pickup(itemstack, picker, pointed_thing, ...)
	itemstack = ItemStack(itemstack)
	-- Invoke global on_item_pickup callbacks.
	for _, callback in ipairs(core.registered_on_item_pickups) do
		local result = callback(itemstack, picker, pointed_thing, ...)
		if result then
			return ItemStack(result)
		end
	end

	-- Pick up item
	local inv = picker and picker:get_inventory()
	if inv then
		return inv:add_item("main", itemstack)
	end
	return itemstack
end

function core.do_item_eat(hp_change, replace_with_item, itemstack, user, pointed_thing)
	for _, callback in ipairs(core.registered_on_item_eats) do
		local result = callback(hp_change, replace_with_item, itemstack, user, pointed_thing)
		if result then
			return result
		end
	end
	-- read definition before potentially emptying the stack
	local def = itemstack:get_definition()
	if itemstack:take_item():is_empty() then
		return itemstack
	end

	if def and def.sound and def.sound.eat then
		core.sound_play(def.sound.eat, {
			pos = user:get_pos(),
			max_hear_distance = 16
		}, true)
	end

	-- Changing hp might kill the player causing mods to do who-knows-what to the
	-- inventory, so do this before set_hp().
	replace_with_item = itemstack:add_item(replace_with_item)
	user:set_wielded_item(itemstack)
	if not replace_with_item:is_empty() then
		local inv = user:get_inventory()
		-- Check if inv is null, since non-players don't have one
		if inv then
			replace_with_item = inv:add_item("main", replace_with_item)
		end
	end
	if not replace_with_item:is_empty() then
		local pos = user:get_pos()
		pos.y = math.floor(pos.y + 0.5)
		core.add_item(pos, replace_with_item)
	end

	user:set_hp(user:get_hp() + hp_change)

	return nil -- don't overwrite wield item a second time
end

function core.item_eat(hp_change, replace_with_item)
	return function(itemstack, user, pointed_thing)  -- closure
		if user then
			return core.do_item_eat(hp_change, replace_with_item, itemstack, user, pointed_thing)
		end
	end
end

function core.node_punch(pos, node, puncher, pointed_thing)
	-- Run script hook
	for _, callback in ipairs(core.registered_on_punchnodes) do
		-- Copy pos and node because callback can modify them
		local pos_copy = vector.copy(pos)
		local node_copy = {name=node.name, param1=node.param1, param2=node.param2}
		local pointed_thing_copy = pointed_thing and copy_pointed_thing(pointed_thing) or nil
		callback(pos_copy, node_copy, puncher, pointed_thing_copy)
	end
end

function core.handle_node_drops(pos, drops, digger)
	-- Add dropped items to object's inventory
	local inv = digger and digger:get_inventory()
	local give_item
	if inv then
		give_item = function(item)
			return inv:add_item("main", item)
		end
	else
		give_item = function(item)
			-- itemstring to ItemStack for left:is_empty()
			return ItemStack(item)
		end
	end

	for _, dropped_item in pairs(drops) do
		local left = give_item(dropped_item)
		if not left:is_empty() then
			local p = vector.offset(pos,
				math.random()/2-0.25,
				math.random()/2-0.25,
				math.random()/2-0.25
			)
			core.add_item(p, left)
		end
	end
end

function core.node_dig(pos, node, digger)
	local diggername = user_name(digger)
	local log = make_log(diggername)
	local def = core.registered_nodes[node.name]
	-- Copy pos because the callback could modify it
	if def and (not def.diggable or
			(def.can_dig and not def.can_dig(vector.copy(pos), digger))) then
		log("info", diggername .. " tried to dig "
			.. node.name .. " which is not diggable "
			.. core.pos_to_string(pos))
		return false
	end

	if core.is_protected(pos, diggername) then
		log("action", diggername
				.. " tried to dig " .. node.name
				.. " at protected position "
				.. core.pos_to_string(pos))
		core.record_protection_violation(pos, diggername)
		return false
	end

	log('action', diggername .. " digs "
		.. node.name .. " at " .. core.pos_to_string(pos))

	local wielded = digger and digger:get_wielded_item()
	local drops = core.get_node_drops(node, wielded and wielded:get_name(),
				wielded and ItemStack(wielded), digger, vector.copy(pos))

	if wielded then
		local wdef = wielded:get_definition()
		local tp = wielded:get_tool_capabilities()
		local dp = core.get_dig_params(def and def.groups, tp, wielded:get_wear())
		if wdef and wdef.after_use then
			wielded = wdef.after_use(wielded, digger, node, dp) or wielded
		else
			-- Wear out tool
			if not core.is_creative_enabled(diggername) then
				wielded:add_wear(dp.wear)
				if wielded:get_count() == 0 and wdef.sound and wdef.sound.breaks then
					core.sound_play(wdef.sound.breaks, {
						pos = pos,
						gain = 0.5
					}, true)
				end
			end
		end
		digger:set_wielded_item(wielded)
	end

	-- Check to see if metadata should be preserved.
	if def and def.preserve_metadata then
		local oldmeta = core.get_meta(pos):to_table().fields
		-- Copy pos and node because the callback can modify them.
		local pos_copy = vector.copy(pos)
		local node_copy = {name=node.name, param1=node.param1, param2=node.param2}
		local drop_stacks = {}
		for k, v in pairs(drops) do
			drop_stacks[k] = ItemStack(v)
		end
		drops = drop_stacks
		def.preserve_metadata(pos_copy, node_copy, oldmeta, drops)
	end

	-- Handle drops
	core.handle_node_drops(pos, drops, digger)

	local oldmetadata = nil
	if def and def.after_dig_node then
		oldmetadata = core.get_meta(pos):to_table()
	end

	-- Remove node and update
	core.remove_node(pos)

	-- Play sound if it was done by a player
	if diggername ~= "" and def and def.sounds and def.sounds.dug then
		core.sound_play(def.sounds.dug, {
			pos = pos,
			exclude_player = diggername,
		}, true)
	end

	-- Run callback
	if def and def.after_dig_node then
		-- Copy pos and node because callback can modify them
		local pos_copy = vector.copy(pos)
		local node_copy = {name=node.name, param1=node.param1, param2=node.param2}
		def.after_dig_node(pos_copy, node_copy, oldmetadata, digger)
	end

	-- Run script hook
	for _, callback in ipairs(core.registered_on_dignodes) do
		local origin = core.callback_origins[callback]
		core.set_last_run_mod(origin.mod)

		-- Copy pos and node because callback can modify them
		local pos_copy = vector.copy(pos)
		local node_copy = {name=node.name, param1=node.param1, param2=node.param2}
		callback(pos_copy, node_copy, digger)
	end

	return true
end

function core.itemstring_with_palette(item, palette_index)
	local stack = ItemStack(item) -- convert to ItemStack
	stack:get_meta():set_int("palette_index", palette_index)
	return stack:to_string()
end

function core.itemstring_with_color(item, colorstring)
	local stack = ItemStack(item) -- convert to ItemStack
	stack:get_meta():set_string("color", colorstring)
	return stack:to_string()
end

-- This is used to allow mods to redefine core.item_place and so on
-- NOTE: This is not the preferred way. Preferred way is to provide enough
--       callbacks to not require redefining global functions. -celeron55
local function redef_wrapper(table, name)
	return function(...)
		return table[name](...)
	end
end

--
-- Item definition defaults
--

local default_stack_max = tonumber(core.settings:get("default_stack_max")) or 99

core.nodedef_default = {
	-- Item properties
	type="node",
	-- name intentionally not defined here
	description = "",
	groups = {},
	inventory_image = "",
	wield_image = "",
	wield_scale = vector.new(1, 1, 1),
	stack_max = default_stack_max,
	usable = false,
	liquids_pointable = false,
	tool_capabilities = nil,
	node_placement_prediction = nil,

	-- Interaction callbacks
	on_place = redef_wrapper(core, 'item_place'), -- core.item_place
	on_drop = redef_wrapper(core, 'item_drop'), -- core.item_drop
	on_pickup = redef_wrapper(core, 'item_pickup'), -- core.item_pickup
	on_use = nil,
	after_use = nil,
	can_dig = nil,

	on_punch = redef_wrapper(core, 'node_punch'), -- core.node_punch
	on_rightclick = nil,
	on_dig = redef_wrapper(core, 'node_dig'), -- core.node_dig

	on_receive_fields = nil,

	-- Node properties
	drawtype = "normal",
	visual_scale = 1.0,
	tiles = nil,
	special_tiles = nil,
	post_effect_color = {a=0, r=0, g=0, b=0},
	paramtype = "none",
	paramtype2 = "none",
	is_ground_content = true,
	sunlight_propagates = false,
	walkable = true,
	pointable = true,
	diggable = true,
	climbable = false,
	buildable_to = false,
	floodable = false,
	liquidtype = "none",
	liquid_alternative_flowing = "",
	liquid_alternative_source = "",
	liquid_viscosity = 0,
	drowning = 0,
	light_source = 0,
	damage_per_second = 0,
	selection_box = {type="regular"},
	legacy_facedir_simple = false,
	legacy_wallmounted = false,
}

core.craftitemdef_default = {
	type="craft",
	-- name intentionally not defined here
	description = "",
	groups = {},
	inventory_image = "",
	wield_image = "",
	wield_scale = vector.new(1, 1, 1),
	stack_max = default_stack_max,
	liquids_pointable = false,
	tool_capabilities = nil,

	-- Interaction callbacks
	on_place = redef_wrapper(core, 'item_place'), -- core.item_place
	on_drop = redef_wrapper(core, 'item_drop'), -- core.item_drop
	on_pickup = redef_wrapper(core, 'item_pickup'), -- core.item_pickup
	on_secondary_use = redef_wrapper(core, 'item_secondary_use'),
	on_use = nil,
}

core.tooldef_default = {
	type="tool",
	-- name intentionally not defined here
	description = "",
	groups = {},
	inventory_image = "",
	wield_image = "",
	wield_scale = vector.new(1, 1, 1),
	stack_max = 1,
	liquids_pointable = false,
	tool_capabilities = nil,

	-- Interaction callbacks
	on_place = redef_wrapper(core, 'item_place'), -- core.item_place
	on_secondary_use = redef_wrapper(core, 'item_secondary_use'),
	on_drop = redef_wrapper(core, 'item_drop'), -- core.item_drop
	on_pickup = redef_wrapper(core, 'item_pickup'), -- core.item_pickup
	on_use = nil,
}

core.noneitemdef_default = {  -- This is used for the hand and unknown items
	type="none",
	-- name intentionally not defined here
	description = "",
	groups = {},
	inventory_image = "",
	wield_image = "",
	wield_scale = vector.new(1, 1, 1),
	stack_max = default_stack_max,
	liquids_pointable = false,
	tool_capabilities = nil,

	-- Interaction callbacks
	on_place = redef_wrapper(core, 'item_place'), -- core.item_place
	on_secondary_use = redef_wrapper(core, 'item_secondary_use'),
	on_pickup = redef_wrapper(core, 'item_pickup'), -- core.item_pickup
	on_drop = nil,
	on_use = nil,
}

--
-- get_node implementation
--

local get_node_raw = core.get_node_raw
local get_name_from_content_id = core.get_name_from_content_id

function core.get_node(pos)
	local content, param1, param2 = get_node_raw(pos.x, pos.y, pos.z)
	return {name = get_name_from_content_id(content), param1 = param1, param2 = param2}
end

function core.get_node_or_nil(pos)
	local content, param1, param2, pos_ok = get_node_raw(pos.x, pos.y, pos.z)
	return pos_ok and
			{name = get_name_from_content_id(content), param1 = param1, param2 = param2}
			or nil
end
