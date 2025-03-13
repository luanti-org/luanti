--
-- Item callbacks
--

local function print_to_everything(msg)
	core.log("action", "[callbacks] " .. msg)
	core.chat_send_all(msg)
end

core.register_craftitem("callbacks:callback_item_1", {
	description = "Callback Test Item 1".."\n"..
		"Tests callbacks: on_secondary_use, on_drop, on_pickup, on_use, after_use".."\n"..
		"Punch/Drop + Sneak: Switch to Callback Test Item 2".."\n"..
		"Aux1 + pickup item: Print additional on_pickup arguments",
	inventory_image = "callbacks_callback_item_1.png",
	wield_image = "callbacks_callback_item_1.png",
	groups = { callback_test = 1 },

	on_secondary_use = function(itemstack, user, pointed_thing)
		print_to_everything("[callbacks:callback_item_1 on_secondary_use] " .. itemstack:get_name())
		local ctrl = user and user:get_player_control() or {}
		if ctrl.sneak then
			itemstack = ItemStack(itemstack)
			itemstack:set_name("callbacks:callback_item_2")
			return itemstack
		end
	end,

	on_drop = function(itemstack, dropper, pos)
		print_to_everything("[callbacks:callback_item_1 on_drop] " .. itemstack:get_name())
		local ctrl = dropper and dropper:get_player_control() or {}
		if ctrl.sneak then
			itemstack = ItemStack(itemstack)
			itemstack:set_name("callbacks:callback_item_2")
		end

		return core.item_drop(itemstack, dropper, pos)
	end,

	on_pickup = function(itemstack, picker, pointed_thing, ...)
		print_to_everything("[callbacks:callback_item_1 on_pickup]")
		assert(pointed_thing.ref:get_luaentity().name == "__builtin:item")
		local ctrl = picker and picker:get_player_control() or {}
		if ctrl.aux1 then
			-- Debug message
			print_to_everything("on_pickup dump:")
			print_to_everything(dump({...}))
		end
		if ctrl.sneak then
			-- Pick up one item of the other kind at once
			local taken = itemstack:take_item()
			taken:set_name("callbacks:callback_item_2")
			local leftover = core.item_pickup(taken, picker, pointed_thing, ...)
			leftover:set_name("callbacks:callback_item_1")
			itemstack:add_item(leftover)
			return itemstack
		elseif ctrl.up then
			-- Don't pick up
			return
		elseif ctrl.left then
			-- Eat it
			return core.do_item_eat(2, nil, itemstack, picker, pointed_thing)
		else
			-- Normal: pick up everything
			return core.item_pickup(itemstack, picker, pointed_thing, ...)
		end
	end,

	on_use = function(itemstack, user, pointed_thing)
		print_to_everything("[callbacks:callback_item_1 on_use] " .. itemstack:get_name())
		local ctrl = user and user:get_player_control() or {}
		if ctrl.sneak then
			itemstack = ItemStack(itemstack)
			itemstack:set_name("callbacks:callback_item_2")
			return itemstack
		end
	end,

	after_use = function(itemstack, user, node, digparams) -- never called
		print_to_everything("[callbacks:callback_item_1 after_use]")
		local ctrl = user and user:get_player_control() or {}
		if ctrl.up then
			itemstack = ItemStack(itemstack)
			itemstack:set_name("callbacks:callback_item_2")
			return itemstack
		end
	end,
})

core.register_craftitem("callbacks:callback_item_2", {
	description = "Callback Test Item 2".."\n"..
		"Punch to switch to Callback Test Item 1",
	inventory_image = "callbacks_callback_item_2.png",
	wield_image = "callbacks_callback_item_2.png",
	groups = { callback_test = 1 },

	on_use = function(itemstack, user, pointed_thing)
		print_to_everything("[callbacks:callback_item_2 on_use]")
		itemstack = ItemStack(itemstack)
		itemstack:set_name("callbacks:callback_item_1")
		return itemstack
	end,
})

core.register_on_item_pickup(function(itemstack, picker, pointed_thing, time_from_last_punch, ...)
	assert(not pointed_thing or pointed_thing.ref:get_luaentity().name == "__builtin:item")

	local item_name = itemstack:get_name()
	if item_name ~= "callbacks:callback_item_1" and item_name ~= "callbacks:callback_item_2" then
		return
	end
	print_to_everything("["..item_name.." register_on_item_pickup]")

	local ctrl = picker and picker:get_player_control() or {}
	if item_name == "callbacks:callback_item_2" and not ctrl.sneak then
		-- Same here. Pick up the other item type.
		itemstack:set_name("callbacks:callback_item_1")
		return picker:get_inventory():add_item("main", itemstack)
	end
end)
