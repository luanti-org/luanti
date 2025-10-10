-- The distinction of what goes here is a bit tricky, basically it's everything
-- that does not (directly or indirectly) need access to ServerEnvironment,
-- Server or writable access to IGameDef on the engine side.
-- (The '_s' stands for standalone.)

--
-- Misc. API functions
--

-- This must match the implementation in src/script/common/c_converter.h
function core.hash_node_position(pos)
	return (pos.z + 0x8000) * 0x100000000 + (pos.y + 0x8000) * 0x10000 + (pos.x + 0x8000)
end


function core.get_position_from_hash(hash)
	local x = (hash % 65536) - 32768
	hash  = math.floor(hash / 65536)
	local y = (hash % 65536) - 32768
	hash  = math.floor(hash / 65536)
	local z = (hash % 65536) - 32768
	return vector.new(x, y, z)
end


function core.get_item_group(name, group)
	local def = core.registered_items[name]
	return def and def.groups[group] or 0
end


function core.get_node_group(name, group)
	core.log("deprecated", "Deprecated usage of get_node_group, use get_item_group instead")
	return core.get_item_group(name, group)
end


function core.setting_get_pos(name)
	return core.settings:get_pos(name)
end


-- See l_env.cpp for the other functions
function core.get_artificial_light(param1)
	return math.floor(param1 / 16)
end

-- PNG encoder safety wrapper

local o_encode_png = core.encode_png
function core.encode_png(width, height, data, compression)
	if type(width) ~= "number" then
		error("Incorrect type for 'width', expected number, got " .. type(width))
	end
	if type(height) ~= "number" then
		error("Incorrect type for 'height', expected number, got " .. type(height))
	end

	if width < 1 then
		error("Incorrect value for 'width', must be at least 1")
	end
	if height < 1 then
		error("Incorrect value for 'height', must be at least 1")
	end

	local expected_byte_count = width * height * 4

	if type(data) ~= "table" and type(data) ~= "string" then
		error("Incorrect type for 'data', expected table or string, got " .. type(data))
	end

	local data_length = type(data) == "table" and #data * 4 or string.len(data)

	if data_length ~= expected_byte_count then
		error(string.format(
			"Incorrect length of 'data', width and height imply %d bytes but %d were provided",
			expected_byte_count,
			data_length
		))
	end

	if type(data) == "table" then
		local dataBuf = {}
		for i = 1, #data do
			dataBuf[i] = core.colorspec_to_bytes(data[i])
		end
		data = table.concat(dataBuf)
	end

	return o_encode_png(width, height, data, compression or 6)
end

-- Helper that pushes a collisionMoveResult structure
if core.set_push_moveresult1 then
	-- must match CollisionAxis in collision.h
	local AXES = {"x", "y", "z"}
	-- <=> script/common/c_content.cpp push_collision_move_result()
	core.set_push_moveresult1(function(b0, b1, b2, axis, npx, npy, npz, v0x, v0y, v0z, v1x, v1y, v1z, v2x, v2y, v2z)
		return {
			touching_ground = b0,
			collides = b1,
			standing_on_object = b2,
			collisions = {{
				type = "node",
				axis = AXES[axis + 1],
				node_pos = vector.new(npx, npy, npz),
				new_pos = vector.new(v0x, v0y, v0z),
				old_velocity = vector.new(v1x, v1y, v1z),
				new_velocity = vector.new(v2x, v2y, v2z),
			}},
		}
	end)
	core.set_push_moveresult1 = nil
end

-- Protocol version table
-- see also src/network/networkprotocol.cpp
core.protocol_versions = {
	["5.0.0"] = 37,
	["5.1.0"] = 38,
	["5.2.0"] = 39,
	["5.3.0"] = 39,
	["5.4.0"] = 39,
	["5.5.0"] = 40,
	["5.6.0"] = 41,
	["5.7.0"] = 42,
	["5.8.0"] = 43,
	["5.9.0"] = 44,
	["5.9.1"] = 45,
	["5.10.0"] = 46,
	["5.11.0"] = 47,
	["5.12.0"] = 48,
	["5.13.0"] = 49,
	["5.14.0"] = 50,
}

setmetatable(core.protocol_versions, {__newindex = function()
	error("core.protocol_versions is read-only")
end})
