-- Luanti
-- Copyright (C) 2022 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later

local mods_dir = "/tmp/.minetest/mods"
local games_dir = "/tmp/.minetest/games"
local txp_dir = "/tmp/.minetest/textures"

local function reset()
	local env = {
		core = {},
		unpack = table.unpack or unpack,
		pkgmgr = {},
		DIR_DELIM = "/",
	}
	env._G = env
	setmetatable(env, { __index = _G })

	local core = env.core

	local calls = {}
	function core.get_games()
		return {}
	end
	function core.delete_dir(...)
		table.insert(calls, { "delete_dir", ... })
		return true
	end
	function core.copy_dir(...)
		table.insert(calls, { "copy_dir", ... })
		return true
	end
	function core.get_texturepath()
		return txp_dir
	end
	function core.get_texturepath_share()
		return txp_dir
	end
	function core.get_modpath()
		return mods_dir
	end
	function core.get_gamepath()
		return games_dir
	end
	function env.fgettext_ne(fmt, ...)
		return fmt
	end

	setfenv(loadfile("builtin/common/misc_helpers.lua"), env)()
	setfenv(loadfile("builtin/mainmenu/content/pkgmgr.lua"), env)()

	function env.assert_calls(list)
		assert.are.same(list, calls)
	end

	return env
end


describe("install_dir", function()
	it("installs texture pack", function()
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = "invalid", path = "/tmp/123" }
		end

		local path, message = env.pkgmgr.install_dir("txp", "/tmp/123", "mytxp", nil)
		assert.is.equal(txp_dir .. "/mytxp", path)
		assert.is._nil(message)
		env.assert_calls({
			{ "delete_dir", txp_dir .. "/mytxp" },
			{ "copy_dir", "/tmp/123", txp_dir .. "/mytxp", false },
		})
	end)

	it("prevents installing other as texture pack", function()
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = "mod", path = "/tmp/123" }
		end

		local path, message = env.pkgmgr.install_dir("txp", "/tmp/123", "mytxp", nil)
		assert.is._nil(path)
		assert.is.equal("Unable to install a $1 as a texture pack", message)
	end)

	it("installs mod", function()
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = "mod", path = "/tmp/123" }
		end

		local path, message = env.pkgmgr.install_dir("mod", "/tmp/123", "mymod", nil)
		assert.is.equal(mods_dir .. "/mymod", path)
		assert.is._nil(message)
		env.assert_calls({
			{ "delete_dir", mods_dir .. "/mymod" },
			{ "copy_dir", "/tmp/123", mods_dir .. "/mymod", false },
		})
	end)

	it("installs modpack", function()
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = "modpack", path = "/tmp/123" }
		end

		local path, message = env.pkgmgr.install_dir("mod", "/tmp/123", "mymod", nil)
		assert.is.equal(mods_dir .. "/mymod", path)
		assert.is._nil(message)
		env.assert_calls({
			{ "delete_dir", mods_dir .. "/mymod" },
			{ "copy_dir", "/tmp/123", mods_dir .. "/mymod", false },
		})
	end)

	it("installs game", function()
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = "game", path = "/tmp/123" }
		end

		local path, message = env.pkgmgr.install_dir("game", "/tmp/123", "mygame", nil)
		assert.is.equal(games_dir .. "/mygame", path)
		assert.is._nil(message)
		env.assert_calls({
			{ "delete_dir", games_dir .. "/mygame" },
			{ "copy_dir", "/tmp/123", games_dir .. "/mygame", false },
		})
	end)

	it("installs mod detects name", function()
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = "mod", path = "/tmp/123" }
		end

		local path, message = env.pkgmgr.install_dir("mod", "/tmp/123", nil, nil)
		assert.is.equal(mods_dir .. "/123", path)
		assert.is._nil(message)
		env.assert_calls({
			{ "delete_dir", mods_dir .. "/123" },
			{ "copy_dir", "/tmp/123", mods_dir .. "/123", false },
		})
	end)

	it("installs mod detects invalid name", function()
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = "mod", path = "/tmp/hi!" }
		end

		local path, message = env.pkgmgr.install_dir("mod", "/tmp/hi!", nil, nil)
		assert.is._nil(path)
		assert.is.equal("Install: Unable to find suitable folder name for $1", message)
	end)

	it("installs mod to target (update)", function()
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = "mod", path = "/tmp/123" }
		end

		local path, message = env.pkgmgr.install_dir("mod", "/tmp/123", "mymod", "/tmp/alt-target")
		assert.is.equal("/tmp/alt-target", path)
		assert.is._nil(message)
		env.assert_calls({
			{ "delete_dir", "/tmp/alt-target" },
			{ "copy_dir", "/tmp/123", "/tmp/alt-target", false },
		})
	end)

	it("checks expected types", function()
		local actual_type = "modpack"
		local env = reset()
		env.pkgmgr.get_base_folder = function()
			return { type = actual_type, path = "/tmp/123" }
		end

		local path, message = env.pkgmgr.install_dir("mod", "/tmp/123", "name", nil)
		assert.is._not._nil(path)
		assert.is._nil(message)

		path, message = env.pkgmgr.install_dir("game", "/tmp/123", "name", nil)
		assert.is._nil(path)
		assert.is.equal("Unable to install a $1 as a $2", message)

		path, message = env.pkgmgr.install_dir("txp", "/tmp/123", "name", nil)
		assert.is._nil(path)
		assert.is.equal("Unable to install a $1 as a texture pack", message)

		actual_type = "game"

		path, message = env.pkgmgr.install_dir("mod", "/tmp/123", "name", nil)
		assert.is._nil(path)
		assert.is.equal("Unable to install a $1 as a $2", message)

		path, message = env.pkgmgr.install_dir("game", "/tmp/123", "name", nil)
		assert.is._not._nil(path)
		assert.is._nil(message)

		path, message = env.pkgmgr.install_dir("txp", "/tmp/123", "name", nil)
		assert.is._nil(path)
		assert.is.equal("Unable to install a $1 as a texture pack", message)

		actual_type = "txp"

		path, message = env.pkgmgr.install_dir("mod", "/tmp/123", "name", nil)
		assert.is._nil(path)
		assert.is.equal("Unable to install a $1 as a $2", message)

		path, message = env.pkgmgr.install_dir("game", "/tmp/123", "name", nil)
		assert.is._nil(path)
		assert.is.equal("Unable to install a $1 as a $2", message)

		path, message = env.pkgmgr.install_dir("txp", "/tmp/123", "name", nil)
		assert.is._not._nil(path)
		assert.is._nil(message)
	end)
end)
