-- Add chat command to place all the nodes in DevTest

local function advance_pos(pos, start_pos, advance_z)
	if advance_z then
		pos.z = pos.z + 2
		pos.x = start_pos.x
	else
		pos.x = pos.x + 2
	end
	if pos.x > 30900 or pos.x - start_pos.x > 46 then
		pos.x = start_pos.x
		pos.z = pos.z + 2
	end
	if pos.z > 30900 then
		-- We ran out of space! Aborting
		aborted = true
		return false
	end
	return pos
end

local function place_nodes(param)
	local nodes = param.nodes
	local name = param.name
	local pos = param.pos
	local start_pos = param.start_pos
	table.sort(nodes)
	core.chat_send_player(name, "Placing nodes …")
	local nodes_placed = 0
	local aborted = false
	for n=1, #nodes do
		local itemstring = nodes[n]
		local def = core.registered_nodes[itemstring]
		local p2_max = 0
		if param.param ~= "no_param2" then
			-- Also test the param2 values of the nodes
			-- ... but we only use permissible param2 values
			if def.paramtype2 == "wallmounted" then
				p2_max = 5
			elseif def.paramtype2 == "facedir" then
				p2_max = 23
			elseif def.paramtype2 == "4dir" then
				p2_max = 3
			elseif def.paramtype2 == "glasslikeliquidlevel" then
				p2_max = 63
			elseif def.paramtype2 == "meshoptions" and def.drawtype == "plantlike" then
				p2_max = 63
			elseif def.paramtype2 == "leveled" then
				p2_max = 127
			elseif def.paramtype2 == "degrotate" and (def.drawtype == "plantlike" or def.drawtype == "mesh") then
				p2_max = 239
			elseif def.paramtype2 == "colorfacedir" or
				def.paramtype2 == "colorwallmounted" or
				def.paramtype2 == "colordegrotate" or
				def.paramtype2 == "color4dir" or
				def.paramtype2 == "color" then
				p2_max = 255
			end
		end
		for p2 = 0, p2_max do
			-- Skip undefined param2 values
			if not ((def.paramtype2 == "meshoptions" and p2 % 8 > 4) or
					(def.paramtype2 == "colorwallmounted" and p2 % 8 > 5) or
					((def.paramtype2 == "colorfacedir" or def.paramtype2 == "colordegrotate")
					and p2 % 32 > 23)) then

				core.set_node(pos, { name = itemstring, param2 = p2 })
				nodes_placed = nodes_placed + 1
				pos = advance_pos(pos, start_pos)
				if not pos then
					aborted = true
					break
				end
			end
		end
		if aborted then
			break
		end
	end
	if aborted then
		core.chat_send_player(name, "Not all nodes could be placed, please move further away from the world boundary. Nodes placed: "..nodes_placed)
	end
	core.chat_send_player(name, "Nodes placed: "..nodes_placed..".")
end

local function after_emerge(blockpos, action, calls_remaining, param)
	if calls_remaining == 0 then
		place_nodes(param)
	end
end

core.register_chatcommand("test_place_nodes", {
	params = "[ no_param2 ]",
	description = "Test: Place all nodes (except dummy and callback nodes) and optionally their permissible param2 variants",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local pos = vector.floor(player:get_pos())
		pos.x = math.ceil(pos.x + 3)
		pos.z = math.ceil(pos.z + 3)
		pos.y = math.ceil(pos.y + 1)
		local start_pos = table.copy(pos)
		if pos.x > 30800 then
			return false, "Too close to world boundary (+X). Please move to X < 30800."
		end
		if pos.z > 30800 then
			return false, "Too close to world boundary (+Z). Please move to Z < 30800."
		end

		local aborted = false
		local nodes = {}
		local emerge_estimate = 0
		for itemstring, def in pairs(core.registered_nodes) do
			if itemstring ~= "ignore" and
					-- Skip callback test and dummy nodes
					-- to avoid clutter and chat spam
					core.get_item_group(itemstring, "callback_test") == 0 and
					core.get_item_group(itemstring, "dummy") == 0 then
				table.insert(nodes, itemstring)
				if def.paramtype2 == 0 then
					emerge_estimate = emerge_estimate + 1
				else
					emerge_estimate = emerge_estimate + 255
				end
			end
		end
		-- Emerge area to make sure that all nodes are being placed.
		-- Note we will emerge much more than we need to (overestimation),
		-- the estimation code could be improved performance-wise …
		local length = 16 + math.ceil(emerge_estimate / 24) * 2
		core.emerge_area(start_pos,
			{ x = start_pos.x + 46, y = start_pos.y, z = start_pos.z + length },
			after_emerge, { nodes = nodes, name = name, pos = pos, start_pos = start_pos, param = param })
		return true, "Emerging area …"
	end,
})

