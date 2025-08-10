-- Safeguard against too much optimization. This way the results cannot be optimized
-- away, but they can be garbage collected (due to __mode = "k").
_G._bench_content_ids_data = setmetatable({}, {__mode = "k"})

local function bench_name2content()
	local t = {}
	_G._bench_content_ids_data[t] = true

	local get_content_id = core.get_content_id

	local start = core.get_us_time()

	for i = 1, 200 do
		for name in pairs(core.registered_nodes) do
			t[#t + 1] = get_content_id(name)
		end
	end

	local finish = core.get_us_time()

	return (finish - start) / 1000
end

local function bench_content2name()
	local t = {}
	_G._bench_content_ids_data[t] = true

	-- Try to estimate the highest content ID that's used
	-- (not accurate but good enough for this test)
	local n = 0
	for _ in pairs(core.registered_nodes) do
		n = n + 1
	end

	local get_name_from_content_id = core.get_name_from_content_id

	local start = core.get_us_time()

	for i = 1, 200 do
		for j = 0, n do
			t[#t + 1] = get_name_from_content_id(j)
		end
	end

	local finish = core.get_us_time()

	return (finish - start) / 1000
end

core.register_chatcommand("bench_name2content", {
	params = "",
	description = "Benchmark: Conversion from node names to content IDs",
	func = function(name, param)
		core.chat_send_player(name, "Benchmarking core.get_content_id. Warming up ...")
		bench_name2content()
		core.chat_send_player(name, "Warming up finished, now benchmarking ...")
		local time = bench_name2content()
		return true, ("Time: %.2f ms"):format(time)
	end,
})

core.register_chatcommand("bench_content2name", {
	params = "",
	description = "Benchmark: Conversion from content IDs to node names",
	func = function(name, param)
		core.chat_send_player(name, "Benchmarking core.get_name_from_content_id. Warming up ...")
		bench_content2name()
		core.chat_send_player(name, "Warming up finished, now benchmarking ...")
		local time = bench_content2name()
		return true, ("Time: %.2f ms"):format(time)
	end,
})

local function get_positions_cube(ppos)
	local pos_list = {}

	local i = 1
	for x=2,100 do
		for y=2,100 do
			for z=2,100 do
				pos_list[i] = ppos:offset(x, y, z)
				i = i + 1
			end
		end
	end

	return pos_list
end

core.register_chatcommand("bench_bulk_set_node", {
	params = "",
	description = "Benchmark: Bulk-set 99×99×99 stone nodes",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local pos_list = get_positions_cube(player:get_pos())

		core.chat_send_player(name, "Benchmarking core.bulk_set_node. Warming up ...")

		-- warm up with stone to prevent having different callbacks
		-- due to different node topology
		core.bulk_set_node(pos_list, {name = "mapgen_stone"})

		core.chat_send_player(name, "Warming up finished, now benchmarking ...")

		local start_time = core.get_us_time()
		for i = 1, #pos_list do
			core.set_node(pos_list[i], {name = "mapgen_stone"})
		end
		local middle_time = core.get_us_time()
		core.bulk_set_node(pos_list, {name = "mapgen_stone"})
		local end_time = core.get_us_time()
		local msg = string.format("Benchmark results: core.set_node loop: %.2f ms; core.bulk_set_node: %.2f ms",
			((middle_time - start_time)) / 1000,
			((end_time - middle_time)) / 1000
		)
		print(msg)
		return true, msg
	end,
})

core.register_chatcommand("bench_bulk_get_node", {
	params = "",
	description = "Benchmark: Bulk-get 99×99×99 nodes",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local pos_list = get_positions_cube(player:get_pos())
		local dummy = 0
		local function bench()
			local start_time = core.get_us_time()
			for i = 1, #pos_list do
				local n = core.get_node(pos_list[i])
				-- Make sure the name lookup is not optimized away (can this even happen?)
				dummy = dummy + #n.name
			end
			return core.get_us_time() - start_time
		end

		core.chat_send_player(name, "Benchmarking core.get_node. Warming up ...")
		bench()

		core.chat_send_player(name, "Warming up finished, now benchmarking ...")
		local result_us = bench()

		local msg = string.format("Benchmark results: core.get_node loop 1: %.2f ms",
				result_us / 1000)
		print(msg)
		return true, msg
	end,
})

core.register_chatcommand("bench_bulk_get_node_raw", {
	params = "",
	description = "Benchmark: Bulk-get 99×99×99 nodes with raw API",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local pos_list = get_positions_cube(player:get_pos())
		local dummy = 0
		local function bench()
			local start_time = core.get_us_time()
			for i = 1, #pos_list do
				local pos_i = pos_list[i]
				local nid = core.get_node_raw(pos_i.x, pos_i.y, pos_i.z)
				-- Make sure the result is not optimized away
				dummy = dummy + nid
			end
			return core.get_us_time() - start_time
		end

		core.chat_send_player(name, "Benchmarking core.get_node_raw. Warming up ...")
		bench()

		core.chat_send_player(name, "Warming up finished, now benchmarking ...")
		local result_us = bench()

		local msg = string.format("Benchmark results: core.get_node_raw loop 1: %.2f ms",
				result_us / 1000)
		print(msg)
		return true, msg
	end,
})

core.register_chatcommand("bench_bulk_get_node_raw2", {
	params = "",
	description = "Benchmark: Bulk-get 99×99×99 nodes with raw API and lookup names",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local pos_list = get_positions_cube(player:get_pos())
		local dummy = 0
		local function bench()
			local start_time = core.get_us_time()
			for i = 1, #pos_list do
				local pos_i = pos_list[i]
				local nid = core.get_node_raw(pos_i.x, pos_i.y, pos_i.z)
				local name = core.get_name_from_content_id(nid)
				-- Make sure the name lookup is not optimized away
				dummy = dummy + #name
			end
			return core.get_us_time() - start_time
		end

		core.chat_send_player(name, "Benchmarking core.get_node_raw+get_name_from_content_id. Warming up ...")
		bench()

		core.chat_send_player(name, "Warming up finished, now benchmarking ...")
		local result_us = bench()

		local msg = string.format("Benchmark results: core.get_node_raw+get_name_from_content_id loop 1: %.2f ms",
				result_us / 1000)
		print(msg)
		return true, msg
	end,
})

core.register_chatcommand("bench_bulk_get_node_vm", {
	params = "",
	description = "Benchmark: Bulk-get 99×99×99 nodes with voxel manipulator",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local pos = player:get_pos()
		local dummy = 0
		local function bench()
			local start_time = core.get_us_time()
			local vm = core.get_voxel_manip(pos:offset(1,1,1), pos:offset(100,100,100))
			local data = vm:get_data()
			local mid_time = core.get_us_time()
			-- Note that the VManip will actually retrieve more than just the 100³ nodes
			-- and also we don't need to iterate pos_list here, so it's not an entirely
			-- fair comparison.
			for i = 1, 99*99*99 do
				local nid = data[i]
				-- Make sure the table lookup is not optimized away
				dummy = dummy + nid
			end
			return core.get_us_time() - start_time, mid_time - start_time
		end

		core.chat_send_player(name, "Benchmarking core.get_voxel_vmanip+get_data+loop . Warming up ...")
		bench()

		core.chat_send_player(name, "Warming up finished, now benchmarking ...")
		local result_us, get_data_us = bench()

		local msg = string.format("Benchmark results: core.get_voxel_vmanip+get_data+loop loop 1: %.2f ms of which get_data() %.2f ms",
				result_us / 1000, get_data_us / 1000)
		print(msg)
		return true, msg
	end,
})

core.register_chatcommand("bench_bulk_swap_node", {
	params = "",
	description = "Benchmark: Bulk-swap 99×99×99 stone nodes",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local pos_list = get_positions_cube(player:get_pos())

		core.chat_send_player(name, "Benchmarking core.bulk_swap_node. Warming up ...")

		-- warm up because first execution otherwise becomes
		-- significantly slower
		core.bulk_swap_node(pos_list, {name = "mapgen_stone"})

		core.chat_send_player(name, "Warming up finished, now benchmarking ...")

		local start_time = core.get_us_time()
		for i = 1, #pos_list do
			core.swap_node(pos_list[i], {name = "mapgen_stone"})
		end
		local middle_time = core.get_us_time()
		core.bulk_swap_node(pos_list, {name = "mapgen_stone"})
		local end_time = core.get_us_time()
		local msg = string.format("Benchmark results: core.swap_node loop: %.2f ms; core.bulk_swap_node: %.2f ms",
			((middle_time - start_time)) / 1000,
			((end_time - middle_time)) / 1000
		)
		print(msg)
		return true, msg
	end,
})
