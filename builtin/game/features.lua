core.features = {
	glasslike_framed = true,
	nodebox_as_selectionbox = true,
	get_all_craft_recipes_works = true,
	use_texture_alpha = true,
	no_legacy_abms = true,
	texture_names_parens = true,
	area_store_custom_ids = true,
	add_entity_with_staticdata = true,
	no_chat_message_prediction = true,
	object_use_texture_alpha = true,
	object_independent_selectionbox = true,
	httpfetch_binary_data = true,
	formspec_version_element = true,
	area_store_persistent_ids = true,
	pathfinder_works = true,
	object_step_has_moveresult = true,
	direct_velocity_on_players = true,
	use_texture_alpha_string_modes = true,
	degrotate_240_steps = true,
	abm_min_max_y = true,
	particlespawner_tweenable = true,
	dynamic_add_media_table = true,
	get_sky_as_table = true,
	get_light_data_buffer = true,
	mod_storage_on_disk = true,
	compress_zstd = true,
	sound_params_start_time = true,
	physics_overrides_v2 = true,
	hud_def_type_field = true,
	random_state_restore = true,
	after_order_expiry_registration = true,
	wallmounted_rotate = true,
	item_specific_pointabilities = true,
	blocking_pointability_type = true,
	dynamic_add_media_startup = true,
	dynamic_add_media_filepath = true,
	lsystem_decoration_type = true,
	item_meta_range = true,
	node_interaction_actor = true,
	moveresult_new_pos = true,
	override_item_remove_fields = true,
	hotbar_hud_element = true,
	bulk_lbms = true,
	abm_without_neighbors = true,
	biome_weights = true,
	particle_blend_clip = true,
	remove_item_match_meta = true,
	httpfetch_additional_methods = true,
	object_guids = true,
	on_timer_four_args = true,
	particlespawner_exclude_player = true,
	generate_decorations_biomes = true,
}

function core.has_feature(arg)
	if type(arg) == "table" then
		local missing_features = {}
		local result = true
		for ftr in pairs(arg) do
			if not core.features[ftr] then
				missing_features[ftr] = true
				result = false
			end
		end
		return result, missing_features
	elseif type(arg) == "string" then
		if not core.features[arg] then
			return false, {[arg]=true}
		end
		return true, {}
	end
end
