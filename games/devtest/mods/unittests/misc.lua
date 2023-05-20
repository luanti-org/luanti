core.register_mapgen_script(core.get_modpath(core.get_current_modname()) ..
	DIR_DELIM .. "inside_mapgen_env.lua")

local function test_pseudo_random()
	-- We have comprehensive unit tests in C++, this is just to make sure the API code isn't messing up
	local gen1 = PseudoRandom(13)
	assert(gen1:next() == 22290)
	assert(gen1:next() == 13854)

	local gen2 = PseudoRandom(gen1:get_state())
	for n = 0, 16 do
		assert(gen1:next() == gen2:next())
	end

	local pr3 = PseudoRandom(-101)
	assert(pr3:next(0, 100) == 35)
	-- unusual case that is normally disallowed:
	assert(pr3:next(10000, 42767) == 12485)
end
unittests.register("test_pseudo_random", test_pseudo_random)

local function test_pcg_random()
	-- We have comprehensive unit tests in C++, this is just to make sure the API code isn't messing up
	local gen1 = PcgRandom(55)

	for n = 0, 16 do
		gen1:next()
	end

	local gen2 = PcgRandom(26)
	gen2:set_state(gen1:get_state())

	for n = 16, 32 do
		assert(gen1:next() == gen2:next())
	end
end
unittests.register("test_pcg_random", test_pcg_random)

local function test_dynamic_media(cb, player)
	if core.get_player_information(player:get_player_name()).protocol_version < 40 then
		core.log("warning", "test_dynamic_media: Client too old, skipping test.")
		return cb()
	end

	-- Check that the client acknowledges media transfers
	local path = core.get_worldpath() .. "/test_media.obj"
	local f = io.open(path, "w")
	f:write("# contents don't matter\n")
	f:close()

	local call_ok = false
	local ok = core.dynamic_add_media({
		filepath = path,
		to_player = player:get_player_name(),
	}, function(name)
		if not call_ok then
			return cb("impossible condition")
		end
		cb()
	end)
	if not ok then
		return cb("dynamic_add_media() returned error")
	end
	call_ok = true

	-- if the callback isn't called this test will just hang :shrug:
end
unittests.register("test_dynamic_media", test_dynamic_media, {async=true, player=true})

local function test_v3f_metatable(player)
	assert(vector.check(player:get_pos()))
end
unittests.register("test_v3f_metatable", test_v3f_metatable, {player=true})

local function test_v3s16_metatable(player, pos)
	local node = core.get_node(pos)
	local found_pos = core.find_node_near(pos, 0, node.name, true)
	assert(vector.check(found_pos))
end
unittests.register("test_v3s16_metatable", test_v3s16_metatable, {map=true})

