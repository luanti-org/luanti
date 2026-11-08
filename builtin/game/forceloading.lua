-- Prevent anyone else accessing those functions
local forceload_block = core.forceload_block
local forceload_free_block = core.forceload_free_block
core.forceload_block = nil
core.forceload_free_block = nil

local blocks_forceloaded
local blocks_temploaded = {}
local total_forceloaded = 0

-- true, if the forceloaded blocks got changed (flag for persistence on-disk)
local forceload_blocks_changed = false

local BLOCKSIZE = core.MAP_BLOCKSIZE
local function get_blockpos(pos)
	return {
		x = math.floor(pos.x/BLOCKSIZE),
		y = math.floor(pos.y/BLOCKSIZE),
		z = math.floor(pos.z/BLOCKSIZE)}
end

-- When we create/free a forceload, it's either transient or persistent. We want
-- to add to/remove from the table that corresponds to the type of forceload, but
-- we also need the other table because whether we forceload a block depends on
-- both tables.
-- This function returns the "primary" table we are adding to/removing from, and
-- the other table.
local function get_relevant_tables(transient)
	if transient then
		return blocks_temploaded, blocks_forceloaded
	else
		return blocks_forceloaded, blocks_temploaded
	end
end

function core.forceload_block(pos, transient, limit)
	local blockpos = get_blockpos(pos)
	local hash = core.hash_node_position(blockpos)
	local relevant_table, other_table = get_relevant_tables(transient)
	if relevant_table[hash] ~= nil then
		relevant_table[hash] = relevant_table[hash] + 1
	elseif other_table[hash] ~= nil then
		relevant_table[hash] = 1
	else
		limit = limit or tonumber(core.settings:get("max_forceloaded_blocks")) or 16
		if limit >= 0 and total_forceloaded >= limit then
			return false
		end
		total_forceloaded = total_forceloaded+1
		relevant_table[hash] = 1
		forceload_block(blockpos)
	end
	if not transient then
		forceload_blocks_changed = true
	end
	return true
end

function core.forceload_free_block(pos, transient)
	local blockpos = get_blockpos(pos)
	local hash = core.hash_node_position(blockpos)
	local relevant_table, other_table = get_relevant_tables(transient)
	if relevant_table[hash] == nil then return end
	if relevant_table[hash] > 1 then
		relevant_table[hash] = relevant_table[hash] - 1
	elseif other_table[hash] ~= nil then
		relevant_table[hash] = nil
	else
		total_forceloaded = total_forceloaded-1
		relevant_table[hash] = nil
		forceload_free_block(blockpos)
	end
	if not transient then
		forceload_blocks_changed = true
	end
end

-- Keep the forceloaded areas after restart
local wpath = core.get_worldpath()
local function read_file(filename)
	local f = io.open(filename, "r")
	if f==nil then return {} end
	local t = f:read("*all")
	f:close()
	if t=="" or t==nil then return {} end
	return core.deserialize(t) or {}
end

blocks_forceloaded = read_file(wpath.."/force_loaded.txt")
for _, __ in pairs(blocks_forceloaded) do
	total_forceloaded = total_forceloaded + 1
end

core.after(5, function()
	for hash, _ in pairs(blocks_forceloaded) do
		local blockpos = core.get_position_from_hash(hash)
		forceload_block(blockpos)
	end
end)

-- persists the currently forceloaded blocks to disk
local function persist_forceloaded_blocks()
	local data = core.serialize(blocks_forceloaded)
	return core.safe_file_write(wpath.."/force_loaded.txt", data)
end

-- periodical forceload persistence
local function periodically_persist_forceloaded_blocks()

	-- only persist if the blocks actually changed
	if forceload_blocks_changed and persist_forceloaded_blocks() then
		-- reset changed flag
		forceload_blocks_changed = false
	end

	-- recheck after some time
	core.after(10, periodically_persist_forceloaded_blocks)
end

-- persist periodically
core.after(5, periodically_persist_forceloaded_blocks)

-- persist on shutdown
core.register_on_shutdown(persist_forceloaded_blocks)
