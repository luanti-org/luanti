local builtin_shared = ...

local facedir_to_euler = {
	{y = 0, x = 0, z = 0},
	{y = -math.pi/2, x = 0, z = 0},
	{y = math.pi, x = 0, z = 0},
	{y = math.pi/2, x = 0, z = 0},
	{y = math.pi/2, x = -math.pi/2, z = math.pi/2},
	{y = math.pi/2, x = math.pi, z = math.pi/2},
	{y = math.pi/2, x = math.pi/2, z = math.pi/2},
	{y = math.pi/2, x = 0, z = math.pi/2},
	{y = -math.pi/2, x = math.pi/2, z = math.pi/2},
	{y = -math.pi/2, x = 0, z = math.pi/2},
	{y = -math.pi/2, x = -math.pi/2, z = math.pi/2},
	{y = -math.pi/2, x = math.pi, z = math.pi/2},
	{y = 0, x = 0, z = math.pi/2},
	{y = 0, x = -math.pi/2, z = math.pi/2},
	{y = 0, x = math.pi, z = math.pi/2},
	{y = 0, x = math.pi/2, z = math.pi/2},
	{y = math.pi, x = math.pi, z = math.pi/2},
	{y = math.pi, x = math.pi/2, z = math.pi/2},
	{y = math.pi, x = 0, z = math.pi/2},
	{y = math.pi, x = -math.pi/2, z = math.pi/2},
	{y = math.pi, x = math.pi, z = 0},
	{y = -math.pi/2, x = math.pi, z = 0},
	{y = 0, x = math.pi, z = 0},
	{y = math.pi/2, x = math.pi, z = 0}
}

local gravity = tonumber(core.settings:get("movement_gravity")) or 9.81

--
-- Falling stuff
--

