-- mods/qol_singlefile/init.lua
-- Quality-of-Life bundle in ONE FILE for Luanti (formerly Minetest).
-- Features:
--   1) Smart vein/treetop mining (safe, capped, batched)
--   2) /sortinv command (merge by metadata, then sort)
--   3) /vein command (toggle + tweak per player) and first-join hints
--
-- Uses documented APIs: register_on_dignode, register_chatcommand,
-- register_on_joinplayer, core.after, find_nodes_in_area, node_dig,
-- InvRef get_list/set_list/add_item, ItemStack to_table. See docs:
--   core callbacks & helpers: https://api.luanti.org/core-namespace-reference/
--   timing/batching: https://docs.luanti.org/for-creators/api/timing-and-event-loop/
--   inventory & ItemStack: https://api.luanti.org/class-reference/
local core = core or minetest
local S = function(s) return s end

-- -----------------------
-- Config (server defaults)
-- -----------------------
local DEF = {
  vein_enabled_default = true,
  vein_radius = 4,           -- cube radius around the starting node (max manhattan ~8)
  vein_max_nodes = 64,       -- hard cap per action
  batch_size = 8,            -- nodes per tick to dig
  allow_trees = true,
  allow_ores  = true,
  tree_name_patterns = {":tree", ":log"},
  ore_name_pattern = "_with_",
}

-- read optional settings: qol_singlefile.<key>
local set = core.settings
local function get_int(k, d)  return tonumber(set:get("qol_singlefile."..k) or "") or d end
local function get_bool(k, d) local v=set:get("qol_singlefile."..k); if v==nil then return d end; v=v:lower(); return v=="true" or v=="1" or v=="yes" end

DEF.vein_radius      = math.max(1, get_int("vein_radius", DEF.vein_radius))
DEF.vein_max_nodes   = math.max(1, get_int("vein_max_nodes", DEF.vein_max_nodes))
DEF.batch_size       = math.max(1, get_int("batch_size", DEF.batch_size))
DEF.vein_enabled_default = get_bool("vein_enabled_default", DEF.vein_enabled_default)
DEF.allow_trees      = get_bool("allow_trees", DEF.allow_trees)
DEF.allow_ores       = get_bool("allow_ores", DEF.allow_ores)

-- -----------------------
-- Small utils
-- -----------------------
local function player_name(p) return p and p.get_player_name and p:get_player_name() or "" end

local function is_tree_node(name)
  if not DEF.allow_trees then return false end
  for _,pat in ipairs(DEF.tree_name_patterns) do
    if name:find(pat, 1, true) then return true end
  end
  -- also accept nodes with group "tree" if present
  local def = core.registered_nodes[name]
  return def and def.groups and def.groups.tree ~= nil
end

local function is_ore_node(name)
  if not DEF.allow_ores then return false end
  return name:find(DEF.ore_name_pattern, 1, true) ~= nil
end

-- Axis neighbors (6-connected)
local OFFS = {
  {x= 1,y= 0,z= 0},{x=-1,y= 0,z= 0},
  {x= 0,y= 1,z= 0},{x= 0,y=-1,z= 0},
  {x= 0,y= 0,z= 1},{x= 0,y= 0,z=-1},
}
local function add(a,b) return {x=a.x+b.x,y=a.y+b.y,z=a.z+b.z} end
local function within_radius(p0, p, r)
  return math.abs(p.x-p0.x) <= r and math.abs(p.y-p0.y) <= r and math.abs(p.z-p0.z) <= r
end

-- --------------------------------------------
-- Per-player state: toggles + reentry guard
-- --------------------------------------------
local vein_enabled = {}   -- name -> bool
local busy_dig     = {}   -- name -> bool (reentry guard)
local vein_radius  = {}   -- name -> int
local vein_max     = {}   -- name -> int

