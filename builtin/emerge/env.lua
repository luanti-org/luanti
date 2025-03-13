-- Reimplementations of some environment function on vmanips, since this is
-- what the emerge environment operates on

-- core.vmanip = <VoxelManip> -- set by C++

function core.set_node(pos, node)
	return core.vmanip:set_node_at(pos, node)
end

function core.bulk_set_node(pos_list, node)
	local vm = core.vmanip
	local set_node_at = vm.set_node_at
	for _, pos in ipairs(pos_list) do
		if not set_node_at(vm, pos, node) then
			return false
		end
	end
	return true
end

core.add_node = core.set_node

-- we don't deal with metadata currently
core.swap_node = core.set_node

core.bulk_swap_node = core.bulk_set_node

function core.remove_node(pos)
	return core.vmanip:set_node_at(pos, {name="air"})
end

function core.get_node(pos)
	return core.vmanip:get_node_at(pos)
end

function core.get_perlin(seed, octaves, persist, spread)
	local params
	if type(seed) == "table" then
		params = table.copy(seed)
	else
		assert(type(seed) == "number")
		params = {
			seed = seed,
			octaves = octaves,
			persist = persist,
			spread = {x=spread, y=spread, z=spread},
		}
	end
	params.seed = core.get_seed(params.seed) -- add mapgen seed
	return PerlinNoise(params)
end


function core.get_perlin_map(params, size)
	local params2 = table.copy(params)
	params2.seed = core.get_seed(params.seed) -- add mapgen seed
	return PerlinNoiseMap(params2, size)
end
