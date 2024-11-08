local S = core.get_translator("testtools")

core.register_entity("testtools:visual_box", {
	initial_properties = {
		visual = "cube",
		textures = {
			"blank.png", "blank.png", "blank.png",
			"blank.png", "blank.png", "blank.png",
		},
		use_texture_alpha = true,
		physical = false,
		pointable = false,
		static_save = false,
	},

	on_activate = function(self)
		self.timestamp = core.get_us_time() + 5000000
	end,

	on_step = function(self)
		if core.get_us_time() >= self.timestamp then
			self.object:remove()
		end
	end,
})

local BOX_TYPES = {"node_box", "collision_box", "selection_box"}
local DEFAULT_BOX_TYPE = "selection_box"

local function visualizer_on_use(itemstack, user, pointed_thing)
	if pointed_thing.type ~= "node" then
		return
	end

	local meta = itemstack:get_meta()
	local box_type = meta:get("box_type") or DEFAULT_BOX_TYPE

	local result = core.get_node_boxes(box_type, pointed_thing.under)
	local t = "testtools_visual_" .. box_type .. ".png"

	for _, box in ipairs(result) do
		local box_min = pointed_thing.under + vector.new(box[1], box[2], box[3])
		local box_max = pointed_thing.under + vector.new(box[4], box[5], box[6])
		local box_center = (box_min + box_max) / 2
		local obj = core.add_entity(box_center, "testtools:visual_box")
		if not obj then
			break
		end
		obj:set_properties({
			textures = {t, t, t, t, t, t},
			-- Add a small offset to avoid Z-fighting.
			visual_size = vector.add(box_max - box_min, 0.01),
		})
	end
end

local function visualizer_on_place(itemstack, placer, pointed_thing)
	local meta = itemstack:get_meta()
	local prev_value = meta:get("box_type") or DEFAULT_BOX_TYPE
	local prev_index = table.indexof(BOX_TYPES, prev_value)
	assert(prev_index ~= -1)

	local new_value = BOX_TYPES[(prev_index % #BOX_TYPES) + 1]
	meta:set_string("box_type", new_value)
	core.chat_send_player(placer:get_player_name(), S("[Node Box Visualizer] box_type = @1", new_value))

	return itemstack
end

core.register_tool("testtools:node_box_visualizer", {
	description = S("Node Box Visualizer") .. "\n" ..
		S("Punch: Show node/collision/selection boxes of the pointed node") .. "\n" ..
		S("Place: Change selected box type (default: selection box)"),
	inventory_image = "testtools_node_box_visualizer.png",
	groups = { testtool = 1, disable_repair = 1 },
	on_use = visualizer_on_use,
	on_place = visualizer_on_place,
	on_secondary_use = visualizer_on_place,
})
