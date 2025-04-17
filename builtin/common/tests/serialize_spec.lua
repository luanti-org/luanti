_G.core = {}

_G.setfenv = require 'busted.compatibility'.setfenv

dofile("builtin/common/serialize.lua")
dofile("builtin/common/vector.lua")
dofile("builtin/common/metatable.lua")

-- Supports circular tables; does not support table keys
-- Correctly checks whether a mapping of references ("same") exists
-- Is significantly more efficient than assert.same
local function assert_same(a, b, same)
	same = same or {}
	if same[a] or same[b] then
		assert(same[a] == b and same[b] == a)
		return
	end
	if a == b then
		return
	end
	if type(a) ~= "table" or type(b) ~= "table" then
		assert(a == b)
		return
	end
	same[a] = b
	same[b] = a
	local count = 0
	for k, v in pairs(a) do
		count = count + 1
		assert(type(k) ~= "table")
		assert_same(v, b[k], same)
	end
	for _ in pairs(b) do
		count = count - 1
	end
	assert(count == 0)
end

local x, y = {}, {}
local t1, t2 = {x, x, y, y}, {x, y, x, y}
assert.same(t1, t2) -- will succeed because it only checks whether the depths match
assert(not pcall(assert_same, t1, t2)) -- will correctly fail because it checks whether the refs match

local pair_mt = {
	__eq = function(x, y)
		return x[1] == y[1] and x[2] == y[2]
	end,
}
local function pair(x, y)
	return setmetatable({x, y}, pair_mt)
end
-- Use our own serialization functions to avoid incorrectly passing test related to references.
core.register_portable_metatable("pair", pair_mt)
assert.equals(pair(1, 2), pair(1, 2))
assert.not_equals(pair(1, 2), pair(3, 4))

describe("serialize", function()
	local function assert_preserves(value)
		local preserved_value = core.deserialize(core.serialize(value))
		assert_same(value, preserved_value)
	end
	local function assert_strictly_preserves(value)
		local preserved_value = core.deserialize(core.serialize(value))
		assert.equals(value, preserved_value)
	end
	local function assert_compatibly_preserves(value)
		local preserved_value = loadstring(core.serialize(value))()
		assert_same(value, preserved_value)
	end
	it("works", function()
		assert_preserves({cat={sound="nyan", speed=400}, dog={sound="woof"}})
	end)

	it("handles characters", function()
		assert_preserves({escape_chars="\n\r\t\v\\\"\'", non_european="θשׁ٩∂"})
	end)

	it("handles nil", function()
		assert_strictly_preserves(nil)
	end)

	it("handles NaN & infinities", function()
		local nan = core.deserialize(core.serialize(0/0))
		assert(nan ~= nan)
		assert_preserves(math.huge)
		assert_preserves(-math.huge)
	end)

	it("handles precise numbers", function()
		assert_preserves(0.2695949158945771)
	end)

	it("handles big integers", function()
		assert_preserves(269594915894577)
	end)

	it("handles recursive structures", function()
		local test_in = { hello = "world" }
		test_in.foo = test_in
		assert_preserves(test_in)
	end)

	it("handles cross-referencing structures", function()
		local test_in = {
			foo = {
				baz = {
					{}
				},
			},
			bar = {
				baz = {},
			},
		}

		test_in.foo.baz[1].foo = test_in.foo
		test_in.foo.baz[1].bar = test_in.bar
		test_in.bar.baz[1] = test_in.foo.baz[1]

		assert_preserves(test_in)
	end)

	it("strips functions in safe mode", function()
		local test_in = {
			func = function(a, b)
				error("test")
			end,
			foo = "bar"
		}
		setfenv(test_in.func, _G)

		local str = core.serialize(test_in)
		assert.not_nil(str:find("loadstring"))

		local test_out = core.deserialize(str, true)
		assert.is_nil(test_out.func)
		assert.equals(test_out.foo, "bar")
	end)

	it("vectors work", function()
		local v = vector.new(1, 2, 3)
		assert_preserves({v})
		assert_compatibly_preserves({v})
		assert_strictly_preserves(v)
		assert_compatibly_preserves(v)
		assert(core.deserialize(core.serialize(v)):check())

		-- abuse
		v = vector.new(1, 2, 3)
		v.a = "bla"
		assert_preserves(v)
	end)

	it("correctly handles typed objects with multiple references", function()
		local x, y = pair(1, 2), pair(1, 2)
		local t = core.deserialize(core.serialize{x, x, y})
		assert.equals(x, t[1])
		assert.equals(x, t[3])
		assert(rawequal(t[1], t[2]))
		assert(not rawequal(t[1], t[3]))
	end)

	it("correctly handles recursive typed objects with the identity function as serializer", function()
		local mt = {
			__eq = function(x, y)
				return x[1] == y[1]
			end,
		}
		core.register_portable_metatable("test_recursive_typed", mt)
		local t = setmetatable({1}, mt)
		t[2] = t
		assert_strictly_preserves(t)
	end)

	it("correctly handles binary trees", function()
		local child = {pair(1, 1)}
		local layers = 4
		for i = 2, layers do
			child[i] = pair(child[i-1], child[i-1])
		end
		local tree = child[layers]
		assert_strictly_preserves(tree)
		local node = core.deserialize(core.serialize(tree))
		for i = 2, layers do
			assert(rawequal(node[1], node[2]))
			node = node[1]
		end
		assert_compatibly_preserves(tree)
	end)

	it("handles keywords as keys", function()
		assert_preserves({["and"] = "keyword", ["for"] = "keyword"})
	end)

	describe("fuzzing", function()
		local atomics = {true, false, math.huge, -math.huge} -- no NaN or nil
		local function atomic()
			return atomics[math.random(1, #atomics)]
		end
		local function num()
			local sign = math.random() < 0.5 and -1 or 1
			-- HACK math.random(a, b) requires a, b & b - a to fit within a 32-bit int
			-- Use two random calls to generate a random number from 0 - 2^50 as lower & upper 25 bits
			local val = math.random(0, 2^25) * 2^25 + math.random(0, 2^25 - 1)
			local exp = math.random() < 0.5 and 1 or 2^(math.random(-120, 120))
			return sign * val * exp
		end
		local function charcodes(count)
			if count == 0 then return end
			return math.random(0, 0xFF), charcodes(count - 1)
		end
		local function str()
			return string.char(charcodes(math.random(0, 100)))
		end
		local primitives = {atomic, num, str}
		local function primitive()
			return primitives[math.random(1, #primitives)]()
		end
		local function tab(max_actions)
			local root = {}
			local tables = {root}
			local function random_table()
				return tables[math.random(1, #tables)]
			end
			for _ = 1, math.random(1, max_actions) do
				local tab = random_table()
				local value
				if math.random() < 0.5 then
					if math.random() < 0.5 then
						value = random_table()
					else
						value = {}
						table.insert(tables, value)
					end
				else
					value = primitive()
				end
				tab[math.random() < 0.5 and (#tab + 1) or primitive()] = value
			end
			return root
		end
		it("primitives work", function()
			for _ = 1, 1e3 do
				assert_preserves(primitive())
			end
		end)
		it("tables work", function()
			for _ = 1, 100 do
				local fuzzed_table = tab(1e3)
				assert_same(fuzzed_table, table.copy(fuzzed_table))
				assert_preserves(fuzzed_table)
			end
		end)
	end)
end)