-- Provide defaults when players join
core.register_on_joinplayer(function(player)
  local name = player_name(player)
  vein_enabled[name] = DEF.vein_enabled_default
  vein_radius[name]  = DEF.vein_radius
  vein_max[name]     = DEF.vein_max_nodes

  local meta = player:get_meta()
  if meta:get_int("qol_singlefile_hinted") ~= 1 then
    core.chat_send_player(name, S("[QOL] Try /vein (toggle or set radius/max) and /sortinv to tidy your inventory."))
    meta:set_int("qol_singlefile_hinted", 1)
  end
end)

-- -----------------------
-- /vein chat command
-- -----------------------
local function show_vein_info(name)
  core.chat_send_player(name, ("[QOL] vein=%s, radius=%d, max=%d, batch=%d"):
    format(vein_enabled[name] and "on" or "off", vein_radius[name], vein_max[name], DEF.batch_size))
end

core.register_chatcommand("vein", {
  params = S("[on|off|radius <n>|max <n>|info]"),
  description = S("Toggle or tune smart vein/treetop mining for you."),
  func = function(name, param)
    param = (param or ""):gsub("^%s+",""):gsub("%s+$","")
    if param == "" or param == "info" then
      show_vein_info(name); return true
    end
    if param == "on" then vein_enabled[name]=true; show_vein_info(name); return true end
    if param == "off" then vein_enabled[name]=false; show_vein_info(name); return true end

    local k,v = param:match("^(radius)%s+(%d+)$")
    if not k then k,v = param:match("^(max)%s+(%d+)$") end
    if k and v then
      v = tonumber(v)
      if k=="radius" then vein_radius[name] = math.max(1, math.min(16, v)) end
      if k=="max"    then vein_max[name]    = math.max(1, math.min(512, v)) end
      show_vein_info(name); return true
    end
    core.chat_send_player(name, S("[QOL] Usage: /vein [on|off|radius <n>|max <n>|info]"))
    return false
  end
})

-- -------------------------------------------------
-- Smart Vein / Treetop Mining (batched, protected)
-- -------------------------------------------------
local function classify_for_cluster(name)
  if is_tree_node(name) then return "tree" end
  if is_ore_node(name)  then return "ore"  end
  return nil
end

