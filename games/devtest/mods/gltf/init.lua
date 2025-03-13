local function register_entity(name, textures, backface_culling)
	core.register_entity("gltf:" .. name, {
		initial_properties = {
			visual = "mesh",
			mesh = "gltf_" .. name .. ".gltf",
			textures = textures,
			backface_culling = backface_culling,
		},
	})
end

-- These do not have texture coordinates; they simple render as black surfaces.
register_entity("minimal_triangle", {}, false)
register_entity("triangle_with_vertex_stride", {}, false)
register_entity("triangle_without_indices", {}, false)
do
	local cube_textures = {"gltf_cube.png"}
	register_entity("blender_cube", cube_textures)
	register_entity("blender_cube_scaled", cube_textures)
	register_entity("blender_cube_matrix_transform", cube_textures)
	core.register_entity("gltf:blender_cube_glb", {
		initial_properties = {
			visual = "mesh",
			mesh = "gltf_blender_cube.glb",
			textures = cube_textures,
			backface_culling = true,
		},
	})
end

register_entity("snow_man", {"gltf_snow_man.png"})
register_entity("spider", {"gltf_spider.png"})

core.register_entity("gltf:spider_animated", {
	initial_properties = {
		visual = "mesh",
		mesh = "gltf_spider_animated.gltf",
		textures = {"gltf_spider.png"},
	},
	on_activate = function(self)
		self.object:set_animation({x = 0, y = 140}, 1)
	end
})

core.register_entity("gltf:simple_skin", {
	initial_properties = {
		visual = "mesh",
		visual_size = vector.new(5, 5, 5),
		mesh = "gltf_simple_skin.gltf",
		textures = {},
		backface_culling = false
	},
	on_activate = function(self)
		self.object:set_animation({x = 0, y = 5.5}, 1)
	end
})

core.register_entity("gltf:simple_skin_step", {
	initial_properties = {
		infotext = "Simple skin, but using STEP interpolation",
		visual = "mesh",
		visual_size = vector.new(5, 5, 5),
		mesh = "gltf_simple_skin_step.gltf",
		textures = {},
		backface_culling = false
	},
	on_activate = function(self)
		self.object:set_animation({x = 0, y = 5.5}, 1)
	end
})

-- The claws rendering incorrectly from one side is expected behavior:
-- They use an unsupported double-sided material.
core.register_entity("gltf:frog", {
	initial_properties = {
		visual = "mesh",
		mesh = "gltf_frog.gltf",
		textures = {"gltf_frog.png"},
		backface_culling = false
	},
	on_activate = function(self)
		self.object:set_animation({x = 0, y = 0.75}, 1)
	end
})


core.register_node("gltf:frog", {
	description = "glTF frog, but it's a node",
	tiles = {{name = "gltf_frog.png", backface_culling = false}},
	drawtype = "mesh",
	mesh = "gltf_frog.gltf",
})

core.register_chatcommand("show_model", {
	params = "<model> [textures]",
	description = "Show a model (defaults to gltf models, for example '/show_model frog').",
	func = function(name, param)
		local model, textures = param:match"^(.-)%s+(.+)$"
		if not model then
			model = "gltf_" .. param .. ".gltf"
			textures = "gltf_" .. param .. ".png"
		end
		core.show_formspec(name, "gltf:model", table.concat{
			"formspec_version[7]",
			"size[10,10]",
			"model[0,0;10,10;model;", model, ";", textures, ";0,0;true;true;0,0;0]",
		})
	end,
})
