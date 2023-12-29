-- add command to change node_visual

core.register_chatcommand("node_visual", {
	params = "nodename field [value]",
	description = "Change node_visual field of actual player to value or show value of field.",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end

		local splits = string.split(param, " ", false, 3)

		if #splits < 2 then
			return false, "Expected node name and node_visual field as parameters."
		end

		local node_name = splits[1]
		local field_name = splits[2]

		if not core.registered_nodes[node_name] then
			return false, "Unknown node "..node_name
		end

		local node_visual = player:get_node_visual(node_name)
		
		if rawequal(node_visual[field_name], nil) then
			return false, "Field "..field_name.." not found in node_visual."
		end
		
		if #splits > 2 then
			if type(node_visual[field_name]) == "number" then
				node_visual[field_name] = tonumber(splits[3])
			elseif type(node_visual[field_name]) == "table" then
				node_visual[field_name] = core.parse_json(splits[3])
				if type(node_visual[field_name]) ~= "table" then
					return false, "Table in json format is expected as value."
				end
			else
				node_visual[field_name] = splits[3]
			end
			player:set_node_visual(node_name, node_visual)
			return true, "Node "..node_name.." node_visual field "..field_name.." set to value: "..dump(node_visual[field_name])
		else
			return true, "Node "..node_name.." node_visual field "..field_name.." have value: "..dump(node_visual[field_name])
		end
	end
})
