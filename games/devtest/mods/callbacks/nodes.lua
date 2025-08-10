local function print_to_everything(msg)
	core.log("action", "[callbacks] " .. msg)
	core.chat_send_all(msg)
end

core.register_node("callbacks:callback_node", {
	description = "Callback Test Node (construct/destruct/timer)".."\n"..
		"Tests callbacks: on_construct, after_place_node, on_destruct, after_destruct, after_dig_node, on_timer",
	tiles = {"callbacks_callback_node.png"},
	groups = {callback_test=1, dig_immediate=3},
	-- This was known to cause a bug in core.item_place_node() when used
	-- via core.place_node(), causing a placer with no position
	paramtype2 = "facedir",
	drop = "",

	on_construct = function(pos)
		print_to_everything("callbacks:callback_node:on_construct("..core.pos_to_string(pos)..")")
		local meta = core.get_meta(pos)
		meta:set_string("mine", "test")
		local timer = core.get_node_timer(pos)
		timer:start(4, 3)
	end,

	after_place_node = function(pos, placer)
		print_to_everything("callbacks:callback_node:after_place_node("..core.pos_to_string(pos)..")")
		local meta = core.get_meta(pos)
		if meta:get_string("mine") == "test" then
			print_to_everything("correct metadata found")
		else
			print_to_everything("incorrect metadata found")
		end
	end,

	on_destruct = function(pos)
		print_to_everything("callbacks:callback_node:on_destruct("..core.pos_to_string(pos)..")")
	end,

	after_destruct = function(pos)
		print_to_everything("callbacks:callback_node:after_destruct("..core.pos_to_string(pos)..")")
	end,

	after_dig_node = function(pos, oldnode, oldmetadata, digger)
		print_to_everything("callbacks:callback_node:after_dig_node("..core.pos_to_string(pos)..")")
	end,

	on_timer = function(pos, elapsed, node, timeout)
		print_to_everything("callbacks:callback_node:on_timer(): elapsed="..
			elapsed .. " node=" .. dump(node) .. " timeout=" .. timeout)
		return true
	end,
})