core.register_entity(":__builtin:falling_node", {
	initial_properties = {
		visual = "node",
		physical = true,
		is_visible = false,
		collide_with_objects = true,
		collisionbox = {-0.5, -0.5, -0.5, 0.5, 0.5, 0.5},
	},

	node = {},
	meta = {},
	floats = false,

	set_node = function(self, node, meta)
		node.param2 = node.param2 or 0
		self.node = node
		meta = meta or {}
		if type(meta.to_table) == "function" then
			meta = meta:to_table()
		end
		for _, list in pairs(meta.inventory or {}) do
			for i, stack in pairs(list) do
				if type(stack) == "userdata" then
					list[i] = stack:to_string()
				end
			end
		end
		local def = core.registered_nodes[node.name]
		if not def then
			-- Don't allow unknown nodes to fall
			core.log("info",
				"Unknown falling node removed at "..
				core.pos_to_string(self.object:get_pos()))
			self.object:remove()
			return
		end
		self.meta = meta

		-- Cache whether we're supposed to float on water
		self.floats = core.get_item_group(node.name, "float") ~= 0

		-- Save liquidtype for falling water
		self.liquidtype = def.liquidtype

		-- Set up entity visuals
		-- For compatibility with older clients we continue to use "item" visual
		-- for simple situations.
		local drawtypes = {normal=true, glasslike=true, allfaces=true, nodebox=true}
		local p2types = {none=true, facedir=true, ["4dir"]=true}
		if drawtypes[def.drawtype] and p2types[def.paramtype2] and def.use_texture_alpha ~= "blend" then
			-- Calculate size of falling node
			local s = vector.zero()
			s.x = (def.visual_scale or 1) * 0.667
			s.y = s.x
			s.z = s.x
			-- Compensate for wield_scale
			if def.wield_scale then
				s.x = s.x / def.wield_scale.x
				s.y = s.y / def.wield_scale.y
				s.z = s.z / def.wield_scale.z
			end
			self.object:set_properties({
				is_visible = true,
				visual = "item",
				wield_item = node.name,
				visual_size = s,
				glow = def.light_source,
			})
			-- Rotate as needed
			if def.paramtype2 == "facedir" then
				local fdir = node.param2 % 32 % 24
				local euler = facedir_to_euler[fdir + 1]
				if euler then
					self.object:set_rotation(euler)
				end
			elseif def.paramtype2 == "4dir" then
				local fdir = node.param2 % 4
				local euler = facedir_to_euler[fdir + 1]
				if euler then
					self.object:set_rotation(euler)
				end
			end
		elseif def.drawtype ~= "airlike" then
			self.object:set_properties({
				is_visible = true,
				node = node,
				glow = def.light_source,
			})
		end

		-- Set collision box (certain nodeboxes only for now)
		local nb_types = {fixed=true, leveled=true, connected=true}
		if def.drawtype == "nodebox" and def.node_box and
			nb_types[def.node_box.type] and def.node_box.fixed then
			local box = table.copy(def.node_box.fixed)
			if type(box[1]) == "table" then
				box = #box == 1 and box[1] or nil -- We can only use a single box
			end
			if box then
				if def.paramtype2 == "leveled" and (self.node.level or 0) > 0 then
					box[5] = -0.5 + self.node.level / 64
				end
				self.object:set_properties({
					collisionbox = box
				})
			end
		end
	end,

	get_staticdata = function(self)
		local ds = {
			node = self.node,
			meta = self.meta,
		}
		return core.serialize(ds)
	end,

	on_activate = function(self, staticdata)
		self.object:set_armor_groups({immortal = 1})
		self.object:set_acceleration(vector.new(0, -gravity, 0))

		local ds = core.deserialize(staticdata)
		if ds and ds.node then
			self:set_node(ds.node, ds.meta)
		elseif ds then
			self:set_node(ds)
		elseif staticdata ~= "" then
			self:set_node({name = staticdata})
		end
	end,

	try_place = function(self, bcp, bcn)
		local bcd = core.registered_nodes[bcn.name]
		-- Add levels if dropped on same leveled node
		if bcd and bcd.paramtype2 == "leveled" and
				bcn.name == self.node.name then
			local addlevel = self.node.level
			if (addlevel or 0) <= 0 then
				addlevel = bcd.leveled
			end
			if core.add_node_level(bcp, addlevel) < addlevel then
				return true
			elseif bcd.buildable_to then
				-- Node level has already reached max, don't place anything
				return true
			end
		end

		-- Decide if we're replacing the node or placing on top
		-- This condition is very similar to the check in core.check_single_for_falling(p)
		local np = vector.copy(bcp)
		if bcd and bcd.buildable_to
				and -- Take "float" group into consideration:
				(
					-- Fall through non-liquids
					not self.floats or bcd.liquidtype == "none" or
					-- Only let sources fall through flowing liquids
					(self.floats and self.liquidtype ~= "none" and bcd.liquidtype ~= "source")
				) then

			core.remove_node(bcp)
		else
			-- We are placing on top so check what's there
			np.y = np.y + 1

			local n2 = core.get_node(np)
			local nd = core.registered_nodes[n2.name]
			if not nd or nd.buildable_to then
				core.remove_node(np)
			else
				-- 'walkable' is used to mean "falling nodes can't replace this"
				-- here. Normally we would collide with the walkable node itself
				-- and place our node on top (so `n2.name == "air"`), but we
				-- re-check this in case we ended up inside a node.
				if not nd.diggable or nd.walkable then
					return false
				end
				nd.on_dig(np, n2, nil)
				-- If it's still there, it might be protected
				if core.get_node(np).name == n2.name then
					return false
				end
			end
		end

		-- Create node
		local def = core.registered_nodes[self.node.name]
		if def then
			core.add_node(np, self.node)
			if self.meta then
				core.get_meta(np):from_table(self.meta)
			end
			if def.sounds and def.sounds.place then
				core.sound_play(def.sounds.place, {pos = np}, true)
			end
		end
		core.check_for_falling(np)
		return true
	end,

	on_step = function(self, dtime, moveresult)
		-- Fallback code since collision detection can't tell us
		-- about liquids (which do not collide)
		if self.floats then
			local pos = self.object:get_pos()

			local bcp = pos:offset(0, -0.7, 0):round()
			local bcn = core.get_node(bcp)

			local bcd = core.registered_nodes[bcn.name]
			if bcd and bcd.liquidtype ~= "none" then
				if self:try_place(bcp, bcn) then
					self.object:remove()
					return
				end
			end
		end

		assert(moveresult)
		if not moveresult.collides then
			return -- Nothing to do :)
		end

		local bcp, bcn
		local player_collision
		if moveresult.touching_ground then
			for _, info in ipairs(moveresult.collisions) do
				if info.type == "object" then
					if info.axis == "y" and info.object:is_player() then
						player_collision = info
					end
				elseif info.axis == "y" then
					bcp = info.node_pos
					bcn = core.get_node(bcp)
					break
				end
			end
		end

		if not bcp then
			-- We're colliding with something, but not the ground. Irrelevant to us.
			if player_collision then
				-- Continue falling through players by moving a little into
				-- their collision box
				-- TODO: this hack could be avoided in the future if objects
				--       could choose who to collide with
				local vel = self.object:get_velocity()
				self.object:set_velocity(vector.new(
					vel.x,
					player_collision.old_velocity.y,
					vel.z
				))
				self.object:set_pos(self.object:get_pos():offset(0, -0.5, 0))
			end
			return
		elseif bcn.name == "ignore" then
			-- Delete on contact with ignore at world edges
			self.object:remove()
			return
		end

		local failure = false

		local pos = self.object:get_pos()
		local distance = vector.apply(vector.subtract(pos, bcp), math.abs)
		if distance.x >= 1 or distance.z >= 1 then
			-- We're colliding with some part of a node that's sticking out
			-- Since we don't want to visually teleport, drop as item
			failure = true
		elseif distance.y >= 2 then
			-- Doors consist of a hidden top node and a bottom node that is
			-- the actual door. Despite the top node being solid, the moveresult
			-- almost always indicates collision with the bottom node.
			-- Compensate for this by checking the top node
			bcp.y = bcp.y + 1
			bcn = core.get_node(bcp)
			local def = core.registered_nodes[bcn.name]
			if not (def and def.walkable) then
				failure = true -- This is unexpected, fail
			end
		end

		-- Try to actually place ourselves
		if not failure then
			failure = not self:try_place(bcp, bcn)
		end

		if failure then
			local drops = core.get_node_drops(self.node, "")
			for _, item in pairs(drops) do
				core.add_item(pos, item)
			end
		end
		self.object:remove()
	end
})

