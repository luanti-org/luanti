_G.core = {}
dofile("builtin/common/math.lua")
dofile("builtin/common/vector.lua")
dofile("builtin/common/misc_helpers.lua")

describe("string", function()
	it("trim()", function()
		assert.equal("foo bar", string.trim("\n \t\tfoo bar\t "))
	end)

	describe("split()", function()
		it("removes empty", function()
			assert.same({ "hello" }, string.split("hello"))
			assert.same({ "hello", "world" }, string.split("hello,world"))
			assert.same({ "hello", "world" }, string.split("hello,world,,,"))
			assert.same({ "hello", "world" }, string.split(",,,hello,world"))
			assert.same({ "hello", "world", "2" }, string.split("hello,,,world,2"))
			assert.same({ "hello ", " world" }, string.split("hello :| world", ":|"))
		end)

		it("keeps empty", function()
			assert.same({ "hello" }, string.split("hello", ",", true))
			assert.same({ "hello", "world" }, string.split("hello,world", ",", true))
			assert.same({ "hello", "world", "" }, string.split("hello,world,", ",", true))
			assert.same({ "hello", "", "", "world", "2" }, string.split("hello,,,world,2", ",", true))
			assert.same({ "", "", "hello", "world", "2" }, string.split(",,hello,world,2", ",", true))
			assert.same({ "hello ", " world | :" }, string.split("hello :| world | :", ":|"))
		end)

		it("max_splits", function()
			assert.same({ "one" }, string.split("one", ",", true, 2))
			assert.same({ "one,two,three,four" }, string.split("one,two,three,four", ",", true, 0))
			assert.same({ "one", "two", "three,four" }, string.split("one,two,three,four", ",", true, 2))
			assert.same({ "one", "", "two,three,four" }, string.split("one,,two,three,four", ",", true, 2))
			assert.same({ "one", "two", "three,four" }, string.split("one,,,,,,two,three,four", ",", false, 2))
		end)

		it("pattern", function()
			assert.same({ "one", "two" }, string.split("one,two", ",", false, -1, true))
			assert.same({ "one", "two", "three" }, string.split("one2two3three", "%d", false, -1, true))
		end)

		it("rejects empty separator", function()
			assert.has.errors(function()
				string.split("", "")
			end)
		end)
	end)
end)

describe("privs", function()
	it("from string", function()
		assert.same({ a = true, b = true }, core.string_to_privs("a,b"))
	end)

	it("to string", function()
		assert.equal("one", core.privs_to_string({ one=true }))

		local ret = core.privs_to_string({ a=true, b=true })
		assert(ret == "a,b" or ret == "b,a")
	end)
end)

describe("pos", function()
	it("from string", function()
		assert.equal(vector.new(10, 5.1, -2), core.string_to_pos("10.0, 5.1, -2"))
		assert.equal(vector.new(10, 5.1, -2), core.string_to_pos("( 10.0, 5.1, -2)"))
		assert.is_nil(core.string_to_pos("asd, 5, -2)"))
	end)

	it("to string", function()
		assert.equal("(10.1,5.2,-2.3)", core.pos_to_string({ x = 10.1, y = 5.2, z = -2.3}))
	end)
end)