-- BFS to collect cluster candidates around pos0
local function collect_cluster(pos0, name0, class, r, maxn)
  local q, qi, visited = {pos0}, 1, {}
  local out = {}
  local function key(p) return p.x.."|"..p.y.."|"..p.z end
  visited[key(pos0)] = true

  while qi <= #q and #out < maxn do
    local p = q[qi]; qi = qi + 1
    -- Explore neighbors within radius
    for _,o in ipairs(OFFS) do
      local np = add(p, o)
      if within_radius(pos0, np, r) then
        local sk = key(np)
        if not visited[sk] then
          visited[sk] = true
          local nn = core.get_node(np).name
          local ok = false
          if class == "ore" then
            ok = (nn == name0)  -- strictly same ore node
          elseif class == "tree" then
            ok = is_tree_node(nn) -- any connected trunk segment
          end
          if ok then
            out[#out+1] = {pos=np, name=nn}
            if #out >= maxn then break end
            q[#q+1] = np
          end
        end
      end
    end
  end
  return out
end

-- Process a list of positions in batches to avoid long server steps
local function dig_batched(digger, todo)
  local name = player_name(digger)
  if not name or not digger:is_player() then return end
  local function step()
    if not todo or #todo == 0 then busy_dig[name] = nil; return end
    -- dig up to batch_size nodes this tick
    for i = 1, math.min(DEF.batch_size, #todo) do
      local entry = table.remove(todo) -- pop from end
      local p = entry.pos
      -- double-check current node still matches; skip if changed or protected
      local n = core.get_node(p)
      if n and n.name == entry.name then
        -- node_dig enforces can_dig, protection & gives proper drops/wear
        core.node_dig(p, n, digger)
      end
    end
    core.after(0, step)
  end
  core.after(0, step)
end

core.register_on_dignode(function(pos, oldnode, digger)
  if not digger or not digger:is_player() then return end
  local name = player_name(digger)
  if busy_dig[name] then return end                   -- prevent recursion
  if vein_enabled[name] == false then return end      -- explicit off
  local class = classify_for_cluster(oldnode.name)
  if not class then return end

  local r   = vein_radius[name] or DEF.vein_radius
  local max = vein_max[name] or DEF.vein_max_nodes

  -- Collect neighbors *excluding* the original node (already dug)
  local todo = collect_cluster(pos, oldnode.name, class, r, max)
  if #todo == 0 then return end

  busy_dig[name] = true
  dig_batched(digger, todo)
end)

-- -----------------------
-- /sortinv chat command
-- -----------------------
local function item_key_for_merge(tbl) -- tbl from ItemStack:to_table()
  -- Key by name + metadata (meta string) ONLY. Wear must stay separate.
  local name = tbl.name or ""
  local meta = tbl.metadata or ""
  return name .. "||" .. meta
end

local function classify_item_type(def)
  -- Prefer def.type when present; fallback by stackability
  local t = def and def.type
  if t == "tool" then return 1 end            -- keep together
  if t == "node" then return 2 end
  if t == "craft" or t == "craftitem" then return 3 end
  -- Fallback buckets
  if def and def.stack_max == 1 then return 1 end
  return 2
end

local function sort_and_merge_inventory(player)
  local inv = player:get_inventory()
  if not inv then return 0 end
  local list = inv:get_list("main") or {}
  if #list == 0 then return 0 end

  -- Separate tools (unstackable / wearful) and mergeable items (by name+metadata)
  local merge_map = {}     -- key -> {name, meta_string, count}
  local tools = {}         -- array of ItemStack (kept as-is)
  for _,stk in ipairs(list) do
    if not stk:is_empty() then
      local def = stk:get_definition() or {}
      local t = def.type
      local stackable = (def.stack_max or 99) > 1 and t ~= "tool" and stk:get_wear() == 0
      if stackable then
        local tbl = stk:to_table() or {}
        local key = item_key_for_merge(tbl)
        local ent = merge_map[key]
        if ent then
          ent.count = ent.count + (tbl.count or 1)
        else
          merge_map[key] = {name = tbl.name, meta = tbl.metadata or "", count = tbl.count or 1}
        end
      else
        tools[#tools+1] = stk
      end
    end
  end

  -- Build back a flat array of ItemStacks, sorted by category then name
  local merged = {}
  for _,ent in pairs(merge_map) do
    -- Create an itemstring with meta preserved; when metadata is present, we must reconstruct via ItemStack table
    local remaining = ent.count
    local stack_max = (core.registered_items[ent.name] and core.registered_items[ent.name].stack_max) or 99
    while remaining > 0 do
      local take = math.min(stack_max, remaining)
      local tbl = {name = ent.name, count = take}
      if ent.meta ~= "" then tbl.metadata = ent.meta end
      merged[#merged+1] = ItemStack(tbl)
      remaining = remaining - take
    end
  end

  -- Sort merged & tools with a stable order:
  local function sort_key(stk)
    local def = stk:get_definition() or {}
    local bucket = classify_item_type(def)
    local nm = stk:get_name() or ""
    return bucket, nm
  end
  table.sort(merged, function(a,b)
    local ba, na = sort_key(a)
    local bb, nb = sort_key(b)
    if ba ~= bb then return ba < bb end
    return na < nb
  end)

  -- Rebuild "main": tools first (grouped by type order), then merged stacks
  -- Tools themselves also sorted lightly for predictability
  table.sort(tools, function(a,b) return (a:get_name() or "") < (b:get_name() or "") end)

  local newlist = {}
  for i=1,#tools do newlist[#newlist+1] = tools[i] end
  for i=1,#merged do newlist[#newlist+1] = merged[i] end
  -- pad to size
  for i=#newlist+1, #list do newlist[i] = ItemStack("") end

  inv:set_list("main", newlist)
  return #tools + #merged
end

core.register_chatcommand("sortinv", {
  description = S("Merge compatible stacks (per-metadata) and sort inventory."),
  func = function(name, param)
    local player = core.get_player_by_name(name)
    if not player then return false, S("Player not found") end
    local moved = sort_and_merge_inventory(player)
    return true, S(("[QOL] Sorted %d stacks."):format(moved))
  end
})
