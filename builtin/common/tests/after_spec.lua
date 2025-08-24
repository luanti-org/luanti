_G.core = {}
_G.vector = {metatable = {}}

dofile("builtin/common/math.lua")
dofile("builtin/common/vector.lua")
dofile("builtin/common/misc_helpers.lua")

function core.get_last_run_mod() return "*test*" end
function core.set_last_run_mod() end

local do_step
function core.register_globalstep(func)
	do_step = func
end

dofile("builtin/common/after.lua")

describe("after", function()
	it("executes callbacks when expected", function()
		local result = ""
		core.after(0, function()
			result = result .. "a"
		end)
		core.after(1, function()
			result = result .. "b"
		end)
		core.after(1, function()
			result = result .. "c"
		end)
		core.after(2, function()
			result = result .. "d"
		end)
		local cancel = core.after(2, function()
			result = result .. "e"
		end)
		do_step(0)
		assert.same("a", result)

		do_step(1)
		assert.same("abc", result)

		core.after(2, function()
			result = result .. "f"
		end)
		core.after(1, function()
			result = result .. "g"
		end)
		core.after(-1, function()
			result = result .. "h"
		end)
		cancel:cancel()
		do_step(1)
		assert.same("abchdg", result)

		do_step(1)
		assert.same("abchdgf", result)
	end)

	it("defers jobs with delay 0", function()
		local result = ""
		core.after(0, function()
			core.after(0, function()
				result = result .. "b"
			end)
			result = result .. "a"
		end)
		do_step(1)
		assert.same("a", result)
		do_step(1)
		assert.same("ab", result)
	end)

	it("passes arguments", function()
		core.after(0, function(...)
			assert.same(0, select("#", ...))
		end)
		core.after(0, function(...)
			assert.same(4, select("#", ...))
			assert.same(1, (select(1, ...)))
			assert.same(nil, (select(2, ...)))
			assert.same("a", (select(3, ...)))
			assert.same(nil, (select(4, ...)))
		end, 1, nil, "a", nil)
		do_step(0)
	end)

	it("rejects invalid arguments", function()
		assert.has.errors(function() core.after() end)
		assert.has.errors(function() core.after(nil, nil) end)
		assert.has.errors(function() core.after(0) end)
		assert.has.errors(function() core.after(0, nil) end)
		assert.has.errors(function() core.after(nil, function() end) end)
		assert.has.errors(function() core.after(0 / 0, function() end) end)
	end)

	-- Make sure that the underlying heap is working correctly
	it("can be abused as a heapsort", function()
		local t = {}
		for i = 1, 1000 do
			t[i] = math.random(100)
		end
		local sorted = table.copy(t)
		table.sort(sorted)
		local i = 0
		for _, v in ipairs(t) do
			core.after(v, function()
				i = i + 1
				assert.equal(v, sorted[i])
			end)
		end
		do_step(math.max(unpack(t)))
		assert.equal(#t, i)
	end)

	it("is in time interval, according to core.get_global_time()", function()
		do_step(0.1 + math.random() * 0.001)

		local num_executed = 0
		local t0 = core.get_global_time()

		for i = 0, 2.00001, 0.05 do
			core.after(i, function()
				local actual_time = core.get_global_time() - t0
				assert(actual_time >= i)
				if i >= 0.1 then
					assert(actual_time < i + 0.1)
				end
				num_executed = num_executed + 1
			end)
		end

		for i = 1, 25 do
			do_step(0.1 + math.random() * 0.001)
		end

		assert(core.get_global_time() - t0 >= 25 * 0.1)
		assert(core.get_global_time() - t0 < 26 * 0.1)

		assert.equal(num_executed, 41)
	end)
end)