describe("area parsing", function()
	describe("valid inputs", function()
		it("accepts absolute numbers", function()
			local p1, p2 = core.string_to_area("(10.0, 5, -2) (  30.2 4 -12.53)")
			assert(p1.x == 10 and p1.y == 5 and p1.z == -2)
			assert(p2.x == 30.2 and p2.y == 4 and p2.z == -12.53)
		end)

		it("accepts relative numbers", function()
			local p1, p2 = core.string_to_area("(1,2,3) (~5,~-5,~)", {x=10,y=10,z=10})
			assert(type(p1) == "table" and type(p2) == "table")
			assert(p1.x == 1 and p1.y == 2 and p1.z == 3)
			assert(p2.x == 15 and p2.y == 5 and p2.z == 10)

			p1, p2 = core.string_to_area("(1 2 3) (~5 ~-5 ~)", {x=10,y=10,z=10})
			assert(type(p1) == "table" and type(p2) == "table")
			assert(p1.x == 1 and p1.y == 2 and p1.z == 3)
			assert(p2.x == 15 and p2.y == 5 and p2.z == 10)
		end)
	end)
	describe("invalid inputs", function()
		it("rejects too few numbers", function()
			local p1, p2 = core.string_to_area("(1,1) (1,1,1,1)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)
		end)

		it("rejects too many numbers", function()
			local p1, p2 = core.string_to_area("(1,1,1,1) (1,1,1,1)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)
		end)

		it("rejects nan & inf", function()
			local p1, p2 = core.string_to_area("(1,1,1) (1,1,nan)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(1,1,1) (1,1,~nan)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(1,1,1) (1,~nan,1)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(1,1,1) (1,1,inf)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(1,1,1) (1,1,~inf)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(1,1,1) (1,~inf,1)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(nan,nan,nan) (nan,nan,nan)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(nan,nan,nan) (nan,nan,nan)")
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(inf,inf,inf) (-inf,-inf,-inf)", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(inf,inf,inf) (-inf,-inf,-inf)")
			assert(p1 == nil and p2 == nil)
		end)

		it("rejects words", function()
			local p1, p2 = core.string_to_area("bananas", {x=1,y=1,z=1})
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("bananas", "foobar")
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("bananas")
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(bananas,bananas,bananas)")
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(bananas,bananas,bananas) (bananas,bananas,bananas)")
			assert(p1 == nil and p2 == nil)
		end)

		it("requires parenthesis & valid numbers", function()
			local p1, p2 = core.string_to_area("(10.0, 5, -2  30.2,   4, -12.53")
			assert(p1 == nil and p2 == nil)

			p1, p2 = core.string_to_area("(10.0, 5,) -2  fgdf2,   4, -12.53")
			assert(p1 == nil and p2 == nil)
		end)
	end)
end)

describe("table", function()
	it("indexof()", function()
		assert.equal(1, table.indexof({"foo", "bar"}, "foo"))
		assert.equal(-1, table.indexof({"foo", "bar"}, "baz"))
		assert.equal(-1, table.indexof({[2] = "foo", [3] = "bar"}, "foo"))
		assert.equal(-1, table.indexof({[1] = "foo", [3] = "bar"}, "bar"))
	end)

	it("keyof()", function()
		assert.equal("a", table.keyof({a = "foo", b = "bar"}, "foo"))
		assert.equal(nil, table.keyof({a = "foo", b = "bar"}, "baz"))
		assert.equal(1, table.keyof({"foo", "bar"}, "foo"))
		assert.equal(2, table.keyof({[2] = "foo", [3] = "bar"}, "foo"))
		assert.equal(3, table.keyof({[1] = "foo", [3] = "bar"}, "bar"))
	end)

	describe("copy()", function()
		it("strips metatables", function()
			local v = vector.new(1, 2, 3)
			local w = table.copy(v)
			assert.are_not.equal(v, w)
			assert.same(v, w)
			assert.equal(nil, getmetatable(w))
		end)
		it("preserves referential structure", function()
			local t = {{}, {}}
			t[1][1] = t[2]
			t[2][1] = t[1]
			local copy = table.copy(t)
			assert.same(t, copy)
			assert.equal(copy[1][1], copy[2])
			assert.equal(copy[2][1], copy[1])
		end)
	end)

	describe("copy_with_metatables()", function()
		it("preserves metatables", function()
			local v = vector.new(1, 2, 3)
			local w = table.copy_with_metatables(v)
			assert.equal(getmetatable(v), getmetatable(w))
			assert(vector.check(w))
			assert.equal(v, w) -- vector overrides ==
		end)
	end)
end)

describe("formspec_escape", function()
	it("escapes", function()
		assert.equal(nil, core.formspec_escape(nil))
		assert.equal("", core.formspec_escape(""))
		assert.equal("\\[Hello\\\\\\[", core.formspec_escape("[Hello\\["))
	end)
end)

describe("math", function()
	it("round()", function()
		assert.equal(0, math.round(0))
		assert.equal(10, math.round(10.3))
		assert.equal(11, math.round(10.5))
		assert.equal(11, math.round(10.7))
		assert.equal(-10, math.round(-10.3))
		assert.equal(-11, math.round(-10.5))
		assert.equal(-11, math.round(-10.7))
		assert.equal(0, math.round(0.49999999999999994))
		assert.equal(0, math.round(-0.49999999999999994))
	end)
end)

describe("dump", function()
	local function test_expression(expr)
		local chunk = assert(loadstring("return " .. expr))
		local refs = {}
		setfenv(chunk, {
			setref = function(id)
				refs[id] = {}
				return function(fields)
					for k, v in pairs(fields) do
						refs[id][k] = v
					end
					return refs[id]
				end
			end,
			getref = function(id)
				return assert(refs[id])
			end,
		})
		assert.equal(expr, dump(chunk()))
	end

	it("nil", function()
		test_expression("nil")
	end)

	it("booleans", function()
		test_expression("false")
		test_expression("true")
	end)

	describe("numbers", function()
		it("formats integers nicely", function()
			test_expression("42")
		end)
		it("avoids misleading rounding", function()
			test_expression("0.3")
			assert.equal("0.30000000000000004", dump(0.1 + 0.2))
		end)
	end)

	it("strings", function()
		test_expression('"hello world"')
		test_expression([["hello \"world\""]])
	end)

	describe("tables", function()
		it("empty", function()
			test_expression("{}")
		end)

		it("lists", function()
			test_expression([[
{
	false,
	true,
	"foo",
	1,
	2,
}]])
		end)

		it("number keys", function()
test_expression([[
{
	[0.5] = false,
	[1.5] = true,
	[2.5] = "foo",
}]])
		end)

		it("dicts", function()
			test_expression([[{
	a = 1,
	b = 2,
	c = 3,
}]])
		end)

		it("mixed", function()
			test_expression([[{
	a = 1,
	b = 2,
	c = 3,
	["d e"] = true,
	"foo",
	"bar",
}]])
		end)

		it("nested", function()
test_expression([[{
	a = {
		1,
		{},
	},
	b = "foo",
	c = {
		[0.5] = 0.1,
		[1.5] = 0.2,
	},
}]])
		end)

		it("circular references", function()
test_expression([[setref(1){
	child = {
		parent = getref(1),
	},
	other_child = {
		parent = getref(1),
	},
}]])
		end)

		it("supports variable indent", function()
			assert.equal('{1,2,3,{foo = "bar",},}', dump({1, 2, 3, {foo = "bar"}}, ""))
			assert.equal('{\n  "x",\n  "y",\n}', dump({"x", "y"}, "  "))
		end)
	end)
end)