local function convert_to_falling_node(pos, node)
	local obj = core.add_entity(pos, "__builtin:falling_node")
	if not obj then
		return false
	end
	-- remember node level, the entities' set_node() uses this
	node.level = core.get_node_level(pos)
	local meta = core.get_meta(pos)
	local metatable = meta and meta:to_table() or {}

	local def = core.registered_nodes[node.name]
	if def and def.sounds and def.sounds.fall then
		core.sound_play(def.sounds.fall, {pos = pos}, true)
	end

	obj:get_luaentity():set_node(node, metatable)
	core.remove_node(pos)
	return true, obj
end

function core.spawn_falling_node(pos)
	local node = core.get_node(pos)
	if node.name == "air" or node.name == "ignore" then
		return false
	end
	return convert_to_falling_node(pos, node)
end

local function drop_attached_node(p)
	local n = core.get_node(p)
	local drops = core.get_node_drops(n, "")
	local def = core.registered_items[n.name]
	if def and def.preserve_metadata then
		local oldmeta = core.get_meta(p):to_table().fields
		-- Copy pos and node because the callback can modify them.
		local pos_copy = vector.copy(p)
		local node_copy = {name=n.name, param1=n.param1, param2=n.param2}
		local drop_stacks = {}
		for k, v in pairs(drops) do
			drop_stacks[k] = ItemStack(v)
		end
		drops = drop_stacks
		def.preserve_metadata(pos_copy, node_copy, oldmeta, drops)
	end
	if def and def.sounds and def.sounds.fall then
		core.sound_play(def.sounds.fall, {pos = p}, true)
	end
	core.remove_node(p)
	for _, item in pairs(drops) do
		local pos = {
			x = p.x + math.random()/2 - 0.25,
			y = p.y + math.random()/2 - 0.25,
			z = p.z + math.random()/2 - 0.25,
		}
		core.add_item(pos, item)
	end
end

function builtin_shared.check_attached_node(p, n, group_rating)
	local def = core.registered_nodes[n.name]
	local d = vector.zero()
	if group_rating == 3 then
		-- always attach to floor
		d.y = -1
	elseif group_rating == 4 then
		-- always attach to ceiling
		d.y = 1
	elseif group_rating == 2 then
		-- attach to facedir or 4dir direction
		if (def.paramtype2 == "facedir" or
				def.paramtype2 == "colorfacedir") then
			-- Attach to whatever facedir is "mounted to".
			-- For facedir, this is where tile no. 5 point at.

			-- The fallback vector here is in case 'facedir to dir' is nil due
			-- to voxelmanip placing a wallmounted node without resetting a
			-- pre-existing param2 value that is out-of-range for facedir.
			-- The fallback vector corresponds to param2 = 0.
			d = core.facedir_to_dir(n.param2) or vector.new(0, 0, 1)
		elseif (def.paramtype2 == "4dir" or
				def.paramtype2 == "color4dir") then
			-- Similar to facedir handling
			d = core.fourdir_to_dir(n.param2) or vector.new(0, 0, 1)
		end
	elseif def.paramtype2 == "wallmounted" or
			def.paramtype2 == "colorwallmounted" then
		-- Attach to whatever this node is "mounted to".
		-- This where tile no. 2 points at.

		-- The fallback vector here is used for the same reason as
		-- for facedir nodes.
		d = core.wallmounted_to_dir(n.param2) or vector.new(0, 1, 0)
	else
		d.y = -1
	end
	local p2 = vector.add(p, d)
	local nn = core.get_node(p2).name
	local def2 = core.registered_nodes[nn]
	if def2 and not def2.walkable then
		return false
	end
	return true
