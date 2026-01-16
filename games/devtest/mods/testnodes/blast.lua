local S = core.get_translator("testnodes")

local TNT_RADIUS = 6
local TNT_INTENSITY = 1.0
local SUPER_TNT_INTENSITY = 5.0

core.register_node("testnodes:on_blast", {
	description = S("Blast Detector") .. "\n" ..
			S("Prints message in chat when its on_blast callback was called"),
	tiles = {"testnodes_blast_detector.png"},
	on_blast = function(pos, intensity)
		core.chat_send_all(
			S("Blast Detector at @1 detects a blast of intensity @2!",
			core.pos_to_string(pos), tostring(intensity)))
	end,
	walkable = false,


	groups = {dig_immediate=3},
})

local get_nodes_in_radius = function(pos, radius)
	local nodes = {}
	for x=-radius, radius do
	for y=-radius, radius do
	for z=-radius, radius do
		local offset = vector.new(x, y, z)
		local checkpos = vector.round(vector.add(pos, offset))
		local dist = vector.distance(pos, checkpos)
		if dist <= radius then
			table.insert(nodes, checkpos)
		end
	end
	end
	end
	return nodes
end

-- Helper function to calculate actual intensity for on_blast.
-- Blast intensity is max. in the center and is
-- reduced linearily with distance from the center.
-- Points outside the blast radius will have intensity 0.

-- distance_from_center: Distance from the center of the blast
-- blast_radius: Full radius of the blast
-- intensity_at_center: Blast intensity in the center
local function calculate_intensity(distance_from_center, blast_radius, intensity_at_center)
	-- PLEASE NOTE: This is only one way to calculate
	-- blast intensities. Actual explosion mods
	-- may choose to use a different method for calculating
	-- intensity, which is a valid choice, as long the
	-- rules in the lua_api.md documentation of on_blast
	-- are followed.
	local strength = blast_radius - distance_from_center
	local intensity = (strength / blast_radius) * intensity_at_center
	intensity = math.max(0, intensity)
	return intensity
end

-- Call on_blast of all nodes within the radius of TNT_RADIUS
-- pos: Center of simulated blast
-- intensity_at_center: Blast intensity in center
local function do_blast(pos, intensity_at_center)
	local nodes = get_nodes_in_radius(pos, TNT_RADIUS)
	for n=1, #nodes do
		local npos = nodes[n]
		local nnode = core.get_node(npos)
		local ndef = core.registered_nodes[nnode.name]
		if ndef and ndef.on_blast then
			local distance = vector.distance(pos, npos)
			local intensity = calculate_intensity(distance, TNT_RADIUS, intensity_at_center)
			if intensity > 0 then
				ndef.on_blast(npos, intensity)
			end
		end
	end
end

core.register_node("testnodes:tnt", {
	description = S("TNT") .. "\n" ..
			S("Punch to call on_blast of all nodes within reach") .. "\n" ..
			S("Radius: @1", TNT_RADIUS) .. "\n" ..
			S("Intensity: @1", TNT_INTENSITY),
	tiles = {
		"testnodes_tnt_top.png",
		"testnodes_tnt_bottom.png",
		"testnodes_tnt_side.png",
	},
	on_punch = function(pos)
		core.chat_send_all(
			S("TNT at @1 simulates a blast of intensity @2.",
			core.pos_to_string(pos), TNT_INTENSITY))
		do_blast(pos, TNT_INTENSITY)
	end,
	on_blast = function(pos, intensity)
		core.chat_send_all(
			S("TNT at @1 detects a blast of intensity @2!",
			core.pos_to_string(pos), intensity))
	end,


	groups = {dig_immediate=2},
})

-- Same as TNT, but higher intensity
core.register_node("testnodes:super_tnt", {
	description = S("Super TNT") .. "\n" ..
			S("Punch to call on_blast of all nodes within reach") .. "\n" ..
			S("Radius: @1", TNT_RADIUS) .. "\n" ..
			S("Intensity: @1", SUPER_TNT_INTENSITY),
	tiles = {
		"testnodes_tnt_top.png^[colorize:#ff00ff:127",
		"testnodes_tnt_bottom.png^[colorize:#ff00ff:127",
		"testnodes_tnt_side.png^[colorize:#ff00ff:127",
	},
	on_punch = function(pos)
		core.chat_send_all(
			S("Super TNT at @1 simulates a blast of intensity @2.",
			core.pos_to_string(pos), SUPER_TNT_INTENSITY))
		do_blast(pos, SUPER_TNT_INTENSITY)
	end,
	on_blast = function(pos, intensity)
		core.chat_send_all(
			S("Super TNT at @1 detects a blast of intensity @2!",
			core.pos_to_string(pos), intensity))
	end,


	groups = {dig_immediate=2},
})


-- The TNT stick is simply a mobile variant of TNT.
-- It intentionally has the same intensity as TNT to keep things simple.
core.register_craftitem("testnodes:tnt_stick", {
	description = S("TNT Stick") .. "\n" ..
			S("Punch: Call on_blast of all nodes within reach") .. "\n" ..
			S("Radius: @1", TNT_RADIUS) .. "\n" ..
			S("Intensity: @1", TNT_INTENSITY),

	inventory_image = "testnodes_tnt_stick.png",
	on_use = function(itemstack, user)
		if not user then
			return
		end
		local pos = user:get_pos()
		core.chat_send_all(
			S("TNT Stick at @1 simulates a blast of intensity @2.",
			core.pos_to_string(pos, 1), TNT_INTENSITY))
		do_blast(pos, TNT_INTENSITY)
	end,


	groups = {dig_immediate=2},
})


