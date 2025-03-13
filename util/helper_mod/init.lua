local mode = core.settings:get("helper_mode")

if mode == "devtest" then

	-- Provide feedback to script by creating files in world path
	core.after(0, function()
		io.close(io.open(core.get_worldpath() .. "/startup", "w"))
	end)
	local function callback(test_ok)
		if not test_ok then
			io.close(io.open(core.get_worldpath() .. "/test_failure", "w"))
		end
		io.close(io.open(core.get_worldpath() .. "/done", "w"))
		core.request_shutdown("", false, 2)
	end
	-- If tests are enabled exit when they're done, otherwise exit on player join
	if core.settings:get_bool("devtest_unittests_autostart") and core.global_exists("unittests") then
		unittests.on_finished = callback
	else
		core.register_on_joinplayer(function() callback(true) end)
	end

elseif mode == "mapgen" then

	-- Stress-test mapgen by constantly generating new area
	local csize = tonumber(core.settings:get("chunksize")) * core.MAP_BLOCKSIZE
	local MINP, MAXP = vector.new(0, -csize, 0), vector.new(csize*3, csize*2, csize)
	local DIR = "x"
	local pstart = vector.new(0, 0, 0)
	local next_, callback
	next_ = function(arg)
		print("emerging " .. core.pos_to_string(pstart))
		core.emerge_area(
			vector.add(pstart, MINP), vector.add(pstart, MAXP),
			callback, arg
		)
	end
	local trig = {}
	callback = function(blockpos, action, calls_rem, n)
		if action == core.EMERGE_CANCELLED or action == core.EMERGE_ERRORED then
			return
		end
		if calls_rem <= 20 and not trig[n] then
			trig[n] = true
			pstart[DIR] = pstart[DIR] + (MAXP[DIR] - MINP[DIR])
			next_(n + 1)
		end
	end
	core.after(0, next_, 1)

elseif mode == "error" then

	local n = tonumber(core.settings:get("error_type"))
	local error_lua = core.get_modpath(core.get_current_modname()) .. "/error.lua"
	if n == 1 then
		print("=> error during startup <=")
		error("intentional")
	elseif n == 2 then
		print("=> error on first step <=")
		core.after(0, error, "intentional")
	elseif n == 3 then
		print("=> error in async script <=")
		core.register_async_dofile(error_lua)
	elseif n == 4 then
		print("=> error in mapgen script <=")
		core.register_mapgen_script(error_lua)
	else
		assert(false)
	end

end