end

--
-- Some common functions
--

function core.check_single_for_falling(p)
	local n = core.get_node(p)
	if core.get_item_group(n.name, "falling_node") ~= 0 then
		local p_bottom = vector.offset(p, 0, -1, 0)
		-- Only spawn falling node if node below is loaded
		local n_bottom = core.get_node_or_nil(p_bottom)
		local d_bottom = n_bottom and core.registered_nodes[n_bottom.name]
		if d_bottom then
			local same = n.name == n_bottom.name
			-- Let leveled nodes fall if it can merge with the bottom node
			if same and d_bottom.paramtype2 == "leveled" and
					core.get_node_level(p_bottom) <
					core.get_node_max_level(p_bottom) then
				local success, _ = convert_to_falling_node(p, n)
				return success
			end
			local d_falling = core.registered_nodes[n.name]
			local do_float = core.get_item_group(n.name, "float") > 0
			-- Otherwise only if the bottom node is considered "fall through"
			if not same and
					(not d_bottom.walkable or d_bottom.buildable_to)
					and -- Take "float" group into consideration:
					(
						-- Fall through non-liquids
						not do_float or d_bottom.liquidtype == "none" or
						-- Only let sources fall through flowing liquids
						(do_float and d_falling.liquidtype == "source" and d_bottom.liquidtype ~= "source")
					) then

				local success, _ = convert_to_falling_node(p, n)
				return success
			end
		end
	end

	local an = core.get_item_group(n.name, "attached_node")
	if an ~= 0 then
		if not builtin_shared.check_attached_node(p, n, an) then
			drop_attached_node(p)
			return true
		end
	end

	return false
end

-- This table is specifically ordered.
-- We don't walk diagonals, only our direct neighbors, and self.
-- Down first as likely case, but always before self. The same with sides.
-- Up must come last, so that things above self will also fall all at once.
local check_for_falling_neighbors = {
	vector.new(-1, -1,  0),
	vector.new( 1, -1,  0),
	vector.new( 0, -1, -1),
	vector.new( 0, -1,  1),
	vector.new( 0, -1,  0),
	vector.new(-1,  0,  0),
	vector.new( 1,  0,  0),
	vector.new( 0,  0,  1),
	vector.new( 0,  0, -1),
	vector.new( 0,  0,  0),
	vector.new( 0,  1,  0),
}

function core.check_for_falling(p)
	-- Round p to prevent falling entities to get stuck.
	p = vector.round(p)

	-- We make a stack, and manually maintain size for performance.
	-- Stored in the stack, we will maintain tables with pos, and
	-- last neighbor visited. This way, when we get back to each
	-- node, we know which directions we have already walked, and
	-- which direction is the next to walk.
	local s = {}
	local n = 0
	-- The neighbor order we will visit from our table.
	local v = 1

	while true do
		-- Push current pos onto the stack.
		n = n + 1
		s[n] = {p = p, v = v}
		-- Select next node from neighbor list.
		p = vector.add(p, check_for_falling_neighbors[v])
		-- Now we check out the node. If it is in need of an update,
		-- it will let us know in the return value (true = updated).
		if not core.check_single_for_falling(p) then
			-- If we don't need to "recurse" (walk) to it then pop
			-- our previous pos off the stack and continue from there,
			-- with the v value we were at when we last were at that
			-- node
			repeat
				local pop = s[n]
				p = pop.p
				v = pop.v
				s[n] = nil
				n = n - 1
				-- If there's nothing left on the stack, and no
				-- more sides to walk to, we're done and can exit
				if n == 0 and v == 11 then
					return
				end
			until v < 11
			-- The next round walk the next neighbor in list.
			v = v + 1
		else
			-- If we did need to walk the neighbor, then
			-- start walking it from the walk order start (1),
			-- and not the order we just pushed up the stack.
			v = 1
		end
	end
end

--
-- Global callbacks
--

local function on_placenode(p, node)
	core.check_for_falling(p)
end
core.register_on_placenode(on_placenode)

local function on_dignode(p, node)
	core.check_for_falling(p)
end
core.register_on_dignode(on_dignode)

local function on_punchnode(p, node)
	core.check_for_falling(p)
end
core.register_on_punchnode(on_punchnode)
