local S = core.get_translator("testpathfinder")

-- Config parameters

-- Maximum direct distance between start and end
local MAX_DIRECT_DISTANCE = 64
-- Maximum search distance
local MAX_SEARCH_DISTANCE = 32
-- Maximum permitted jump height
local MAX_JUMP = 1
-- Maximum permitted drop height
local MAX_DROP = 5
-- If true, mod won't refuse to run pathfinder even at long distances
local IGNORE_MAX_DISTANCE_SAFEGUARD = false

-- End of config parameters

local timer = 0
local algorithms = {
	"A*_noprefetch",
	"A*",
	"Dijkstra",
}

local function find_path_for_player(player, itemstack)
	local meta = itemstack:get_meta()
	if not meta then
		return
	end
	local x = meta:get_int("pos_x")
	local y = meta:get_int("pos_y")
	local z = meta:get_int("pos_z")
	local algo = meta:get_int("algorithm")
	if x and y and z then
		local pos2 = {x=x, y=y, z=z}
		algo = algorithms[algo+1]
		local pos1 = vector.round(player:get_pos())
		-- Don't bother calling pathfinder for high distance to avoid freezing
		if (not IGNORE_MAX_DISTANCE_SAFEGUARD) and (vector.distance(pos1, pos2) > MAX_DIRECT_DISTANCE) then
			core.chat_send_player(player:get_player_name(), S("Destination too far away! Set a destination (via placing) within a distance of @1 and try again!", MAX_DIRECT_DISTANCE))
			return
		end
		local str = S("Path from @1 to @2:",
			core.pos_to_string(pos1),
			core.pos_to_string(pos2))

		core.chat_send_player(player:get_player_name(), str)
		local time_start = core.get_us_time()
		local path = core.find_path(pos1, pos2, MAX_SEARCH_DISTANCE, MAX_JUMP, MAX_DROP, algo)
		local time_end = core.get_us_time()
		local time_diff = time_end - time_start
		str = ""
		if not path then
			core.chat_send_player(player:get_player_name(), S("No path!"))
			core.chat_send_player(player:get_player_name(), S("Time: @1 ms", time_diff/1000))
			return
		end
		for s=1, #path do
			str = str .. core.pos_to_string(path[s]) .. "\n"
			local t
			if s == #path then
				t = "testpathfinder_waypoint_end.png"
			elseif s == 1 then
				t = "testpathfinder_waypoint_start.png"
			else
				local c = math.floor(((#path-s)/#path)*255)
				t = string.format("testpathfinder_waypoint.png^[multiply:#%02x%02x00", 0xFF-c, c)
			end
			core.add_particle({
				pos = path[s],
				expirationtime = 5 + 0.2 * s,
				playername = player:get_player_name(),
				glow = core.LIGHT_MAX,
				texture = t,
				size = 3,
			})
		end
		core.chat_send_player(player:get_player_name(), str)
		core.chat_send_player(player:get_player_name(), S("Path length: @1", #path))
		core.chat_send_player(player:get_player_name(), S("Time: @1 ms", time_diff/1000))
	end
end

local function set_destination(itemstack, user, pointed_thing)
	if not (user and user:is_player()) then
		return
	end
	local name = user:get_player_name()
	local obj
	local meta = itemstack:get_meta()
	if pointed_thing.type == "node" then
		local pos = pointed_thing.above
		meta:set_int("pos_x", pos.x)
		meta:set_int("pos_y", pos.y)
		meta:set_int("pos_z", pos.z)
		core.chat_send_player(user:get_player_name(), S("Destination set to @1", core.pos_to_string(pos)))
		return itemstack
	end
end

local function find_path_or_set_algorithm(itemstack, user, pointed_thing)
	if not (user and user:is_player()) then
		return
	end
	local ctrl = user:get_player_control()
	-- No sneak: Find path
	if not ctrl.sneak then
		find_path_for_player(user, itemstack)
	else
	-- Sneak: Set algorithm
		local meta = itemstack:get_meta()
		local algo = meta:get_int("algorithm")
		algo = (algo + 1) % #algorithms
		meta:set_int("algorithm", algo)
		core.chat_send_player(user:get_player_name(), S("Algorithm: @1", algorithms[algo+1]))
		return itemstack
	end
end

-- Punch: Find path
-- Sneak+punch: Select pathfinding algorithm
-- Place: Select destination node
core.register_tool("testpathfinder:testpathfinder", {
	description = S("Pathfinder Tester") .."\n"..
		S("Finds path between 2 points") .."\n"..
		S("Place on node: Select destination") .."\n"..
		S("Punch: Find path from here") .."\n"..
		S("Sneak+Punch: Change algorithm"),
	inventory_image = "testpathfinder_testpathfinder.png",
	groups = { testtool = 1, disable_repair = 1 },
	on_use = find_path_or_set_algorithm,
	on_secondary_use = set_destination,
	on_place = set_destination,
})


