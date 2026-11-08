local pos = {x = 16, y = 32, z = -16}

local function fixture()
	local state = {loads = 0, frees = 0, writes = {}, jobs = {}, write_ok = true}
	_G.core = {
		MAP_BLOCKSIZE = 16,
		settings = {get = function() return "16" end},
		get_worldpath = function() return "unused-forceload-fixture" end,
		hash_node_position = function(p) return p.x .. "," .. p.y .. "," .. p.z end,
		forceload_block = function() state.loads = state.loads + 1 end,
		forceload_free_block = function() state.frees = state.frees + 1 end,
		serialize = function(t)
			local result = {}
			for k, v in pairs(t) do result[k] = v end
			state.serialized = result
			return "fixture"
		end,
		safe_file_write = function()
			state.writes[#state.writes + 1] = state.serialized
			return state.write_ok
		end,
		after = function(_, fn) state.jobs[#state.jobs + 1] = fn end,
		register_on_shutdown = function(fn) state.shutdown = fn end,
	}
	local original_open = io.open
	io.open = function() return nil end
	local ok, err = pcall(dofile, "builtin/game/forceloading.lua")
	io.open = original_open
	assert(ok, err)
	function state.persist()
		state.jobs[#state.jobs]()
	end
	return state
end

describe("forceloading", function()
	for _, transient_first in ipairs({true, false}) do
		it("returns true for mixed references, transient first: " .. tostring(transient_first), function()
			local state = fixture()
			assert.is_true(core.forceload_block(pos, transient_first))
			assert.is_true(core.forceload_block(pos, not transient_first))
			assert.equal(1, state.loads)
			core.forceload_free_block(pos, transient_first)
			assert.equal(0, state.frees)
			core.forceload_free_block(pos, not transient_first)
			assert.equal(1, state.frees)
		end)
	end

	it("retains the block until the last reference is freed", function()
		local state = fixture()
		assert.is_true(core.forceload_block(pos))
		assert.is_true(core.forceload_block(pos))
		assert.is_true(core.forceload_block(pos, true))
		core.forceload_free_block(pos)
		core.forceload_free_block(pos)
		assert.equal(0, state.frees)
		core.forceload_free_block(pos, true)
		assert.equal(1, state.frees)
	end)

	it("does not dirty persistent state for transient changes", function()
		local state = fixture()
		core.forceload_block(pos, true)
		state.persist()
		core.forceload_free_block(pos, true)
		state.persist()
		assert.equal(0, #state.writes)
	end)

	it("does not dirty persistent state for rejected or missing references", function()
		local state = fixture()
		assert.is_false(core.forceload_block(pos, false, 0))
		core.forceload_free_block(pos)
		state.persist()
		assert.equal(0, #state.writes)
	end)

	it("persists reference counts and clears the dirty flag after success", function()
		local state = fixture()
		core.forceload_block(pos)
		core.forceload_block(pos)
		state.persist()
		assert.same({["1,2,-1"] = 2}, state.writes[1])
		state.persist()
		assert.equal(1, #state.writes)
		core.forceload_free_block(pos)
		state.persist()
		assert.same({["1,2,-1"] = 1}, state.writes[2])
		core.forceload_free_block(pos)
		state.persist()
		assert.same({}, state.writes[3])
	end)

	it("retries a failed write without another reference change", function()
		local state = fixture()
		state.write_ok = false
		core.forceload_block(pos)
		state.persist()
		state.persist()
		assert.equal(2, #state.writes)
		state.write_ok = true
		state.persist()
		state.persist()
		assert.equal(3, #state.writes)
		assert.same({["1,2,-1"] = 1}, state.writes[3])
	end)

	it("keeps transient references out of shutdown persistence", function()
		local state = fixture()
		core.forceload_block(pos, true)
		core.forceload_block({x = 32, y = 32, z = -16})
		state.shutdown()
		assert.same({["2,2,-1"] = 1}, state.writes[1])
	end)
end)
