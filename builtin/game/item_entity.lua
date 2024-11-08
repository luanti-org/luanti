function core.spawn_item(pos, item)
	-- Take item in any format
	local stack = ItemStack(item)
	local obj = core.add_entity(pos, "__builtin:item")
	-- Don't use obj if it couldn't be added to the map.
	if obj then
		obj:get_luaentity():set_item(stack:to_string())
	end
	return obj
end

-- If item_entity_ttl is not set, enity will have default life time
-- Setting it to -1 disables the feature

local time_to_live = tonumber(core.settings:get("item_entity_ttl")) or 900
local gravity = tonumber(core.settings:get("movement_gravity")) or 9.81


core.register_entity(":__builtin:item", {
	initial_properties = {
		hp_max = 1,
		physical = true,
		collide_with_objects = false,
		collisionbox = {-0.3, -0.3, -0.3, 0.3, 0.3, 0.3},
		visual = "wielditem",
		visual_size = {x = 0.4, y = 0.4},
		textures = {""},
		is_visible = false,
	},

	itemstring = "",
	moving_state = true,
	physical_state = true,
	-- Item expiry
	age = 0,
	-- Pushing item out of solid nodes
	force_out = nil,
	force_out_start = nil,

	set_item = function(self, item)
		local stack = ItemStack(item or self.itemstring)
		self.itemstring = stack:to_string()
		if self.itemstring == "" then
			-- item not yet known
			return
		end

		-- Backwards compatibility: old clients use the texture
		-- to get the type of the item
		local itemname = stack:is_known() and stack:get_name() or "unknown"

		local max_count = stack:get_stack_max()
		local count = math.min(stack:get_count(), max_count)
		local size = 0.2 + 0.1 * (count / max_count) ^ (1 / 3)
		local def = core.registered_items[itemname]
		local glow = def and def.light_source and
			math.floor(def.light_source / 2 + 0.5)

		local size_bias = 1e-3 * math.random() -- small random bias to counter Z-fighting
		local c = {-size, -size, -size, size, size, size}
		self.object:set_properties({
			is_visible = true,
			visual = "wielditem",
			textures = {itemname},
			visual_size = {x = size + size_bias, y = size + size_bias},
			collisionbox = c,
			automatic_rotate = math.pi * 0.5 * 0.2 / size,
			wield_item = self.itemstring,
			glow = glow,
			infotext = stack:get_description(),
		})

		-- cache for usage in on_step
		self._collisionbox = c
	end,

	get_staticdata = function(self)
		return core.serialize({
			itemstring = self.itemstring,
			age = self.age,
			dropped_by = self.dropped_by
		})
	end,

	on_activate = function(self, staticdata, dtime_s)
		if string.sub(staticdata, 1, string.len("return")) == "return" then
			local data = core.deserialize(staticdata)
			if data and type(data) == "table" then
				self.itemstring = data.itemstring
				self.age = (data.age or 0) + dtime_s
				self.dropped_by = data.dropped_by
			end
		else
			self.itemstring = staticdata
		end
		self.object:set_armor_groups({immortal = 1})
		self.object:set_velocity({x = 0, y = 2, z = 0})
		self.object:set_acceleration({x = 0, y = -gravity, z = 0})
		self._collisionbox = self.initial_properties.collisionbox
		self:set_item()
	end,

	try_merge_with = function(self, own_stack, object, entity)
		if self.age == entity.age then
			-- Cannot merge with itself
			return false
		end

		local stack = ItemStack(entity.itemstring)
		local name = stack:get_name()
		if own_stack:get_name() ~= name or
				own_stack:get_meta() ~= stack:get_meta() or
				own_stack:get_wear() ~= stack:get_wear() or
				own_stack:get_free_space() == 0 then
			-- Cannot merge different or full stack
			return false
		end

		local count = own_stack:get_count()
		local total_count = stack:get_count() + count
		local max_count = stack:get_stack_max()

		if total_count > max_count then
			return false
		end
		-- Merge the remote stack into this one

		local pos = object:get_pos()
		pos.y = pos.y + ((total_count - count) / max_count) * 0.15
		self.object:move_to(pos)

		self.age = 0 -- Handle as new entity
		own_stack:set_count(total_count)
		self:set_item(own_stack)

		entity.itemstring = ""
		object:remove()
		return true
	end,

	enable_physics = function(self)
		if not self.physical_state then
			self.physical_state = true
			self.object:set_properties({physical = true})
			self.object:set_velocity({x=0, y=0, z=0})
			self.object:set_acceleration({x=0, y=-gravity, z=0})
		end
	end,

	disable_physics = function(self)
		if self.physical_state then
			self.physical_state = false
			self.object:set_properties({physical = false})
			self.object:set_velocity({x=0, y=0, z=0})
			self.object:set_acceleration({x=0, y=0, z=0})
		end
	end,

	on_step = function(self, dtime, moveresult)
		self.age = self.age + dtime
		if time_to_live > 0 and self.age > time_to_live then
			self.itemstring = ""
			self.object:remove()
			return
		end

		local pos = self.object:get_pos()
		local node = core.get_node_or_nil({
			x = pos.x,
			y = pos.y + self._collisionbox[2] - 0.05,
			z = pos.z
		})
		-- Delete in 'ignore' nodes
		if node and node.name == "ignore" then
			self.itemstring = ""
			self.object:remove()
			return
		end

		-- Prevent assert when item_entity is attached
		if moveresult == nil and self.object:get_attach() then
			return
		end

		if self.force_out then
			-- This code runs after the entity got a push from the is_stuck code.
			-- It makes sure the entity is entirely outside the solid node
			local c = self._collisionbox
			local s = self.force_out_start
			local f = self.force_out
			local ok = (f.x > 0 and pos.x + c[1] > s.x + 0.5) or
				(f.y > 0 and pos.y + c[2] > s.y + 0.5) or
				(f.z > 0 and pos.z + c[3] > s.z + 0.5) or
				(f.x < 0 and pos.x + c[4] < s.x - 0.5) or
				(f.z < 0 and pos.z + c[6] < s.z - 0.5)
			if ok then
				-- Item was successfully forced out
				self.force_out = nil
				self:enable_physics()
				return
			end
		end

		if not self.physical_state then
			return -- Don't do anything
		end

		assert(moveresult,
			"Collision info missing, this is caused by an out-of-date/buggy mod or game")

		if not moveresult.collides then
			-- future TODO: items should probably decelerate in air
			return
		end

		-- Push item out when stuck inside solid node
		local is_stuck = false
		local snode = core.get_node_or_nil(pos)
		if snode then
			local sdef = core.registered_nodes[snode.name] or {}
			is_stuck = (sdef.walkable == nil or sdef.walkable == true)
				and (sdef.collision_box == nil or sdef.collision_box.type == "regular")
				and (sdef.node_box == nil or sdef.node_box.type == "regular")
		end

		if is_stuck then
			local shootdir
			local order = {
				{x=1, y=0, z=0}, {x=-1, y=0, z= 0},
				{x=0, y=0, z=1}, {x= 0, y=0, z=-1},
			}

			-- Check which one of the 4 sides is free
			for o = 1, #order do
				local cnode = core.get_node(vector.add(pos, order[o])).name
				local cdef = core.registered_nodes[cnode] or {}
				if cnode ~= "ignore" and cdef.walkable == false then
					shootdir = order[o]
					break
				end
			end
			-- If none of the 4 sides is free, check upwards
			if not shootdir then
				shootdir = {x=0, y=1, z=0}
				local cnode = core.get_node(vector.add(pos, shootdir)).name
				if cnode == "ignore" then
					shootdir = nil -- Do not push into ignore
				end
			end

			if shootdir then
				-- Set new item moving speed accordingly
				local newv = vector.multiply(shootdir, 3)
				self:disable_physics()
				self.object:set_velocity(newv)

				self.force_out = newv
				self.force_out_start = vector.round(pos)
				return
			end
		end

		node = nil -- ground node we're colliding with
		if moveresult.touching_ground then
			for _, info in ipairs(moveresult.collisions) do
				if info.axis == "y" then
					node = core.get_node(info.node_pos)
					break
				end
			end
		end

		-- Slide on slippery nodes
		local def = node and core.registered_nodes[node.name]
		local keep_movement = false

		if def then
			local slippery = core.get_item_group(node.name, "slippery")
			local vel = self.object:get_velocity()
			if slippery ~= 0 and (math.abs(vel.x) > 0.1 or math.abs(vel.z) > 0.1) then
				-- Horizontal deceleration
				local factor = math.min(4 / (slippery + 4) * dtime, 1)
				self.object:set_velocity({
					x = vel.x * (1 - factor),
					y = 0,
					z = vel.z * (1 - factor)
				})
				keep_movement = true
			end
		end

		if not keep_movement then
			self.object:set_velocity({x=0, y=0, z=0})
		end

		if self.moving_state == keep_movement then
			-- Do not update anything until the moving state changes
			return
		end
		self.moving_state = keep_movement

		-- Only collect items if not moving
		if self.moving_state then
			return
		end
		-- Collect the items around to merge with
		local own_stack = ItemStack(self.itemstring)
		if own_stack:get_free_space() == 0 then
			return
		end
		local objects = core.get_objects_inside_radius(pos, 1.0)
		for k, obj in pairs(objects) do
			local entity = obj:get_luaentity()
			if entity and entity.name == "__builtin:item" then
				if self:try_merge_with(own_stack, obj, entity) then
					own_stack = ItemStack(self.itemstring)
					if own_stack:get_free_space() == 0 then
						return
					end
				end
			end
		end
	end,

	on_punch = function(self, hitter, ...)
		if self.itemstring == "" then
			self.object:remove()
			return
		end

		-- Call on_pickup callback in item definition.
		local itemstack = ItemStack(self.itemstring)
		local callback = itemstack:get_definition().on_pickup

		local ret = callback(itemstack, hitter, {type = "object", ref = self.object}, ...)
		if not ret then
			-- Don't modify (and don't reset rotation)
			return
		end
		itemstack = ItemStack(ret)

		-- Handle the leftover itemstack
		if itemstack:is_empty() then
			self.itemstring = ""
			self.object:remove()
		else
			self:set_item(itemstack)
		end
	end,
})