local function test_clear_meta(_, pos)
	local ref = core.get_meta(pos)

	for way = 1, 3 do
		ref:set_string("foo", "bar")
		assert(ref:contains("foo"))

		if way == 1 then
			ref:from_table({})
		elseif way == 2 then
			ref:from_table(nil)
		else
			ref:set_string("foo", "")
		end

		assert(#core.find_nodes_with_meta(pos, pos) == 0, "clearing failed " .. way)
	end
end
unittests.register("test_clear_meta", test_clear_meta, {map=true})

local on_punch_called, on_place_called
core.register_on_placenode(function()
	on_place_called = true
end)
core.register_on_punchnode(function()
	on_punch_called = true
end)
local function test_node_callbacks(_, pos)
	on_place_called = false
	on_punch_called = false

	core.place_node(pos, {name="basenodes:dirt"})
	assert(on_place_called, "on_place not called")
	core.punch_node(pos)
	assert(on_punch_called, "on_punch not called")
	core.remove_node(pos)
end
unittests.register("test_node_callbacks", test_node_callbacks, {map=true})

local function test_hashing()
	local input = "hello\000world"
	assert(core.sha1(input) == "f85b420f1e43ebf88649dfcab302b898d889606c")
	assert(core.sha256(input) == "b206899bc103669c8e7b36de29d73f95b46795b508aa87d612b2ce84bfb29df2")
end
unittests.register("test_hashing", test_hashing)

local function test_compress()
	-- This text should be compressible, to make sure the results are... normal
	local text = "The\000 icey canoe couldn't move very well on the\128 lake. The\000 ice was too stiff and the icey canoe's paddles simply wouldn't punch through."
	local methods = {
		"deflate",
		"zstd",
		-- "noodle", -- for warning alarm test
	}
	local zstd_magic = string.char(0x28, 0xB5, 0x2F, 0xFD)
	for _, method in ipairs(methods) do
		local compressed = core.compress(text, method)
		assert(core.decompress(compressed, method) == text, "input/output mismatch for compression method " .. method)
		local has_zstd_magic = compressed:sub(1, 4) == zstd_magic
		if method == "zstd" then
			assert(has_zstd_magic, "zstd magic number not in zstd method")
		else
			assert(not has_zstd_magic, "zstd magic number in method " .. method .. " (which is not zstd)")
		end
	end
end
unittests.register("test_compress", test_compress)

local function test_urlencode()
	-- checks that API code handles null bytes
	assert(core.urlencode("foo\000bar!") == "foo%00bar%21")
end
unittests.register("test_urlencode", test_urlencode)

local function test_parse_json()
	local raw = "{\"how\\u0000weird\":\n\"yes\\u0000really\",\"n\":-1234567891011,\"z\":null}"
	do
		local data = core.parse_json(raw)
		assert(data["how\000weird"] == "yes\000really")
		assert(data.n == -1234567891011)
		assert(data.z == nil)
	end
	do
		local null = {}
		local data = core.parse_json(raw, null)
		assert(data.z == null)
	end
	do
		local data, err = core.parse_json('"ceci n\'est pas un json', nil, true)
		assert(data == nil)
		assert(type(err) == "string")
	end
end
unittests.register("test_parse_json", test_parse_json)

local function test_write_json()
	-- deeply nested structures should be preserved
	local leaf = 42
	local data = leaf
	for i = 1, 1000 do
		data = {data}
	end
	local roundtripped = core.parse_json(core.write_json(data))
	for i = 1, 1000 do
		roundtripped = roundtripped[1]
	end
	assert(roundtripped == 42)
end
unittests.register("test_write_json", test_write_json)

local function lint_json_files()
	-- Check that files we ship with Luanti are valid JSON
	local stack = {core.get_builtin_path()}
	local checked = 0
	while #stack > 0 do
		local path = table.remove(stack)
		for _, name in ipairs(core.get_dir_list(path, true)) do
			stack[#stack+1] = path .. "/" .. name
		end
		for _, name in ipairs(core.get_dir_list(path, false)) do
			if name:match("%.json$") then
				local f = io.open(path .. "/" .. name, "rb")
				print(path .. "/" .. name)
				assert(core.parse_json(f:read("*all"), -1) ~= nil)
				f:close()
				checked = checked + 1
			end
		end
	end
	assert(checked > 0, "no files found?!")
end
unittests.register("lint_json_files", lint_json_files)

local function test_game_info()
	local info = core.get_game_info()
	local game_conf = Settings(info.path .. "/game.conf")
	assert(info.id == "devtest")
	assert(info.title == game_conf:get("title"))
end
unittests.register("test_game_info", test_game_info)

local function test_mapgen_edges(cb)
	-- Test that the map can extend to the expected edges and no further.
	local min_edge, max_edge = core.get_mapgen_edges()
	local min_finished = {}
	local max_finished = {}
	local function finish()
		if #min_finished ~= 1 then
			return cb("Expected 1 block to emerge around mapgen minimum edge")
		end
		if min_finished[1] ~= (min_edge / core.MAP_BLOCKSIZE):floor() then
			return cb("Expected block within minimum edge to emerge")
		end
		if #max_finished ~= 1 then
			return cb("Expected 1 block to emerge around mapgen maximum edge")
		end
		if max_finished[1] ~= (max_edge / core.MAP_BLOCKSIZE):floor() then
			return cb("Expected block within maximum edge to emerge")
		end
		return cb()
	end
	local emerges_left = 2
	local function emerge_block(blockpos, action, blocks_left, finished)
		if action ~= core.EMERGE_CANCELLED then
			table.insert(finished, blockpos)
		end
		if blocks_left == 0 then
			emerges_left = emerges_left - 1
			if emerges_left == 0 then
				return finish()
			end
		end
	end
	core.emerge_area(min_edge:subtract(1), min_edge, emerge_block, min_finished)
	core.emerge_area(max_edge, max_edge:add(1), emerge_block, max_finished)
end
unittests.register("test_mapgen_edges", test_mapgen_edges, {map=true, async=true})

local finish_test_on_mapblocks_changed
core.register_on_mapblocks_changed(function(modified_blocks, modified_block_count)
	if finish_test_on_mapblocks_changed then
		finish_test_on_mapblocks_changed(modified_blocks, modified_block_count)
		finish_test_on_mapblocks_changed = nil
	end
end)
local function test_on_mapblocks_changed(cb, player, pos)
	local bp1 = (pos / core.MAP_BLOCKSIZE):floor()
	local bp2 = bp1:add(1)
	for _, bp in ipairs({bp1, bp2}) do
		-- Make a modification in the block.
		local p = bp * core.MAP_BLOCKSIZE
		core.load_area(p)
		local meta = core.get_meta(p)
		meta:set_int("test_on_mapblocks_changed", meta:get_int("test_on_mapblocks_changed") + 1)
	end
	finish_test_on_mapblocks_changed = function(modified_blocks, modified_block_count)
		if modified_block_count < 2 then
			return cb("Expected at least two mapblocks to be recorded as modified")
		end
		if not modified_blocks[core.hash_node_position(bp1)] or
				not modified_blocks[core.hash_node_position(bp2)] then
			return cb("The expected mapblocks were not recorded as modified")
		end
		cb()
	end
end
unittests.register("test_on_mapblocks_changed", test_on_mapblocks_changed, {map=true, async=true})

local function test_gennotify_api()
	local DECO_ID = 123
	local UD_ID = "unittests:dummy"

	-- the engine doesn't check if the id is actually valid, maybe it should
	core.set_gen_notify({decoration=true}, {DECO_ID})

	core.set_gen_notify({custom=true}, nil, {UD_ID})

	local flags, deco, custom = core.get_gen_notify()
	local function ff(flag)
		return (" " .. flags .. " "):match("[ ,]" .. flag .. "[ ,]") ~= nil
	end
	assert(ff("decoration"), "'decoration' flag missing")
	assert(ff("custom"), "'custom' flag missing")
	assert(table.indexof(deco, DECO_ID) > 0)
	assert(table.indexof(custom, UD_ID) > 0)

	core.set_gen_notify({decoration=false, custom=false})

	flags, deco, custom = core.get_gen_notify()
	assert(not ff("decoration") and not ff("custom"))
	assert(#deco == 0, "deco ids not empty")
	assert(#custom == 0, "custom ids not empty")
end
unittests.register("test_gennotify_api", test_gennotify_api)

-- <=> inside_mapgen_env.lua
local function test_mapgen_env(cb)
	-- emerge threads start delayed so this can take a second
	local res = core.ipc_get("unittests:mg")
	if res == nil then
		return core.after(0, test_mapgen_env, cb)
	end
	-- handle error status
	if res[1] then
		cb()
	else
		cb(res[2])
	end
end
unittests.register("test_mapgen_env", test_mapgen_env, {async=true})

local function test_ipc_vector_preserve(cb)
	-- the IPC also uses register_portable_metatable
	core.ipc_set("unittests:v", vector.new(4, 0, 4))
	local v = core.ipc_get("unittests:v")
	assert(type(v) == "table")
	assert(vector.check(v))
end
unittests.register("test_ipc_vector_preserve", test_ipc_vector_preserve)

local function test_ipc_poll(cb)
	core.ipc_set("unittests:flag", nil)
	assert(core.ipc_poll("unittests:flag", 1) == false)

	-- Note that unlike the async result callback - which has to wait for the
	-- next server step - the IPC is instant
	local t0 = core.get_us_time()
	core.handle_async(function()
		core.ipc_set("unittests:flag", true)
	end, function() end)
	assert(core.ipc_poll("unittests:flag", 1000) == true, "Wait failed (or slow machine?)")
	print("delta: " .. (core.get_us_time() - t0) .. "us")
end
unittests.register("test_ipc_poll", test_ipc_poll)

unittests.register("test_encode_network", function()
	-- 8-bit integers
	assert(core.encode_network("bbbbbbb", 0, 1, -1, -128, 127, 255, 256) ==
			"\x00\x01\xFF\x80\x7F\xFF\x00")
	assert(core.encode_network("BBBBBBB", 0, 1, -1, -128, 127, 255, 256) ==
			"\x00\x01\xFF\x80\x7F\xFF\x00")

	-- 16-bit integers
	assert(core.encode_network("hhhhhhhh",
			0,          1,          257,        -1,
			-32768,     32767,      65535,      65536) ==
			"\x00\x00".."\x00\x01".."\x01\x01".."\xFF\xFF"..
			"\x80\x00".."\x7F\xFF".."\xFF\xFF".."\x00\x00")
	assert(core.encode_network("HHHHHHHH",
			0,          1,          257,        -1,
			-32768,     32767,      65535,      65536) ==
			"\x00\x00".."\x00\x01".."\x01\x01".."\xFF\xFF"..
			"\x80\x00".."\x7F\xFF".."\xFF\xFF".."\x00\x00")

	-- 32-bit integers
	assert(core.encode_network("iiiiiiii",
			0,                  257,                2^24-1,             -1,
			-2^31,              2^31-1,             2^32-1,             2^32) ==
			"\x00\x00\x00\x00".."\x00\x00\x01\x01".."\x00\xFF\xFF\xFF".."\xFF\xFF\xFF\xFF"..
			"\x80\x00\x00\x00".."\x7F\xFF\xFF\xFF".."\xFF\xFF\xFF\xFF".."\x00\x00\x00\x00")
	assert(core.encode_network("IIIIIIII",
			0,                  257,                2^24-1,             -1,
			-2^31,              2^31-1,             2^32-1,             2^32) ==
			"\x00\x00\x00\x00".."\x00\x00\x01\x01".."\x00\xFF\xFF\xFF".."\xFF\xFF\xFF\xFF"..
			"\x80\x00\x00\x00".."\x7F\xFF\xFF\xFF".."\xFF\xFF\xFF\xFF".."\x00\x00\x00\x00")

	-- 64-bit integers
	assert(core.encode_network("llllll",
			0,                                  1,
			511,                                -1,
			2^53-1,                             -2^53) ==
			"\x00\x00\x00\x00\x00\x00\x00\x00".."\x00\x00\x00\x00\x00\x00\x00\x01"..
			"\x00\x00\x00\x00\x00\x00\x01\xFF".."\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"..
			"\x00\x1F\xFF\xFF\xFF\xFF\xFF\xFF".."\xFF\xE0\x00\x00\x00\x00\x00\x00")
	assert(core.encode_network("LLLLLL",
			0,                                  1,
			511,                                -1,
			2^53-1,                             -2^53) ==
			"\x00\x00\x00\x00\x00\x00\x00\x00".."\x00\x00\x00\x00\x00\x00\x00\x01"..
			"\x00\x00\x00\x00\x00\x00\x01\xFF".."\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"..
			"\x00\x1F\xFF\xFF\xFF\xFF\xFF\xFF".."\xFF\xE0\x00\x00\x00\x00\x00\x00")

	-- Strings
	local max_16 = string.rep("*", 2^16 - 1)
	local max_32 = string.rep("*", 2^26)

	assert(core.encode_network("ssss",
			"",                 "hello",
			max_16,             max_16.."too long") ==
			"\x00\x00"..        "\x00\x05hello"..
			"\xFF\xFF"..max_16.."\xFF\xFF"..max_16)
	assert(core.encode_network("SSSS",
			"",                         "hello",
			max_32,                     max_32.."too long") ==
			"\x00\x00\x00\x00"..        "\x00\x00\x00\x05hello"..
			"\x04\x00\x00\x00"..max_32.."\x04\x00\x00\x00"..max_32)
	assert(core.encode_network("zzzz",
			"",   "hello",   "hello\0embedded", max_16.."longer") ==
			"\0".."hello\0".."hello\0"..        max_16.."longer\0")
	assert(core.encode_network("ZZZZ",
			"", "hello", "hello\0embedded", max_16.."longer") ==
			"".."hello".."hello\0embedded"..max_16.."longer")

	-- Spaces
	assert(core.encode_network("B I", 255, 2^31) == "\xFF\x80\x00\x00\x00")
	assert(core.encode_network("  B Zz ", 15, "abc", "xyz") == "\x0Fabcxyz\0")

	-- Empty format strings
	assert(core.encode_network("") == "")
	assert(core.encode_network("   ", 5, "extra args") == "")
end)

unittests.register("test_decode_network", function()
	local d

	-- 8-bit integers
	d = {core.decode_network("bbbbb", "\x00\x01\x7F\x80\xFF")}
	assert(#d == 5)
	assert(d[1] == 0 and d[2] == 1 and d[3] == 127 and d[4] == -128 and d[5] == -1)

	d = {core.decode_network("BBBBB", "\x00\x01\x7F\x80\xFF")}
	assert(#d == 5)
	assert(d[1] == 0 and d[2] == 1 and d[3] == 127 and d[4] == 128 and d[5] == 255)

	-- 16-bit integers
	d = {core.decode_network("hhhhhh",
			"\x00\x00".."\x00\x01".."\x01\x01"..
			"\x7F\xFF".."\x80\x00".."\xFF\xFF")}
	assert(#d == 6)
	assert(d[1] == 0 and d[2] == 1 and d[3] == 257 and
			d[4] == 32767 and d[5] == -32768 and d[6] == -1)

	d = {core.decode_network("HHHHHH",
			"\x00\x00".."\x00\x01".."\x01\x01"..
			"\x7F\xFF".."\x80\x00".."\xFF\xFF")}
	assert(#d == 6)
	assert(d[1] == 0 and d[2] == 1 and d[3] == 257 and
			d[4] == 32767 and d[5] == 32768 and d[6] == 65535)

	-- 32-bit integers
	d = {core.decode_network("iiiiii",
			"\x00\x00\x00\x00".."\x00\x00\x00\x01".."\x00\xFF\xFF\xFF"..
			"\x7F\xFF\xFF\xFF".."\x80\x00\x00\x00".."\xFF\xFF\xFF\xFF")}
	assert(#d == 6)
	assert(d[1] == 0 and d[2] == 1 and d[3] == 2^24-1 and
			d[4] == 2^31-1 and d[5] == -2^31 and d[6] == -1)

	d = {core.decode_network("IIIIII",
			"\x00\x00\x00\x00".."\x00\x00\x00\x01".."\x00\xFF\xFF\xFF"..
			"\x7F\xFF\xFF\xFF".."\x80\x00\x00\x00".."\xFF\xFF\xFF\xFF")}
	assert(#d == 6)
	assert(d[1] == 0 and d[2] == 1 and d[3] == 2^24-1 and
			d[4] == 2^31-1 and d[5] == 2^31 and d[6] == 2^32-1)

	-- 64-bit integers
	d = {core.decode_network("llllll",
			"\x00\x00\x00\x00\x00\x00\x00\x00".."\x00\x00\x00\x00\x00\x00\x00\x01"..
			"\x00\x00\x00\x00\x00\x00\x01\xFF".."\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"..
			"\x00\x1F\xFF\xFF\xFF\xFF\xFF\xFF".."\xFF\xE0\x00\x00\x00\x00\x00\x00")}
	assert(#d == 6)
	assert(d[1] == 0 and d[2] == 1 and d[3] == 511 and
			d[4] == -1 and d[5] == 2^53-1 and d[6] == -2^53)

	d = {core.decode_network("LLLLLL",
			"\x00\x00\x00\x00\x00\x00\x00\x00".."\x00\x00\x00\x00\x00\x00\x00\x01"..
			"\x00\x00\x00\x00\x00\x00\x01\xFF".."\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"..
			"\x00\x1F\xFF\xFF\xFF\xFF\xFF\xFF".."\xFF\xE0\x00\x00\x00\x00\x00\x00")}
	assert(#d == 6)
	assert(d[1] == 0 and d[2] == 1 and d[3] == 511 and
			d[4] == 2^64-1 and d[5] == 2^53-1 and d[6] == 2^64 - 2^53)

	-- Floating point numbers
	local enc = core.encode_network("fff",
			0.0, 123.456, -987.654)
	assert(#enc == 3 * 4)

	d = {core.decode_network("fff", enc)}
	assert(#d == 3)
	assert(d[1] == 0.0 and d[2] > 123.45 and d[2] < 123.46 and
			d[3] > -987.66 and d[3] < -987.65)

	-- Strings
	local max_16 = string.rep("*", 2^16 - 1)
	local max_32 = string.rep("*", 2^26)

	d = {core.decode_network("ssss",
			"\x00\x00".."\x00\x05hello".."\xFF\xFF"..max_16.."\x00\xFFtoo short")}
	assert(#d == 4)
	assert(d[1] == "" and d[2] == "hello" and d[3] == max_16 and d[4] == "too short")

	d = {core.decode_network("SSSSS",
			"\x00\x00\x00\x00".."\x00\x00\x00\x05hello"..
			"\x04\x00\x00\x00"..max_32.."\x04\x00\x00\x08"..max_32.."too long"..
			"\x00\x00\x00\xFFtoo short")}
	assert(#d == 5)
	assert(d[1] == "" and d[2] == "hello" and
			d[3] == max_32 and d[4] == max_32 and d[5] == "too short")

	d = {core.decode_network("zzzz", "\0".."hello\0".."missing end")}
	assert(#d == 4)
	assert(d[1] == "" and d[2] == "hello" and d[3] == "missing end" and d[4] == "")

	-- Verbatim strings
	d = {core.decode_network("ZZZZ", "xxxyyyyyzzz", 3, 0, 5, -1)}
	assert(#d == 4)
	assert(d[1] == "xxx" and d[2] == "" and d[3] == "yyyyy" and d[4] == "zzz")

	-- Read past end
	d = {core.decode_network("bhilBHILf", "")}
	assert(#d == 9)
	assert(d[1] == 0 and d[2] == 0 and d[3] == 0 and d[4] == 0 and
			d[5] == 0 and d[6] == 0 and d[7] == 0 and d[8] == 0 and d[9] == 0.0)

	d = {core.decode_network("ZsSzZ", "xx", 4, 4)}
	assert(#d == 5)
	assert(d[1] == "xx\0\0" and d[2] == "" and d[3] == "" and
			d[4] == "" and d[5] == "\0\0\0\0")

	-- Spaces
	d = {core.decode_network("B I", "\xFF\x80\x00\x00\x00")}
	assert(#d == 2)
	assert(d[1] == 255 and d[2] == 2^31)

	d = {core.decode_network("  B Zz ", "\x0Fabcxyz\0", 3)}
	assert(#d == 3)
	assert(d[1] == 15 and d[2] == "abc" and d[3] == "xyz")

	-- Empty format strings
	d = {core.decode_network("", "some random data")}
	assert(#d == 0)
	d = {core.decode_network("   ", "some random data", 3, 5)}
	assert(#d == 0)
end)
