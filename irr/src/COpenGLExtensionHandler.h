// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "EDriverFeatures.h"
#include "irrTypes.h"
#include "os.h"

#include "COpenGLCommon.h"

#include "COpenGLCoreFeature.h"

namespace video
{

class IContextManager;

static const char *const OpenGLFeatureStrings[] = {
		"GL_3DFX_multisample",
		"GL_3DFX_tbuffer",
		"GL_3DFX_texture_compression_FXT1",
		"GL_AMD_blend_minmax_factor",
		"GL_AMD_conservative_depth",
		"GL_AMD_debug_output",
		"GL_AMD_depth_clamp_separate",
		"GL_AMD_draw_buffers_blend",
		"GL_AMD_interleaved_elements",
		"GL_AMD_multi_draw_indirect",
		"GL_AMD_name_gen_delete",
		"GL_AMD_performance_monitor",
		"GL_AMD_pinned_memory",
		"GL_AMD_query_buffer_object",
		"GL_AMD_sample_positions",
		"GL_AMD_seamless_cubemap_per_texture",
		"GL_AMD_shader_atomic_counter_ops",
		"GL_AMD_shader_stencil_export",
		"GL_AMD_shader_trinary_minmax",
		"GL_AMD_sparse_texture",
		"GL_AMD_stencil_operation_extended",
		"GL_AMD_texture_texture4",
		"GL_AMD_transform_feedback3_lines_triangles",
		"GL_AMD_vertex_shader_layer",
		"GL_AMD_vertex_shader_tessellator",
		"GL_AMD_vertex_shader_viewport_index",
		"GL_APPLE_aux_depth_stencil",
		"GL_APPLE_client_storage",
		"GL_APPLE_element_array",
		"GL_APPLE_fence",
		"GL_APPLE_float_pixels",
		"GL_APPLE_flush_buffer_range",
		"GL_APPLE_object_purgeable",
		"GL_APPLE_rgb_422",
		"GL_APPLE_row_bytes",
		"GL_APPLE_specular_vector",
		"GL_APPLE_texture_range",
		"GL_APPLE_transform_hint",
		"GL_APPLE_vertex_array_object",
		"GL_APPLE_vertex_array_range",
		"GL_APPLE_vertex_program_evaluators",
		"GL_APPLE_ycbcr_422",
		"GL_ARB_arrays_of_arrays",
		"GL_ARB_base_instance",
		"GL_ARB_bindless_texture",
		"GL_ARB_blend_func_extended",
		"GL_ARB_buffer_storage",
		"GL_ARB_cl_event",
		"GL_ARB_clear_buffer_object",
		"GL_ARB_clear_texture",
		"GL_ARB_color_buffer_float",
		"GL_ARB_compatibility",
		"GL_ARB_compressed_texture_pixel_storage",
		"GL_ARB_compute_shader",
		"GL_ARB_compute_variable_group_size",
		"GL_ARB_conservative_depth",
		"GL_ARB_copy_buffer",
		"GL_ARB_copy_image",
		"GL_ARB_debug_output",
		"GL_ARB_depth_buffer_float",
		"GL_ARB_depth_clamp",
		"GL_ARB_depth_texture",
		"GL_ARB_direct_state_access",
		"GL_ARB_draw_buffers",
		"GL_ARB_draw_buffers_blend",
		"GL_ARB_draw_elements_base_vertex",
		"GL_ARB_draw_indirect",
		"GL_ARB_draw_instanced",
		"GL_ARB_ES2_compatibility",
		"GL_ARB_ES3_compatibility",
		"GL_ARB_enhanced_layouts",
		"GL_ARB_explicit_attrib_location",
		"GL_ARB_explicit_uniform_location",
		"GL_ARB_fragment_coord_conventions",
		"GL_ARB_fragment_layer_viewport",
		"GL_ARB_fragment_program",
		"GL_ARB_fragment_program_shadow",
		"GL_ARB_fragment_shader",
		"GL_ARB_framebuffer_no_attachments",
		"GL_ARB_framebuffer_object",
		"GL_ARB_framebuffer_sRGB",
		"GL_ARB_geometry_shader4",
		"GL_ARB_get_program_binary",
		"GL_ARB_gpu_shader5",
		"GL_ARB_gpu_shader_fp64",
		"GL_ARB_half_float_pixel",
		"GL_ARB_half_float_vertex",
		"GL_ARB_imaging",
		"GL_ARB_indirect_parameters",
		"GL_ARB_instanced_arrays",
		"GL_ARB_internalformat_query",
		"GL_ARB_internalformat_query2",
		"GL_ARB_invalidate_subdata",
		"GL_ARB_map_buffer_alignment",
		"GL_ARB_map_buffer_range",
		"GL_ARB_matrix_palette",
		"GL_ARB_multi_bind",
		"GL_ARB_multi_draw_indirect",
		"GL_ARB_multisample",
		"GL_ARB_multitexture",
		"GL_ARB_occlusion_query",
		"GL_ARB_occlusion_query2",
		"GL_ARB_pixel_buffer_object",
		"GL_ARB_point_parameters",
		"GL_ARB_point_sprite",
		"GL_ARB_program_interface_query",
		"GL_ARB_provoking_vertex",
		"GL_ARB_query_buffer_object",
		"GL_ARB_robust_buffer_access_behavior",
		"GL_ARB_robustness",
		"GL_ARB_robustness_isolation",
		"GL_ARB_sample_shading",
		"GL_ARB_sampler_objects",
		"GL_ARB_seamless_cube_map",
		"GL_ARB_seamless_cubemap_per_texture",
		"GL_ARB_separate_shader_objects",
		"GL_ARB_shader_atomic_counters",
		"GL_ARB_shader_bit_encoding",
		"GL_ARB_shader_draw_parameters",
		"GL_ARB_shader_group_vote",
		"GL_ARB_shader_image_load_store",
		"GL_ARB_shader_image_size",
		"GL_ARB_shader_objects",
		"GL_ARB_shader_precision",
		"GL_ARB_shader_stencil_export",
		"GL_ARB_shader_storage_buffer_object",
		"GL_ARB_shader_subroutine",
		"GL_ARB_shader_texture_lod",
		"GL_ARB_shading_language_100",
		"GL_ARB_shading_language_420pack",
		"GL_ARB_shading_language_include",
		"GL_ARB_shading_language_packing",
		"GL_ARB_shadow",
		"GL_ARB_shadow_ambient",
		"GL_ARB_sparse_texture",
		"GL_ARB_stencil_texturing",
		"GL_ARB_sync",
		"GL_ARB_tessellation_shader",
		"GL_ARB_texture_border_clamp",
		"GL_ARB_texture_buffer_object",
		"GL_ARB_texture_buffer_object_rgb32",
		"GL_ARB_texture_buffer_range",
		"GL_ARB_texture_compression",
		"GL_ARB_texture_compression_bptc",
		"GL_ARB_texture_compression_rgtc",
		"GL_ARB_texture_cube_map",
		"GL_ARB_texture_cube_map_array",
		"GL_ARB_texture_env_add",
		"GL_ARB_texture_env_combine",
		"GL_ARB_texture_env_crossbar",
		"GL_ARB_texture_env_dot3",
		"GL_ARB_texture_float",
		"GL_ARB_texture_gather",
		"GL_ARB_texture_mirror_clamp_to_edge",
		"GL_ARB_texture_mirrored_repeat",
		"GL_ARB_texture_multisample",
		"GL_ARB_texture_non_power_of_two",
		"GL_ARB_texture_query_levels",
		"GL_ARB_texture_query_lod",
		"GL_ARB_texture_rectangle",
		"GL_ARB_texture_rg",
		"GL_ARB_texture_rgb10_a2ui",
		"GL_ARB_texture_stencil8",
		"GL_ARB_texture_storage",
		"GL_ARB_texture_storage_multisample",
		"GL_ARB_texture_swizzle",
		"GL_ARB_texture_view",
		"GL_ARB_timer_query",
		"GL_ARB_transform_feedback2",
		"GL_ARB_transform_feedback3",
		"GL_ARB_transform_feedback_instanced",
		"GL_ARB_transpose_matrix",
		"GL_ARB_uniform_buffer_object",
		"GL_ARB_vertex_array_bgra",
		"GL_ARB_vertex_array_object",
		"GL_ARB_vertex_attrib_64bit",
		"GL_ARB_vertex_attrib_binding",
		"GL_ARB_vertex_blend",
		"GL_ARB_vertex_buffer_object",
		"GL_ARB_vertex_program",
		"GL_ARB_vertex_shader",
		"GL_ARB_vertex_type_10f_11f_11f_rev",
		"GL_ARB_vertex_type_2_10_10_10_rev",
		"GL_ARB_viewport_array",
		"GL_ARB_window_pos",
		"GL_ATI_draw_buffers",
		"GL_ATI_element_array",
		"GL_ATI_envmap_bumpmap",
		"GL_ATI_fragment_shader",
		"GL_ATI_map_object_buffer",
		"GL_ATI_meminfo",
		"GL_ATI_pixel_format_float",
		"GL_ATI_pn_triangles",
		"GL_ATI_separate_stencil",
		"GL_ATI_text_fragment_shader",
		"GL_ATI_texture_env_combine3",
		"GL_ATI_texture_float",
		"GL_ATI_texture_mirror_once",
		"GL_ATI_vertex_array_object",
		"GL_ATI_vertex_attrib_array_object",
		"GL_ATI_vertex_streams",
		"GL_EXT_422_pixels",
		"GL_EXT_abgr",
		"GL_EXT_bgra",
		"GL_EXT_bindable_uniform",
		"GL_EXT_blend_color",
		"GL_EXT_blend_equation_separate",
		"GL_EXT_blend_func_separate",
		"GL_EXT_blend_logic_op",
		"GL_EXT_blend_minmax",
		"GL_EXT_blend_subtract",
		"GL_EXT_clip_volume_hint",
		"GL_EXT_cmyka",
		"GL_EXT_color_subtable",
		"GL_EXT_compiled_vertex_array",
		"GL_EXT_convolution",
		"GL_EXT_coordinate_frame",
		"GL_EXT_copy_texture",
		"GL_EXT_cull_vertex",
		"GL_EXT_debug_label",
		"GL_EXT_debug_marker",
		"GL_EXT_depth_bounds_test",
		"GL_EXT_direct_state_access",
		"GL_EXT_draw_buffers2",
		"GL_EXT_draw_instanced",
		"GL_EXT_draw_range_elements",
		"GL_EXT_fog_coord",
		"GL_EXT_framebuffer_blit",
		"GL_EXT_framebuffer_multisample",
		"GL_EXT_framebuffer_multisample_blit_scaled",
		"GL_EXT_framebuffer_object",
		"GL_EXT_framebuffer_sRGB",
		"GL_EXT_geometry_shader4",
		"GL_EXT_gpu_program_parameters",
		"GL_EXT_gpu_shader4",
		"GL_EXT_histogram",
		"GL_EXT_index_array_formats",
		"GL_EXT_index_func",
		"GL_EXT_index_material",
		"GL_EXT_index_texture",
		"GL_EXT_light_texture",
		"GL_EXT_misc_attribute",
		"GL_EXT_multi_draw_arrays",
		"GL_EXT_multisample",
		"GL_EXT_packed_depth_stencil",
		"GL_EXT_packed_float",
		"GL_EXT_packed_pixels",
		"GL_EXT_paletted_texture",
		"GL_EXT_pixel_buffer_object",
		"GL_EXT_pixel_transform",
		"GL_EXT_pixel_transform_color_table",
		"GL_EXT_point_parameters",
		"GL_EXT_polygon_offset",
		"GL_EXT_provoking_vertex",
		"GL_EXT_rescale_normal",
		"GL_EXT_secondary_color",
		"GL_EXT_separate_shader_objects",
		"GL_EXT_separate_specular_color",
		"GL_EXT_shader_image_load_store",
		"GL_EXT_shader_integer_mix",
		"GL_EXT_shadow_funcs",
		"GL_EXT_shared_texture_palette",
		"GL_EXT_stencil_clear_tag",
		"GL_EXT_stencil_two_side",
		"GL_EXT_stencil_wrap",
		"GL_EXT_subtexture",
		"GL_EXT_texture",
		"GL_EXT_texture3D",
		"GL_EXT_texture_array",
		"GL_EXT_texture_buffer_object",
		"GL_EXT_texture_compression_latc",
		"GL_EXT_texture_compression_rgtc",
		"GL_EXT_texture_compression_s3tc",
		"GL_EXT_texture_cube_map",
		"GL_EXT_texture_env_add",
		"GL_EXT_texture_env_combine",
		"GL_EXT_texture_env_dot3",
		"GL_EXT_texture_filter_anisotropic",
		"GL_EXT_texture_integer",
		"GL_EXT_texture_lod_bias",
		"GL_EXT_texture_mirror_clamp",
		"GL_EXT_texture_object",
		"GL_EXT_texture_perturb_normal",
		"GL_EXT_texture_shared_exponent",
		"GL_EXT_texture_snorm",
		"GL_EXT_texture_sRGB",
		"GL_EXT_texture_sRGB_decode",
		"GL_EXT_texture_swizzle",
		"GL_EXT_timer_query",
		"GL_EXT_transform_feedback",
		"GL_EXT_vertex_array",
		"GL_EXT_vertex_array_bgra",
		"GL_EXT_vertex_attrib_64bit",
		"GL_EXT_vertex_shader",
		"GL_EXT_vertex_weighting",
		"GL_EXT_x11_sync_object",
		"GL_GREMEDY_frame_terminator",
		"GL_GREMEDY_string_marker",
		"GL_HP_convolution_border_modes",
		"GL_HP_image_transform",
		"GL_HP_occlusion_test",
		"GL_HP_texture_lighting",
		"GL_IBM_cull_vertex",
		"GL_IBM_multimode_draw_arrays",
		"GL_IBM_rasterpos_clip",
		"GL_IBM_static_data",
		"GL_IBM_texture_mirrored_repeat",
		"GL_IBM_vertex_array_lists",
		"GL_INGR_blend_func_separate",
		"GL_INGR_color_clamp",
		"GL_INGR_interlace_read",
		"GL_INGR_palette_buffer",
		"GL_INTEL_map_texture",
		"GL_INTEL_parallel_arrays",
		"GL_INTEL_texture_scissor",
		"GL_KHR_debug",
		"GL_KHR_texture_compression_astc_hdr",
		"GL_KHR_texture_compression_astc_ldr",
		"GL_MESA_pack_invert",
		"GL_MESA_resize_buffers",
		"GL_MESA_window_pos",
		"GL_MESAX_texture_stack",
		"GL_MESA_ycbcr_texture",
		"GL_NVX_conditional_render",
		"GL_NV_bindless_multi_draw_indirect",
		"GL_NV_bindless_texture",
		"GL_NV_blend_equation_advanced",
		"GL_NV_blend_equation_advanced_coherent",
		"GL_NV_blend_square",
		"GL_NV_compute_program5",
		"GL_NV_conditional_render",
		"GL_NV_copy_depth_to_color",
		"GL_NV_copy_image",
		"GL_NV_deep_texture3D",
		"GL_NV_depth_buffer_float",
		"GL_NV_depth_clamp",
		"GL_NV_draw_texture",
		"GL_NV_evaluators",
		"GL_NV_explicit_multisample",
		"GL_NV_fence",
		"GL_NV_float_buffer",
		"GL_NV_fog_distance",
		"GL_NV_fragment_program",
		"GL_NV_fragment_program2",
		"GL_NV_fragment_program4",
		"GL_NV_fragment_program_option",
		"GL_NV_framebuffer_multisample_coverage",
		"GL_NV_geometry_program4",
		"GL_NV_geometry_shader4",
		"GL_NV_gpu_program4",
		"GL_NV_gpu_program5",
		"GL_NV_gpu_program5_mem_extended",
		"GL_NV_gpu_shader5",
		"GL_NV_half_float",
		"GL_NV_light_max_exponent",
		"GL_NV_multisample_coverage",
		"GL_NV_multisample_filter_hint",
		"GL_NV_occlusion_query",
		"GL_NV_packed_depth_stencil",
		"GL_NV_parameter_buffer_object",
		"GL_NV_parameter_buffer_object2",
		"GL_NV_path_rendering",
		"GL_NV_pixel_data_range",
		"GL_NV_point_sprite",
		"GL_NV_present_video",
		"GL_NV_primitive_restart",
		"GL_NV_register_combiners",
		"GL_NV_register_combiners2",
		"GL_NV_shader_atomic_counters",
		"GL_NV_shader_atomic_float",
		"GL_NV_shader_buffer_load",
		"GL_NV_shader_buffer_store",
		"GL_NV_shader_storage_buffer_object",
		"GL_NV_tessellation_program5",
		"GL_NV_texgen_emboss",
		"GL_NV_texgen_reflection",
		"GL_NV_texture_barrier",
		"GL_NV_texture_compression_vtc",
		"GL_NV_texture_env_combine4",
		"GL_NV_texture_expand_normal",
		"GL_NV_texture_multisample",
		"GL_NV_texture_rectangle",
		"GL_NV_texture_shader",
		"GL_NV_texture_shader2",
		"GL_NV_texture_shader3",
		"GL_NV_transform_feedback",
		"GL_NV_transform_feedback2",
		"GL_NV_vdpau_interop",
		"GL_NV_vertex_array_range",
		"GL_NV_vertex_array_range2",
		"GL_NV_vertex_attrib_integer_64bit",
		"GL_NV_vertex_buffer_unified_memory",
		"GL_NV_vertex_program",
		"GL_NV_vertex_program1_1",
		"GL_NV_vertex_program2",
		"GL_NV_vertex_program2_option",
		"GL_NV_vertex_program3",
		"GL_NV_vertex_program4",
		"GL_NV_video_capture",
		"GL_OES_byte_coordinates",
		"GL_OES_compressed_paletted_texture",
		"GL_OES_fixed_point",
		"GL_OES_query_matrix",
		"GL_OES_read_format",
		"GL_OES_single_precision",
		"GL_OML_interlace",
		"GL_OML_resample",
		"GL_OML_subsample",
		"GL_PGI_misc_hints",
		"GL_PGI_vertex_hints",
		"GL_REND_screen_coordinates",
		"GL_S3_s3tc",
		"GL_SGI_color_matrix",
		"GL_SGI_color_table",
		"GL_SGI_texture_color_table",
		"GL_SGIS_detail_texture",
		"GL_SGIS_fog_function",
		"GL_SGIS_generate_mipmap",
		"GL_SGIS_multisample",
		"GL_SGIS_pixel_texture",
		"GL_SGIS_point_line_texgen",
		"GL_SGIS_point_parameters",
		"GL_SGIS_sharpen_texture",
		"GL_SGIS_texture4D",
		"GL_SGIS_texture_border_clamp",
		"GL_SGIS_texture_color_mask",
		"GL_SGIS_texture_edge_clamp",
		"GL_SGIS_texture_filter4",
		"GL_SGIS_texture_lod",
		"GL_SGIS_texture_select",
		"GL_SGIX_async",
		"GL_SGIX_async_histogram",
		"GL_SGIX_async_pixel",
		"GL_SGIX_blend_alpha_minmax",
		"GL_SGIX_calligraphic_fragment",
		"GL_SGIX_clipmap",
		"GL_SGIX_convolution_accuracy",
		"GL_SGIX_depth_pass_instrument",
		"GL_SGIX_depth_texture",
		"GL_SGIX_flush_raster",
		"GL_SGIX_fog_offset",
		"GL_SGIX_fog_scale",
		"GL_SGIX_fragment_lighting",
		"GL_SGIX_framezoom",
		"GL_SGIX_igloo_interface",
		"GL_SGIX_instruments",
		"GL_SGIX_interlace",
		"GL_SGIX_ir_instrument1",
		"GL_SGIX_list_priority",
		"GL_SGIX_pixel_texture",
		"GL_SGIX_pixel_tiles",
		"GL_SGIX_polynomial_ffd",
		"GL_SGIX_reference_plane",
		"GL_SGIX_resample",
		"GL_SGIX_scalebias_hint",
		"GL_SGIX_shadow",
		"GL_SGIX_shadow_ambient",
		"GL_SGIX_sprite",
		"GL_SGIX_subsample",
		"GL_SGIX_tag_sample_buffer",
		"GL_SGIX_texture_add_env",
		"GL_SGIX_texture_coordinate_clamp",
		"GL_SGIX_texture_lod_bias",
		"GL_SGIX_texture_multi_buffer",
		"GL_SGIX_texture_scale_bias",
		"GL_SGIX_vertex_preclip",
		"GL_SGIX_ycrcb",
		"GL_SGIX_ycrcba",
		"GL_SGIX_ycrcb_subsample",
		"GL_SUN_convolution_border_modes",
		"GL_SUN_global_alpha",
		"GL_SUN_mesh_array",
		"GL_SUN_slice_accum",
		"GL_SUN_triangle_list",
		"GL_SUN_vertex",
		"GL_SUNX_constant_data",
		"GL_WIN_phong_shading",
		"GL_WIN_specular_fog",
		// unofficial stuff
		"GL_NVX_gpu_memory_info"};

class COpenGLExtensionHandler
{
public:
	enum EOpenGLFeatures
	{
		IRR_3DFX_multisample = 0,
		IRR_3DFX_tbuffer,
		IRR_3DFX_texture_compression_FXT1,
		IRR_AMD_blend_minmax_factor,
		IRR_AMD_conservative_depth,
		IRR_AMD_debug_output,
		IRR_AMD_depth_clamp_separate,
		IRR_AMD_draw_buffers_blend,
		IRR_AMD_interleaved_elements,
		IRR_AMD_multi_draw_indirect,
		IRR_AMD_name_gen_delete,
		IRR_AMD_performance_monitor,
		IRR_AMD_pinned_memory,
		IRR_AMD_query_buffer_object,
		IRR_AMD_sample_positions,
		IRR_AMD_seamless_cubemap_per_texture,
		IRR_AMD_shader_atomic_counter_ops,
		IRR_AMD_shader_stencil_export,
		IRR_AMD_shader_trinary_minmax,
		IRR_AMD_sparse_texture,
		IRR_AMD_stencil_operation_extended,
		IRR_AMD_texture_texture4,
		IRR_AMD_transform_feedback3_lines_triangles,
		IRR_AMD_vertex_shader_layer,
		IRR_AMD_vertex_shader_tessellator,
		IRR_AMD_vertex_shader_viewport_index,
		IRR_APPLE_aux_depth_stencil,
		IRR_APPLE_client_storage,
		IRR_APPLE_element_array,
		IRR_APPLE_fence,
		IRR_APPLE_float_pixels,
		IRR_APPLE_flush_buffer_range,
		IRR_APPLE_object_purgeable,
		IRR_APPLE_rgb_422,
		IRR_APPLE_row_bytes,
		IRR_APPLE_specular_vector,
		IRR_APPLE_texture_range,
		IRR_APPLE_transform_hint,
		IRR_APPLE_vertex_array_object,
		IRR_APPLE_vertex_array_range,
		IRR_APPLE_vertex_program_evaluators,
		IRR_APPLE_ycbcr_422,
		IRR_ARB_arrays_of_arrays,
		IRR_ARB_base_instance,
		IRR_ARB_bindless_texture,
		IRR_ARB_blend_func_extended,
		IRR_ARB_buffer_storage,
		IRR_ARB_cl_event,
		IRR_ARB_clear_buffer_object,
		IRR_ARB_clear_texture,
		IRR_ARB_color_buffer_float,
		IRR_ARB_compatibility,
		IRR_ARB_compressed_texture_pixel_storage,
		IRR_ARB_compute_shader,
		IRR_ARB_compute_variable_group_size,
		IRR_ARB_conservative_depth,
		IRR_ARB_copy_buffer,
		IRR_ARB_copy_image,
		IRR_ARB_debug_output,
		IRR_ARB_depth_buffer_float,
		IRR_ARB_depth_clamp,
		IRR_ARB_depth_texture,
		IRR_ARB_direct_state_access,
		IRR_ARB_draw_buffers,
		IRR_ARB_draw_buffers_blend,
		IRR_ARB_draw_elements_base_vertex,
		IRR_ARB_draw_indirect,
		IRR_ARB_draw_instanced,
		IRR_ARB_ES2_compatibility,
		IRR_ARB_ES3_compatibility,
		IRR_ARB_enhanced_layouts,
		IRR_ARB_explicit_attrib_location,
		IRR_ARB_explicit_uniform_location,
		IRR_ARB_fragment_coord_conventions,
		IRR_ARB_fragment_layer_viewport,
		IRR_ARB_fragment_program,
		IRR_ARB_fragment_program_shadow,
		IRR_ARB_fragment_shader,
		IRR_ARB_framebuffer_no_attachments,
		IRR_ARB_framebuffer_object,
		IRR_ARB_framebuffer_sRGB,
		IRR_ARB_geometry_shader4,
		IRR_ARB_get_program_binary,
		IRR_ARB_gpu_shader5,
		IRR_ARB_gpu_shader_fp64,
		IRR_ARB_half_float_pixel,
		IRR_ARB_half_float_vertex,
		IRR_ARB_imaging,
		IRR_ARB_indirect_parameters,
		IRR_ARB_instanced_arrays,
		IRR_ARB_internalformat_query,
		IRR_ARB_internalformat_query2,
		IRR_ARB_invalidate_subdata,
		IRR_ARB_map_buffer_alignment,
		IRR_ARB_map_buffer_range,
		IRR_ARB_matrix_palette,
		IRR_ARB_multi_bind,
		IRR_ARB_multi_draw_indirect,
		IRR_ARB_multisample,
		IRR_ARB_multitexture,
		IRR_ARB_occlusion_query,
		IRR_ARB_occlusion_query2,
		IRR_ARB_pixel_buffer_object,
		IRR_ARB_point_parameters,
		IRR_ARB_point_sprite,
		IRR_ARB_program_interface_query,
		IRR_ARB_provoking_vertex,
		IRR_ARB_query_buffer_object,
		IRR_ARB_robust_buffer_access_behavior,
		IRR_ARB_robustness,
		IRR_ARB_robustness_isolation,
		IRR_ARB_sample_shading,
		IRR_ARB_sampler_objects,
		IRR_ARB_seamless_cube_map,
		IRR_ARB_seamless_cubemap_per_texture,
		IRR_ARB_separate_shader_objects,
		IRR_ARB_shader_atomic_counters,
		IRR_ARB_shader_bit_encoding,
		IRR_ARB_shader_draw_parameters,
		IRR_ARB_shader_group_vote,
		IRR_ARB_shader_image_load_store,
		IRR_ARB_shader_image_size,
		IRR_ARB_shader_objects,
		IRR_ARB_shader_precision,
		IRR_ARB_shader_stencil_export,
		IRR_ARB_shader_storage_buffer_object,
		IRR_ARB_shader_subroutine,
		IRR_ARB_shader_texture_lod,
		IRR_ARB_shading_language_100,
		IRR_ARB_shading_language_420pack,
		IRR_ARB_shading_language_include,
		IRR_ARB_shading_language_packing,
		IRR_ARB_shadow,
		IRR_ARB_shadow_ambient,
		IRR_ARB_sparse_texture,
		IRR_ARB_stencil_texturing,
		IRR_ARB_sync,
		IRR_ARB_tessellation_shader,
		IRR_ARB_texture_border_clamp,
		IRR_ARB_texture_buffer_object,
		IRR_ARB_texture_buffer_object_rgb32,
		IRR_ARB_texture_buffer_range,
		IRR_ARB_texture_compression,
		IRR_ARB_texture_compression_bptc,
		IRR_ARB_texture_compression_rgtc,
		IRR_ARB_texture_cube_map,
		IRR_ARB_texture_cube_map_array,
		IRR_ARB_texture_env_add,
		IRR_ARB_texture_env_combine,
		IRR_ARB_texture_env_crossbar,
		IRR_ARB_texture_env_dot3,
		IRR_ARB_texture_float,
		IRR_ARB_texture_gather,
		IRR_ARB_texture_mirror_clamp_to_edge,
		IRR_ARB_texture_mirrored_repeat,
		IRR_ARB_texture_multisample,
		IRR_ARB_texture_non_power_of_two,
		IRR_ARB_texture_query_levels,
		IRR_ARB_texture_query_lod,
		IRR_ARB_texture_rectangle,
		IRR_ARB_texture_rg,
		IRR_ARB_texture_rgb10_a2ui,
		IRR_ARB_texture_stencil8,
		IRR_ARB_texture_storage,
		IRR_ARB_texture_storage_multisample,
		IRR_ARB_texture_swizzle,
		IRR_ARB_texture_view,
		IRR_ARB_timer_query,
		IRR_ARB_transform_feedback2,
		IRR_ARB_transform_feedback3,
		IRR_ARB_transform_feedback_instanced,
		IRR_ARB_transpose_matrix,
		IRR_ARB_uniform_buffer_object,
		IRR_ARB_vertex_array_bgra,
		IRR_ARB_vertex_array_object,
		IRR_ARB_vertex_attrib_64bit,
		IRR_ARB_vertex_attrib_binding,
		IRR_ARB_vertex_blend,
		IRR_ARB_vertex_buffer_object,
		IRR_ARB_vertex_program,
		IRR_ARB_vertex_shader,
		IRR_ARB_vertex_type_10f_11f_11f_rev,
		IRR_ARB_vertex_type_2_10_10_10_rev,
		IRR_ARB_viewport_array,
		IRR_ARB_window_pos,
		IRR_ATI_draw_buffers,
		IRR_ATI_element_array,
		IRR_ATI_envmap_bumpmap,
		IRR_ATI_fragment_shader,
		IRR_ATI_map_object_buffer,
		IRR_ATI_meminfo,
		IRR_ATI_pixel_format_float,
		IRR_ATI_pn_triangles,
		IRR_ATI_separate_stencil,
		IRR_ATI_text_fragment_shader,
		IRR_ATI_texture_env_combine3,
		IRR_ATI_texture_float,
		IRR_ATI_texture_mirror_once,
		IRR_ATI_vertex_array_object,
		IRR_ATI_vertex_attrib_array_object,
		IRR_ATI_vertex_streams,
		IRR_EXT_422_pixels,
		IRR_EXT_abgr,
		IRR_EXT_bgra,
		IRR_EXT_bindable_uniform,
		IRR_EXT_blend_color,
		IRR_EXT_blend_equation_separate,
		IRR_EXT_blend_func_separate,
		IRR_EXT_blend_logic_op,
		IRR_EXT_blend_minmax,
		IRR_EXT_blend_subtract,
		IRR_EXT_clip_volume_hint,
		IRR_EXT_cmyka,
		IRR_EXT_color_subtable,
		IRR_EXT_compiled_vertex_array,
		IRR_EXT_convolution,
		IRR_EXT_coordinate_frame,
		IRR_EXT_copy_texture,
		IRR_EXT_cull_vertex,
		IRR_EXT_debug_label,
		IRR_EXT_debug_marker,
		IRR_EXT_depth_bounds_test,
		IRR_EXT_direct_state_access,
		IRR_EXT_draw_buffers2,
		IRR_EXT_draw_instanced,
		IRR_EXT_draw_range_elements,
		IRR_EXT_fog_coord,
		IRR_EXT_framebuffer_blit,
		IRR_EXT_framebuffer_multisample,
		IRR_EXT_framebuffer_multisample_blit_scaled,
		IRR_EXT_framebuffer_object,
		IRR_EXT_framebuffer_sRGB,
		IRR_EXT_geometry_shader4,
		IRR_EXT_gpu_program_parameters,
		IRR_EXT_gpu_shader4,
		IRR_EXT_histogram,
		IRR_EXT_index_array_formats,
		IRR_EXT_index_func,
		IRR_EXT_index_material,
		IRR_EXT_index_texture,
		IRR_EXT_light_texture,
		IRR_EXT_misc_attribute,
		IRR_EXT_multi_draw_arrays,
		IRR_EXT_multisample,
		IRR_EXT_packed_depth_stencil,
		IRR_EXT_packed_float,
		IRR_EXT_packed_pixels,
		IRR_EXT_paletted_texture,
		IRR_EXT_pixel_buffer_object,
		IRR_EXT_pixel_transform,
		IRR_EXT_pixel_transform_color_table,
		IRR_EXT_point_parameters,
		IRR_EXT_polygon_offset,
		IRR_EXT_provoking_vertex,
		IRR_EXT_rescale_normal,
		IRR_EXT_secondary_color,
		IRR_EXT_separate_shader_objects,
		IRR_EXT_separate_specular_color,
		IRR_EXT_shader_image_load_store,
		IRR_EXT_shader_integer_mix,
		IRR_EXT_shadow_funcs,
		IRR_EXT_shared_texture_palette,
		IRR_EXT_stencil_clear_tag,
		IRR_EXT_stencil_two_side,
		IRR_EXT_stencil_wrap,
		IRR_EXT_subtexture,
		IRR_EXT_texture,
		IRR_EXT_texture3D,
		IRR_EXT_texture_array,
		IRR_EXT_texture_buffer_object,
		IRR_EXT_texture_compression_latc,
		IRR_EXT_texture_compression_rgtc,
		IRR_EXT_texture_compression_s3tc,
		IRR_EXT_texture_cube_map,
		IRR_EXT_texture_env_add,
		IRR_EXT_texture_env_combine,
		IRR_EXT_texture_env_dot3,
		IRR_EXT_texture_filter_anisotropic,
		IRR_EXT_texture_integer,
		IRR_EXT_texture_lod_bias,
		IRR_EXT_texture_mirror_clamp,
		IRR_EXT_texture_object,
		IRR_EXT_texture_perturb_normal,
		IRR_EXT_texture_shared_exponent,
		IRR_EXT_texture_snorm,
		IRR_EXT_texture_sRGB,
		IRR_EXT_texture_sRGB_decode,
		IRR_EXT_texture_swizzle,
		IRR_EXT_timer_query,
		IRR_EXT_transform_feedback,
		IRR_EXT_vertex_array,
		IRR_EXT_vertex_array_bgra,
		IRR_EXT_vertex_attrib_64bit,
		IRR_EXT_vertex_shader,
		IRR_EXT_vertex_weighting,
		IRR_EXT_x11_sync_object,
		IRR_GREMEDY_frame_terminator,
		IRR_GREMEDY_string_marker,
		IRR_HP_convolution_border_modes,
		IRR_HP_image_transform,
		IRR_HP_occlusion_test,
		IRR_HP_texture_lighting,
		IRR_IBM_cull_vertex,
		IRR_IBM_multimode_draw_arrays,
		IRR_IBM_rasterpos_clip,
		IRR_IBM_static_data,
		IRR_IBM_texture_mirrored_repeat,
		IRR_IBM_vertex_array_lists,
		IRR_INGR_blend_func_separate,
		IRR_INGR_color_clamp,
		IRR_INGR_interlace_read,
		IRR_INGR_palette_buffer,
		IRR_INTEL_map_texture,
		IRR_INTEL_parallel_arrays,
		IRR_INTEL_texture_scissor,
		IRR_KHR_debug,
		IRR_KHR_texture_compression_astc_hdr,
		IRR_KHR_texture_compression_astc_ldr,
		IRR_MESA_pack_invert,
		IRR_MESA_resize_buffers,
		IRR_MESA_window_pos,
		IRR_MESAX_texture_stack,
		IRR_MESA_ycbcr_texture,
		IRR_NVX_conditional_render,
		IRR_NV_bindless_multi_draw_indirect,
		IRR_NV_bindless_texture,
		IRR_NV_blend_equation_advanced,
		IRR_NV_blend_equation_advanced_coherent,
		IRR_NV_blend_square,
		IRR_NV_compute_program5,
		IRR_NV_conditional_render,
		IRR_NV_copy_depth_to_color,
		IRR_NV_copy_image,
		IRR_NV_deep_texture3D,
		IRR_NV_depth_buffer_float,
		IRR_NV_depth_clamp,
		IRR_NV_draw_texture,
		IRR_NV_evaluators,
		IRR_NV_explicit_multisample,
		IRR_NV_fence,
		IRR_NV_float_buffer,
		IRR_NV_fog_distance,
		IRR_NV_fragment_program,
		IRR_NV_fragment_program2,
		IRR_NV_fragment_program4,
		IRR_NV_fragment_program_option,
		IRR_NV_framebuffer_multisample_coverage,
		IRR_NV_geometry_program4,
		IRR_NV_geometry_shader4,
		IRR_NV_gpu_program4,
		IRR_NV_gpu_program5,
		IRR_NV_gpu_program5_mem_extended,
		IRR_NV_gpu_shader5,
		IRR_NV_half_float,
		IRR_NV_light_max_exponent,
		IRR_NV_multisample_coverage,
		IRR_NV_multisample_filter_hint,
		IRR_NV_occlusion_query,
		IRR_NV_packed_depth_stencil,
		IRR_NV_parameter_buffer_object,
		IRR_NV_parameter_buffer_object2,
		IRR_NV_path_rendering,
		IRR_NV_pixel_data_range,
		IRR_NV_point_sprite,
		IRR_NV_present_video,
		IRR_NV_primitive_restart,
		IRR_NV_register_combiners,
		IRR_NV_register_combiners2,
		IRR_NV_shader_atomic_counters,
		IRR_NV_shader_atomic_float,
		IRR_NV_shader_buffer_load,
		IRR_NV_shader_buffer_store,
		IRR_NV_shader_storage_buffer_object,
		IRR_NV_tessellation_program5,
		IRR_NV_texgen_emboss,
		IRR_NV_texgen_reflection,
		IRR_NV_texture_barrier,
		IRR_NV_texture_compression_vtc,
		IRR_NV_texture_env_combine4,
		IRR_NV_texture_expand_normal,
		IRR_NV_texture_multisample,
		IRR_NV_texture_rectangle,
		IRR_NV_texture_shader,
		IRR_NV_texture_shader2,
		IRR_NV_texture_shader3,
		IRR_NV_transform_feedback,
		IRR_NV_transform_feedback2,
		IRR_NV_vdpau_interop,
		IRR_NV_vertex_array_range,
		IRR_NV_vertex_array_range2,
		IRR_NV_vertex_attrib_integer_64bit,
		IRR_NV_vertex_buffer_unified_memory,
		IRR_NV_vertex_program,
		IRR_NV_vertex_program1_1,
		IRR_NV_vertex_program2,
		IRR_NV_vertex_program2_option,
		IRR_NV_vertex_program3,
		IRR_NV_vertex_program4,
		IRR_NV_video_capture,
		IRR_OES_byte_coordinates,
		IRR_OES_compressed_paletted_texture,
		IRR_OES_fixed_point,
		IRR_OES_query_matrix,
		IRR_OES_read_format,
		IRR_OES_single_precision,
		IRR_OML_interlace,
		IRR_OML_resample,
		IRR_OML_subsample,
		IRR_PGI_misc_hints,
		IRR_PGI_vertex_hints,
		IRR_REND_screen_coordinates,
		IRR_S3_s3tc,
		IRR_SGI_color_matrix,
		IRR_SGI_color_table,
		IRR_SGI_texture_color_table,
		IRR_SGIS_detail_texture,
		IRR_SGIS_fog_function,
		IRR_SGIS_generate_mipmap,
		IRR_SGIS_multisample,
		IRR_SGIS_pixel_texture,
		IRR_SGIS_point_line_texgen,
		IRR_SGIS_point_parameters,
		IRR_SGIS_sharpen_texture,
		IRR_SGIS_texture4D,
		IRR_SGIS_texture_border_clamp,
		IRR_SGIS_texture_color_mask,
		IRR_SGIS_texture_edge_clamp,
		IRR_SGIS_texture_filter4,
		IRR_SGIS_texture_lod,
		IRR_SGIS_texture_select,
		IRR_SGIX_async,
		IRR_SGIX_async_histogram,
		IRR_SGIX_async_pixel,
		IRR_SGIX_blend_alpha_minmax,
		IRR_SGIX_calligraphic_fragment,
		IRR_SGIX_clipmap,
		IRR_SGIX_convolution_accuracy,
		IRR_SGIX_depth_pass_instrument,
		IRR_SGIX_depth_texture,
		IRR_SGIX_flush_raster,
		IRR_SGIX_fog_offset,
		IRR_SGIX_fog_scale,
		IRR_SGIX_fragment_lighting,
		IRR_SGIX_framezoom,
		IRR_SGIX_igloo_interface,
		IRR_SGIX_instruments,
		IRR_SGIX_interlace,
		IRR_SGIX_ir_instrument1,
		IRR_SGIX_list_priority,
		IRR_SGIX_pixel_texture,
		IRR_SGIX_pixel_tiles,
		IRR_SGIX_polynomial_ffd,
		IRR_SGIX_reference_plane,
		IRR_SGIX_resample,
		IRR_SGIX_scalebias_hint,
		IRR_SGIX_shadow,
		IRR_SGIX_shadow_ambient,
		IRR_SGIX_sprite,
		IRR_SGIX_subsample,
		IRR_SGIX_tag_sample_buffer,
		IRR_SGIX_texture_add_env,
		IRR_SGIX_texture_coordinate_clamp,
		IRR_SGIX_texture_lod_bias,
		IRR_SGIX_texture_multi_buffer,
		IRR_SGIX_texture_scale_bias,
		IRR_SGIX_vertex_preclip,
		IRR_SGIX_ycrcb,
		IRR_SGIX_ycrcba,
		IRR_SGIX_ycrcb_subsample,
		IRR_SUN_convolution_border_modes,
		IRR_SUN_global_alpha,
		IRR_SUN_mesh_array,
		IRR_SUN_slice_accum,
		IRR_SUN_triangle_list,
		IRR_SUN_vertex,
		IRR_SUNX_constant_data,
		IRR_WIN_phong_shading,
		IRR_WIN_specular_fog,
		IRR_NVX_gpu_memory_info,
		IRR_OpenGL_Feature_Count
	};

	// constructor
	COpenGLExtensionHandler();

	// deferred initialization
	void initExtensions(video::IContextManager *cmgr, bool stencilBuffer);

	const COpenGLCoreFeature &getFeature() const;

	//! queries the features of the driver, returns true if feature is available
	bool queryFeature(E_VIDEO_DRIVER_FEATURE feature) const;

	//! queries the features of the driver, returns true if feature is available
	bool queryOpenGLFeature(EOpenGLFeatures feature) const
	{
		return FeatureAvailable[feature];
	}

	//! show all features with availability
	void dump(ELOG_LEVEL logLevel) const;

	// Some variables for properties
	bool StencilBuffer;
	bool TextureCompressionExtension;

	// Some non-boolean properties
	//! Maximal Anisotropy
	u8 MaxAnisotropy;
	//! Number of auxiliary buffers
	u8 MaxAuxBuffers;
	//! Optimal number of indices per meshbuffer
	u32 MaxIndices;
	//! Maximal texture dimension
	u32 MaxTextureSize;
	//! Maximal vertices handled by geometry shaders
	u32 MaxGeometryVerticesOut;
	//! Maximal LOD Bias
	f32 MaxTextureLODBias;
	//! Minimal and maximal supported thickness for lines without smoothing
	GLfloat DimAliasedLine[2];
	//! Minimal and maximal supported thickness for points without smoothing
	GLfloat DimAliasedPoint[2];
	//! Minimal and maximal supported thickness for lines with smoothing
	GLfloat DimSmoothedLine[2];
	//! Minimal and maximal supported thickness for points with smoothing
	GLfloat DimSmoothedPoint[2];

	//! OpenGL version as Integer: 100*Major+Minor, i.e. 2.1 becomes 201
	u16 Version;
	//! GLSL version as Integer: 100*Major+Minor
	u16 ShaderLanguageVersion;

	bool OcclusionQuerySupport;

	// Info needed for workarounds.
	bool IsAtiRadeonX;

	//! Workaround until direct state access with framebuffers is stable enough in drivers
	// https://devtalk.nvidia.com/default/topic/1030494/opengl/bug-amp-amp-spec-violation-checknamedframebufferstatus-returns-gl_framebuffer_incomplete_dimensions_ext-under-gl-4-5-core/
	// https://stackoverflow.com/questions/51304706/problems-with-attaching-textures-of-different-sizes-to-fbo
	static bool needsDSAFramebufferHack;

	// public access to the (loaded) extensions.
	// general functions
	void irrGlActiveTexture(GLenum texture);
	void irrGlClientActiveTexture(GLenum texture);
	void extGlPointParameterf(GLint loc, GLfloat f);
	void extGlPointParameterfv(GLint loc, const GLfloat *v);
	void extGlStencilFuncSeparate(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);
	void extGlStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
	void irrGlCompressedTexImage2D(GLenum target, GLint level,
			GLenum internalformat, GLsizei width, GLsizei height,
			GLint border, GLsizei imageSize, const void *data);
	void irrGlCompressedTexSubImage2D(GLenum target, GLint level,
			GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
			GLenum format, GLsizei imageSize, const void *data);
	inline void irrGlObjectLabel(GLenum identifier, GLuint name, const char *label)
	{
		// unimplemented
	}

	// shader programming
	void extGlGenPrograms(GLsizei n, GLuint *programs);
	void extGlBindProgram(GLenum target, GLuint program);
	void extGlProgramString(GLenum target, GLenum format, GLsizei len, const GLvoid *string);
	void extGlLoadProgram(GLenum target, GLuint id, GLsizei len, const GLubyte *string);
	void extGlDeletePrograms(GLsizei n, const GLuint *programs);
	void extGlProgramLocalParameter4fv(GLenum, GLuint, const GLfloat *);
	GLhandleARB extGlCreateShaderObject(GLenum shaderType);
	GLuint extGlCreateShader(GLenum shaderType);
	// note: Due to the type confusion between shader_objects and OpenGL 2.0
	// we have to add the ARB extension for proper method definitions in case
	// that handleARB and uint are the same type
	void extGlShaderSourceARB(GLhandleARB shader, GLsizei numOfStrings, const char **strings, const GLint *lenOfStrings);
	void extGlShaderSource(GLuint shader, GLsizei numOfStrings, const char **strings, const GLint *lenOfStrings);
	// note: Due to the type confusion between shader_objects and OpenGL 2.0
	// we have to add the ARB extension for proper method definitions in case
	// that handleARB and uint are the same type
	void extGlCompileShaderARB(GLhandleARB shader);
	void extGlCompileShader(GLuint shader);
	GLhandleARB extGlCreateProgramObject(void);
	GLuint extGlCreateProgram(void);
	void extGlAttachObject(GLhandleARB program, GLhandleARB shader);
	void extGlAttachShader(GLuint program, GLuint shader);
	void extGlLinkProgramARB(GLhandleARB program);
	// note: Due to the type confusion between shader_objects and OpenGL 2.0
	// we have to add the ARB extension for proper method definitions in case
	// that handleARB and uint are the same type
	void extGlLinkProgram(GLuint program);
	void extGlUseProgramObject(GLhandleARB prog);
	void irrGlUseProgram(GLuint prog);
	void extGlDeleteObject(GLhandleARB object);
	void extGlDeleteProgram(GLuint object);
	void extGlDeleteShader(GLuint shader);
	void extGlGetAttachedShaders(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders);
	void extGlGetAttachedObjects(GLhandleARB program, GLsizei maxcount, GLsizei *count, GLhandleARB *shaders);
	void extGlGetInfoLog(GLhandleARB object, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
	void extGlGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
	void extGlGetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
	void extGlGetObjectParameteriv(GLhandleARB object, GLenum type, GLint *param);
	void extGlGetShaderiv(GLuint shader, GLenum type, GLint *param);
	void extGlGetProgramiv(GLuint program, GLenum type, GLint *param);
	GLint extGlGetUniformLocationARB(GLhandleARB program, const char *name);
	GLint extGlGetUniformLocation(GLuint program, const char *name);
	void extGlUniform1fv(GLint loc, GLsizei count, const GLfloat *v);
	void extGlUniform2fv(GLint loc, GLsizei count, const GLfloat *v);
	void extGlUniform3fv(GLint loc, GLsizei count, const GLfloat *v);
	void extGlUniform4fv(GLint loc, GLsizei count, const GLfloat *v);
	void extGlUniform1bv(GLint loc, GLsizei count, const bool *v);
	void extGlUniform2bv(GLint loc, GLsizei count, const bool *v);
	void extGlUniform3bv(GLint loc, GLsizei count, const bool *v);
	void extGlUniform4bv(GLint loc, GLsizei count, const bool *v);
	void extGlUniform1iv(GLint loc, GLsizei count, const GLint *v);
	void extGlUniform2iv(GLint loc, GLsizei count, const GLint *v);
	void extGlUniform3iv(GLint loc, GLsizei count, const GLint *v);
	void extGlUniform4iv(GLint loc, GLsizei count, const GLint *v);
	void extGlUniform1uiv(GLint loc, GLsizei count, const GLuint *v);
	void extGlUniform2uiv(GLint loc, GLsizei count, const GLuint *v);
	void extGlUniform3uiv(GLint loc, GLsizei count, const GLuint *v);
	void extGlUniform4uiv(GLint loc, GLsizei count, const GLuint *v);
	void extGlUniformMatrix2fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlUniformMatrix2x3fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlUniformMatrix2x4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlUniformMatrix3x2fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlUniformMatrix3fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlUniformMatrix3x4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlUniformMatrix4x2fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlUniformMatrix4x3fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlUniformMatrix4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v);
	void extGlGetActiveUniformARB(GLhandleARB program, GLuint index, GLsizei maxlength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
	void extGlGetActiveUniform(GLuint program, GLuint index, GLsizei maxlength, GLsizei *length, GLint *size, GLenum *type, GLchar *name);

	// framebuffer objects
	void irrGlBindFramebuffer(GLenum target, GLuint framebuffer);
	void irrGlDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
	void irrGlGenFramebuffers(GLsizei n, GLuint *framebuffers);
	GLenum irrGlCheckFramebufferStatus(GLenum target);
	void irrGlFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
	void irrGlBindRenderbuffer(GLenum target, GLuint renderbuffer);
	void irrGlDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
	void irrGlGenRenderbuffers(GLsizei n, GLuint *renderbuffers);
	void irrGlRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
	void irrGlFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
	void irrGlGenerateMipmap(GLenum target);
	void irrGlActiveStencilFace(GLenum face);
	void irrGlDrawBuffer(GLenum mode);
	void irrGlDrawBuffers(GLsizei n, const GLenum *bufs);

	// vertex buffer object
	void extGlGenBuffers(GLsizei n, GLuint *buffers);
	void extGlBindBuffer(GLenum target, GLuint buffer);
	void extGlBufferData(GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
	void extGlDeleteBuffers(GLsizei n, const GLuint *buffers);
	void extGlBufferSubData(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);
	void extGlGetBufferSubData(GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data);
	void *extGlMapBuffer(GLenum target, GLenum access);
	GLboolean extGlUnmapBuffer(GLenum target);
	GLboolean extGlIsBuffer(GLuint buffer);
	void extGlGetBufferParameteriv(GLenum target, GLenum pname, GLint *params);
	void extGlGetBufferPointerv(GLenum target, GLenum pname, GLvoid **params);
	void extGlProvokingVertex(GLenum mode);
	void extGlProgramParameteri(GLuint program, GLenum pname, GLint value);

	// occlusion query
	void extGlGenQueries(GLsizei n, GLuint *ids);
	void extGlDeleteQueries(GLsizei n, const GLuint *ids);
	GLboolean extGlIsQuery(GLuint id);
	void extGlBeginQuery(GLenum target, GLuint id);
	void extGlEndQuery(GLenum target);
	void extGlGetQueryiv(GLenum target, GLenum pname, GLint *params);
	void extGlGetQueryObjectiv(GLuint id, GLenum pname, GLint *params);
	void extGlGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params);

	// blend
	void irrGlBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
	void irrGlBlendEquation(GLenum mode);

	// indexed
	void irrGlEnableIndexed(GLenum target, GLuint index);
	void irrGlDisableIndexed(GLenum target, GLuint index);
	void irrGlColorMaskIndexed(GLuint buf, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
	void irrGlBlendFuncIndexed(GLuint buf, GLenum src, GLenum dst);
	void irrGlBlendFuncSeparateIndexed(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
	void irrGlBlendEquationIndexed(GLuint buf, GLenum mode);
	void irrGlBlendEquationSeparateIndexed(GLuint buf, GLenum modeRGB, GLenum modeAlpha);

	void extGlTextureSubImage2D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
	void extGlTextureStorage2D(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
	void extGlTextureStorage3D(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
	void extGlGetTextureImage(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
	void extGlNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
	void extGlTextureParameteri(GLuint texture, GLenum pname, GLint param);
	void extGlTextureParameterf(GLuint texture, GLenum pname, GLfloat param);
	void extGlTextureParameteriv(GLuint texture, GLenum pname, const GLint *params);
	void extGlTextureParameterfv(GLuint texture, GLenum pname, const GLfloat *params);
	void extGlCreateTextures(GLenum target, GLsizei n, GLuint *textures);
	void extGlCreateFramebuffers(GLsizei n, GLuint *framebuffers);
	void extGlBindTextures(GLuint first, GLsizei count, const GLuint *textures, const GLenum *targets);
	void extGlGenerateTextureMipmap(GLuint texture, GLenum target);

	// generic vsync setting method for several extensions
	void extGlSwapInterval(int interval);

	// the global feature array
	bool FeatureAvailable[IRR_OpenGL_Feature_Count];

protected:
	COpenGLCoreFeature Feature;

	PFNGLACTIVETEXTUREPROC pGlActiveTexture;
	PFNGLACTIVETEXTUREARBPROC pGlActiveTextureARB;
	PFNGLCLIENTACTIVETEXTUREARBPROC pGlClientActiveTextureARB;
	PFNGLGENPROGRAMSARBPROC pGlGenProgramsARB;
	PFNGLGENPROGRAMSNVPROC pGlGenProgramsNV;
	PFNGLBINDPROGRAMARBPROC pGlBindProgramARB;
	PFNGLBINDPROGRAMNVPROC pGlBindProgramNV;
	PFNGLDELETEPROGRAMSARBPROC pGlDeleteProgramsARB;
	PFNGLDELETEPROGRAMSNVPROC pGlDeleteProgramsNV;
	PFNGLPROGRAMSTRINGARBPROC pGlProgramStringARB;
	PFNGLLOADPROGRAMNVPROC pGlLoadProgramNV;
	PFNGLPROGRAMLOCALPARAMETER4FVARBPROC pGlProgramLocalParameter4fvARB;
	PFNGLCREATESHADEROBJECTARBPROC pGlCreateShaderObjectARB;
	PFNGLSHADERSOURCEARBPROC pGlShaderSourceARB;
	PFNGLCOMPILESHADERARBPROC pGlCompileShaderARB;
	PFNGLCREATEPROGRAMOBJECTARBPROC pGlCreateProgramObjectARB;
	PFNGLATTACHOBJECTARBPROC pGlAttachObjectARB;
	PFNGLLINKPROGRAMARBPROC pGlLinkProgramARB;
	PFNGLUSEPROGRAMOBJECTARBPROC pGlUseProgramObjectARB;
	PFNGLDELETEOBJECTARBPROC pGlDeleteObjectARB;
	PFNGLCREATEPROGRAMPROC pGlCreateProgram;
	PFNGLUSEPROGRAMPROC pGlUseProgram;
	PFNGLDELETEPROGRAMPROC pGlDeleteProgram;
	PFNGLDELETESHADERPROC pGlDeleteShader;
	PFNGLGETATTACHEDOBJECTSARBPROC pGlGetAttachedObjectsARB;
	PFNGLGETATTACHEDSHADERSPROC pGlGetAttachedShaders;
	PFNGLCREATESHADERPROC pGlCreateShader;
	PFNGLSHADERSOURCEPROC pGlShaderSource;
	PFNGLCOMPILESHADERPROC pGlCompileShader;
	PFNGLATTACHSHADERPROC pGlAttachShader;
	PFNGLLINKPROGRAMPROC pGlLinkProgram;
	PFNGLGETINFOLOGARBPROC pGlGetInfoLogARB;
	PFNGLGETSHADERINFOLOGPROC pGlGetShaderInfoLog;
	PFNGLGETPROGRAMINFOLOGPROC pGlGetProgramInfoLog;
	PFNGLGETOBJECTPARAMETERIVARBPROC pGlGetObjectParameterivARB;
	PFNGLGETSHADERIVPROC pGlGetShaderiv;
	PFNGLGETSHADERIVPROC pGlGetProgramiv;
	PFNGLGETUNIFORMLOCATIONARBPROC pGlGetUniformLocationARB;
	PFNGLGETUNIFORMLOCATIONPROC pGlGetUniformLocation;
	PFNGLUNIFORM1FVARBPROC pGlUniform1fvARB;
	PFNGLUNIFORM2FVARBPROC pGlUniform2fvARB;
	PFNGLUNIFORM3FVARBPROC pGlUniform3fvARB;
	PFNGLUNIFORM4FVARBPROC pGlUniform4fvARB;
	PFNGLUNIFORM1IVARBPROC pGlUniform1ivARB;
	PFNGLUNIFORM2IVARBPROC pGlUniform2ivARB;
	PFNGLUNIFORM3IVARBPROC pGlUniform3ivARB;
	PFNGLUNIFORM4IVARBPROC pGlUniform4ivARB;
	PFNGLUNIFORM1UIVPROC pGlUniform1uiv;
	PFNGLUNIFORM2UIVPROC pGlUniform2uiv;
	PFNGLUNIFORM3UIVPROC pGlUniform3uiv;
	PFNGLUNIFORM4UIVPROC pGlUniform4uiv;
	PFNGLUNIFORMMATRIX2FVARBPROC pGlUniformMatrix2fvARB;
	PFNGLUNIFORMMATRIX2X3FVPROC pGlUniformMatrix2x3fv;
	PFNGLUNIFORMMATRIX2X4FVPROC pGlUniformMatrix2x4fv;
	PFNGLUNIFORMMATRIX3X2FVPROC pGlUniformMatrix3x2fv;
	PFNGLUNIFORMMATRIX3FVARBPROC pGlUniformMatrix3fvARB;
	PFNGLUNIFORMMATRIX3X4FVPROC pGlUniformMatrix3x4fv;
	PFNGLUNIFORMMATRIX4X2FVPROC pGlUniformMatrix4x2fv;
	PFNGLUNIFORMMATRIX4X3FVPROC pGlUniformMatrix4x3fv;
	PFNGLUNIFORMMATRIX4FVARBPROC pGlUniformMatrix4fvARB;
	PFNGLGETACTIVEUNIFORMARBPROC pGlGetActiveUniformARB;
	PFNGLGETACTIVEUNIFORMPROC pGlGetActiveUniform;
	PFNGLPOINTPARAMETERFARBPROC pGlPointParameterfARB;
	PFNGLPOINTPARAMETERFVARBPROC pGlPointParameterfvARB;
	PFNGLSTENCILFUNCSEPARATEPROC pGlStencilFuncSeparate;
	PFNGLSTENCILOPSEPARATEPROC pGlStencilOpSeparate;
	PFNGLSTENCILFUNCSEPARATEATIPROC pGlStencilFuncSeparateATI;
	PFNGLSTENCILOPSEPARATEATIPROC pGlStencilOpSeparateATI;
	PFNGLCOMPRESSEDTEXIMAGE2DPROC pGlCompressedTexImage2D;
	PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC pGlCompressedTexSubImage2D;
	// ARB framebuffer object
	PFNGLBINDFRAMEBUFFERPROC pGlBindFramebuffer;
	PFNGLDELETEFRAMEBUFFERSPROC pGlDeleteFramebuffers;
	PFNGLGENFRAMEBUFFERSPROC pGlGenFramebuffers;
	PFNGLCHECKFRAMEBUFFERSTATUSPROC pGlCheckFramebufferStatus;
	PFNGLFRAMEBUFFERTEXTURE2DPROC pGlFramebufferTexture2D;
	PFNGLBINDRENDERBUFFERPROC pGlBindRenderbuffer;
	PFNGLDELETERENDERBUFFERSPROC pGlDeleteRenderbuffers;
	PFNGLGENRENDERBUFFERSPROC pGlGenRenderbuffers;
	PFNGLRENDERBUFFERSTORAGEPROC pGlRenderbufferStorage;
	PFNGLFRAMEBUFFERRENDERBUFFERPROC pGlFramebufferRenderbuffer;
	PFNGLGENERATEMIPMAPPROC pGlGenerateMipmap;
	// EXT framebuffer object
	PFNGLBINDFRAMEBUFFEREXTPROC pGlBindFramebufferEXT;
	PFNGLDELETEFRAMEBUFFERSEXTPROC pGlDeleteFramebuffersEXT;
	PFNGLGENFRAMEBUFFERSEXTPROC pGlGenFramebuffersEXT;
	PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC pGlCheckFramebufferStatusEXT;
	PFNGLFRAMEBUFFERTEXTURE2DEXTPROC pGlFramebufferTexture2DEXT;
	PFNGLBINDRENDERBUFFEREXTPROC pGlBindRenderbufferEXT;
	PFNGLDELETERENDERBUFFERSEXTPROC pGlDeleteRenderbuffersEXT;
	PFNGLGENRENDERBUFFERSEXTPROC pGlGenRenderbuffersEXT;
	PFNGLRENDERBUFFERSTORAGEEXTPROC pGlRenderbufferStorageEXT;
	PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC pGlFramebufferRenderbufferEXT;
	PFNGLGENERATEMIPMAPEXTPROC pGlGenerateMipmapEXT;
	PFNGLACTIVESTENCILFACEEXTPROC pGlActiveStencilFaceEXT;
	PFNGLDRAWBUFFERSARBPROC pGlDrawBuffersARB;
	PFNGLDRAWBUFFERSATIPROC pGlDrawBuffersATI;
	PFNGLGENBUFFERSARBPROC pGlGenBuffersARB;
	PFNGLBINDBUFFERARBPROC pGlBindBufferARB;
	PFNGLBUFFERDATAARBPROC pGlBufferDataARB;
	PFNGLDELETEBUFFERSARBPROC pGlDeleteBuffersARB;
	PFNGLBUFFERSUBDATAARBPROC pGlBufferSubDataARB;
	PFNGLGETBUFFERSUBDATAARBPROC pGlGetBufferSubDataARB;
	PFNGLMAPBUFFERARBPROC pGlMapBufferARB;
	PFNGLUNMAPBUFFERARBPROC pGlUnmapBufferARB;
	PFNGLISBUFFERARBPROC pGlIsBufferARB;
	PFNGLGETBUFFERPARAMETERIVARBPROC pGlGetBufferParameterivARB;
	PFNGLGETBUFFERPOINTERVARBPROC pGlGetBufferPointervARB;
	PFNGLPROVOKINGVERTEXPROC pGlProvokingVertexARB;
	PFNGLPROVOKINGVERTEXEXTPROC pGlProvokingVertexEXT;
	PFNGLPROGRAMPARAMETERIARBPROC pGlProgramParameteriARB;
	PFNGLPROGRAMPARAMETERIEXTPROC pGlProgramParameteriEXT;
	PFNGLGENQUERIESARBPROC pGlGenQueriesARB;
	PFNGLDELETEQUERIESARBPROC pGlDeleteQueriesARB;
	PFNGLISQUERYARBPROC pGlIsQueryARB;
	PFNGLBEGINQUERYARBPROC pGlBeginQueryARB;
	PFNGLENDQUERYARBPROC pGlEndQueryARB;
	PFNGLGETQUERYIVARBPROC pGlGetQueryivARB;
	PFNGLGETQUERYOBJECTIVARBPROC pGlGetQueryObjectivARB;
	PFNGLGETQUERYOBJECTUIVARBPROC pGlGetQueryObjectuivARB;
	PFNGLGENOCCLUSIONQUERIESNVPROC pGlGenOcclusionQueriesNV;
	PFNGLDELETEOCCLUSIONQUERIESNVPROC pGlDeleteOcclusionQueriesNV;
	PFNGLISOCCLUSIONQUERYNVPROC pGlIsOcclusionQueryNV;
	PFNGLBEGINOCCLUSIONQUERYNVPROC pGlBeginOcclusionQueryNV;
	PFNGLENDOCCLUSIONQUERYNVPROC pGlEndOcclusionQueryNV;
	PFNGLGETOCCLUSIONQUERYIVNVPROC pGlGetOcclusionQueryivNV;
	PFNGLGETOCCLUSIONQUERYUIVNVPROC pGlGetOcclusionQueryuivNV;
	// Blend
	PFNGLBLENDFUNCSEPARATEEXTPROC pGlBlendFuncSeparateEXT;
	PFNGLBLENDFUNCSEPARATEPROC pGlBlendFuncSeparate;
	PFNGLBLENDEQUATIONEXTPROC pGlBlendEquationEXT;
	PFNGLBLENDEQUATIONPROC pGlBlendEquation;
	PFNGLBLENDEQUATIONSEPARATEEXTPROC pGlBlendEquationSeparateEXT;
	PFNGLBLENDEQUATIONSEPARATEPROC pGlBlendEquationSeparate;
	// Indexed
	PFNGLENABLEINDEXEDEXTPROC pGlEnableIndexedEXT;
	PFNGLDISABLEINDEXEDEXTPROC pGlDisableIndexedEXT;
	PFNGLCOLORMASKINDEXEDEXTPROC pGlColorMaskIndexedEXT;
	PFNGLBLENDFUNCINDEXEDAMDPROC pGlBlendFuncIndexedAMD;
	PFNGLBLENDFUNCIPROC pGlBlendFunciARB;
	PFNGLBLENDFUNCSEPARATEINDEXEDAMDPROC pGlBlendFuncSeparateIndexedAMD;
	PFNGLBLENDFUNCSEPARATEIPROC pGlBlendFuncSeparateiARB;
	PFNGLBLENDEQUATIONINDEXEDAMDPROC pGlBlendEquationIndexedAMD;
	PFNGLBLENDEQUATIONIPROC pGlBlendEquationiARB;
	PFNGLBLENDEQUATIONSEPARATEINDEXEDAMDPROC pGlBlendEquationSeparateIndexedAMD;
	PFNGLBLENDEQUATIONSEPARATEIPROC pGlBlendEquationSeparateiARB;

	// DSA
	PFNGLTEXTURESTORAGE2DPROC pGlTextureStorage2D;
	PFNGLTEXTURESTORAGE3DPROC pGlTextureStorage3D;
	PFNGLTEXTURESUBIMAGE2DPROC pGlTextureSubImage2D;
	PFNGLGETTEXTUREIMAGEPROC pGlGetTextureImage;
	PFNGLNAMEDFRAMEBUFFERTEXTUREPROC pGlNamedFramebufferTexture;
	PFNGLTEXTUREPARAMETERIPROC pGlTextureParameteri;
	PFNGLTEXTUREPARAMETERFPROC pGlTextureParameterf;
	PFNGLTEXTUREPARAMETERIVPROC pGlTextureParameteriv;
	PFNGLTEXTUREPARAMETERFVPROC pGlTextureParameterfv;

	PFNGLCREATETEXTURESPROC pGlCreateTextures;
	PFNGLCREATEFRAMEBUFFERSPROC pGlCreateFramebuffers;
	PFNGLBINDTEXTURESPROC pGlBindTextures;
	PFNGLGENERATETEXTUREMIPMAPPROC pGlGenerateTextureMipmap;
	// DSA with EXT or functions to simulate it
	PFNGLTEXTURESTORAGE2DEXTPROC pGlTextureStorage2DEXT;
	PFNGLTEXSTORAGE2DPROC pGlTexStorage2D;
	PFNGLTEXTURESTORAGE3DEXTPROC pGlTextureStorage3DEXT;
	PFNGLTEXSTORAGE3DPROC pGlTexStorage3D;
	PFNGLTEXTURESUBIMAGE2DEXTPROC pGlTextureSubImage2DEXT;
	PFNGLGETTEXTUREIMAGEEXTPROC pGlGetTextureImageEXT;
	PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC pGlNamedFramebufferTextureEXT;
	PFNGLFRAMEBUFFERTEXTUREPROC pGlFramebufferTexture;
	PFNGLGENERATETEXTUREMIPMAPEXTPROC pGlGenerateTextureMipmapEXT;

#if defined(WGL_EXT_swap_control)
	PFNWGLSWAPINTERVALEXTPROC pWglSwapIntervalEXT;
#endif
#if defined(GLX_SGI_swap_control)
	PFNGLXSWAPINTERVALSGIPROC pGlxSwapIntervalSGI;
#endif
#if defined(GLX_EXT_swap_control)
	PFNGLXSWAPINTERVALEXTPROC pGlxSwapIntervalEXT;
#endif
#if defined(GLX_MESA_swap_control)
	PFNGLXSWAPINTERVALMESAPROC pGlxSwapIntervalMESA;
#endif
};

inline void COpenGLExtensionHandler::irrGlActiveTexture(GLenum texture)
{
	if (pGlActiveTexture)
		pGlActiveTexture(texture);
	else if (pGlActiveTextureARB)
		pGlActiveTextureARB(texture);
}

inline void COpenGLExtensionHandler::irrGlClientActiveTexture(GLenum texture)
{
	if (pGlClientActiveTextureARB)
		pGlClientActiveTextureARB(texture);
}

inline void COpenGLExtensionHandler::extGlGenPrograms(GLsizei n, GLuint *programs)
{
	if (programs)
		memset(programs, 0, n * sizeof(GLuint));
	if (pGlGenProgramsARB)
		pGlGenProgramsARB(n, programs);
	else if (pGlGenProgramsNV)
		pGlGenProgramsNV(n, programs);
}

inline void COpenGLExtensionHandler::extGlBindProgram(GLenum target, GLuint program)
{
	if (pGlBindProgramARB)
		pGlBindProgramARB(target, program);
	else if (pGlBindProgramNV)
		pGlBindProgramNV(target, program);
}

inline void COpenGLExtensionHandler::extGlProgramString(GLenum target, GLenum format, GLsizei len, const GLvoid *string)
{
	if (pGlProgramStringARB)
		pGlProgramStringARB(target, format, len, string);
}

inline void COpenGLExtensionHandler::extGlLoadProgram(GLenum target, GLuint id, GLsizei len, const GLubyte *string)
{
	if (pGlLoadProgramNV)
		pGlLoadProgramNV(target, id, len, string);
}

inline void COpenGLExtensionHandler::extGlDeletePrograms(GLsizei n, const GLuint *programs)
{
	if (pGlDeleteProgramsARB)
		pGlDeleteProgramsARB(n, programs);
	else if (pGlDeleteProgramsNV)
		pGlDeleteProgramsNV(n, programs);
}

inline void COpenGLExtensionHandler::extGlProgramLocalParameter4fv(GLenum n, GLuint i, const GLfloat *f)
{
	if (pGlProgramLocalParameter4fvARB)
		pGlProgramLocalParameter4fvARB(n, i, f);
}

inline GLhandleARB COpenGLExtensionHandler::extGlCreateShaderObject(GLenum shaderType)
{
	if (pGlCreateShaderObjectARB)
		return pGlCreateShaderObjectARB(shaderType);
	return 0;
}

inline GLuint COpenGLExtensionHandler::extGlCreateShader(GLenum shaderType)
{
	if (pGlCreateShader)
		return pGlCreateShader(shaderType);
	return 0;
}

inline void COpenGLExtensionHandler::extGlShaderSourceARB(GLhandleARB shader, GLsizei numOfStrings, const char **strings, const GLint *lenOfStrings)
{
	if (pGlShaderSourceARB)
		pGlShaderSourceARB(shader, numOfStrings, strings, lenOfStrings);
}

inline void COpenGLExtensionHandler::extGlShaderSource(GLuint shader, GLsizei numOfStrings, const char **strings, const GLint *lenOfStrings)
{
	if (pGlShaderSource)
		pGlShaderSource(shader, numOfStrings, strings, lenOfStrings);
}

inline void COpenGLExtensionHandler::extGlCompileShaderARB(GLhandleARB shader)
{
	if (pGlCompileShaderARB)
		pGlCompileShaderARB(shader);
}

inline void COpenGLExtensionHandler::extGlCompileShader(GLuint shader)
{
	if (pGlCompileShader)
		pGlCompileShader(shader);
}

inline GLhandleARB COpenGLExtensionHandler::extGlCreateProgramObject(void)
{
	if (pGlCreateProgramObjectARB)
		return pGlCreateProgramObjectARB();
	return 0;
}

inline GLuint COpenGLExtensionHandler::extGlCreateProgram(void)
{
	if (pGlCreateProgram)
		return pGlCreateProgram();
	return 0;
}

inline void COpenGLExtensionHandler::extGlAttachObject(GLhandleARB program, GLhandleARB shader)
{
	if (pGlAttachObjectARB)
		pGlAttachObjectARB(program, shader);
}

inline void COpenGLExtensionHandler::extGlAttachShader(GLuint program, GLuint shader)
{
	if (pGlAttachShader)
		pGlAttachShader(program, shader);
}

inline void COpenGLExtensionHandler::extGlLinkProgramARB(GLhandleARB program)
{
	if (pGlLinkProgramARB)
		pGlLinkProgramARB(program);
}

inline void COpenGLExtensionHandler::extGlLinkProgram(GLuint program)
{
	if (pGlLinkProgram)
		pGlLinkProgram(program);
}

inline void COpenGLExtensionHandler::extGlUseProgramObject(GLhandleARB prog)
{
	if (pGlUseProgramObjectARB)
		pGlUseProgramObjectARB(prog);
}

inline void COpenGLExtensionHandler::irrGlUseProgram(GLuint prog)
{
	if (pGlUseProgram)
		pGlUseProgram(prog);
}

inline void COpenGLExtensionHandler::extGlDeleteObject(GLhandleARB object)
{
	if (pGlDeleteObjectARB)
		pGlDeleteObjectARB(object);
}

inline void COpenGLExtensionHandler::extGlDeleteProgram(GLuint object)
{
	if (pGlDeleteProgram)
		pGlDeleteProgram(object);
}

inline void COpenGLExtensionHandler::extGlDeleteShader(GLuint shader)
{
	if (pGlDeleteShader)
		pGlDeleteShader(shader);
}

inline void COpenGLExtensionHandler::extGlGetAttachedObjects(GLhandleARB program, GLsizei maxcount, GLsizei *count, GLhandleARB *shaders)
{
	if (count)
		*count = 0;
	if (pGlGetAttachedObjectsARB)
		pGlGetAttachedObjectsARB(program, maxcount, count, shaders);
}

inline void COpenGLExtensionHandler::extGlGetAttachedShaders(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders)
{
	if (count)
		*count = 0;
	if (pGlGetAttachedShaders)
		pGlGetAttachedShaders(program, maxcount, count, shaders);
}

inline void COpenGLExtensionHandler::extGlGetInfoLog(GLhandleARB object, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog)
{
	if (length)
		*length = 0;
	if (pGlGetInfoLogARB)
		pGlGetInfoLogARB(object, maxLength, length, infoLog);
}

inline void COpenGLExtensionHandler::extGlGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog)
{
	if (length)
		*length = 0;
	if (pGlGetShaderInfoLog)
		pGlGetShaderInfoLog(shader, maxLength, length, infoLog);
}

inline void COpenGLExtensionHandler::extGlGetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog)
{
	if (length)
		*length = 0;
	if (pGlGetProgramInfoLog)
		pGlGetProgramInfoLog(program, maxLength, length, infoLog);
}

inline void COpenGLExtensionHandler::extGlGetObjectParameteriv(GLhandleARB object, GLenum type, GLint *param)
{
	if (pGlGetObjectParameterivARB)
		pGlGetObjectParameterivARB(object, type, param);
}

inline void COpenGLExtensionHandler::extGlGetShaderiv(GLuint shader, GLenum type, GLint *param)
{
	if (pGlGetShaderiv)
		pGlGetShaderiv(shader, type, param);
}

inline void COpenGLExtensionHandler::extGlGetProgramiv(GLuint program, GLenum type, GLint *param)
{
	if (pGlGetProgramiv)
		pGlGetProgramiv(program, type, param);
}

inline GLint COpenGLExtensionHandler::extGlGetUniformLocationARB(GLhandleARB program, const char *name)
{
	if (pGlGetUniformLocationARB)
		return pGlGetUniformLocationARB(program, name);
	return 0;
}

inline GLint COpenGLExtensionHandler::extGlGetUniformLocation(GLuint program, const char *name)
{
	if (pGlGetUniformLocation)
		return pGlGetUniformLocation(program, name);
	return 0;
}

inline void COpenGLExtensionHandler::extGlUniform1fv(GLint loc, GLsizei count, const GLfloat *v)
{
	if (pGlUniform1fvARB)
		pGlUniform1fvARB(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform2fv(GLint loc, GLsizei count, const GLfloat *v)
{
	if (pGlUniform2fvARB)
		pGlUniform2fvARB(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform3fv(GLint loc, GLsizei count, const GLfloat *v)
{
	if (pGlUniform3fvARB)
		pGlUniform3fvARB(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform4fv(GLint loc, GLsizei count, const GLfloat *v)
{
	if (pGlUniform4fvARB)
		pGlUniform4fvARB(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform1uiv(GLint loc, GLsizei count, const GLuint *v)
{
	if (pGlUniform1uiv)
		pGlUniform1uiv(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform2uiv(GLint loc, GLsizei count, const GLuint *v)
{
	if (pGlUniform2uiv)
		pGlUniform2uiv(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform3uiv(GLint loc, GLsizei count, const GLuint *v)
{
	if (pGlUniform3uiv)
		pGlUniform3uiv(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform4uiv(GLint loc, GLsizei count, const GLuint *v)
{
	if (pGlUniform4uiv)
		pGlUniform4uiv(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform1iv(GLint loc, GLsizei count, const GLint *v)
{
	if (pGlUniform1ivARB)
		pGlUniform1ivARB(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform2iv(GLint loc, GLsizei count, const GLint *v)
{
	if (pGlUniform2ivARB)
		pGlUniform2ivARB(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform3iv(GLint loc, GLsizei count, const GLint *v)
{
	if (pGlUniform3ivARB)
		pGlUniform3ivARB(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniform4iv(GLint loc, GLsizei count, const GLint *v)
{
	if (pGlUniform4ivARB)
		pGlUniform4ivARB(loc, count, v);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix2fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix2fvARB)
		pGlUniformMatrix2fvARB(loc, count, transpose, v);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix2x3fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix2x3fv)
		pGlUniformMatrix2x3fv(loc, count, transpose, v);
	else
		os::Printer::log("glUniformMatrix2x3fv not supported", ELL_ERROR);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix2x4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix2x4fv)
		pGlUniformMatrix2x4fv(loc, count, transpose, v);
	else
		os::Printer::log("glUniformMatrix2x4fv not supported", ELL_ERROR);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix3x2fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix3x2fv)
		pGlUniformMatrix3x2fv(loc, count, transpose, v);
	else
		os::Printer::log("glUniformMatrix3x2fv not supported", ELL_ERROR);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix3fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix3fvARB)
		pGlUniformMatrix3fvARB(loc, count, transpose, v);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix3x4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix3x4fv)
		pGlUniformMatrix3x4fv(loc, count, transpose, v);
	else
		os::Printer::log("glUniformMatrix3x4fv not supported", ELL_ERROR);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix4x2fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix4x2fv)
		pGlUniformMatrix4x2fv(loc, count, transpose, v);
	else
		os::Printer::log("glUniformMatrix4x2fv not supported", ELL_ERROR);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix4x3fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix4x3fv)
		pGlUniformMatrix4x3fv(loc, count, transpose, v);
	else
		os::Printer::log("glUniformMatrix4x3fv not supported", ELL_ERROR);
}

inline void COpenGLExtensionHandler::extGlUniformMatrix4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat *v)
{
	if (pGlUniformMatrix4fvARB)
		pGlUniformMatrix4fvARB(loc, count, transpose, v);
}

inline void COpenGLExtensionHandler::extGlGetActiveUniformARB(GLhandleARB program,
		GLuint index, GLsizei maxlength, GLsizei *length,
		GLint *size, GLenum *type, GLcharARB *name)
{
	if (length)
		*length = 0;
	if (pGlGetActiveUniformARB)
		pGlGetActiveUniformARB(program, index, maxlength, length, size, type, name);
}

inline void COpenGLExtensionHandler::extGlGetActiveUniform(GLuint program,
		GLuint index, GLsizei maxlength, GLsizei *length,
		GLint *size, GLenum *type, GLchar *name)
{
	if (length)
		*length = 0;
	if (pGlGetActiveUniform)
		pGlGetActiveUniform(program, index, maxlength, length, size, type, name);
}

inline void COpenGLExtensionHandler::extGlPointParameterf(GLint loc, GLfloat f)
{
	if (pGlPointParameterfARB)
		pGlPointParameterfARB(loc, f);
}

inline void COpenGLExtensionHandler::extGlPointParameterfv(GLint loc, const GLfloat *v)
{
	if (pGlPointParameterfvARB)
		pGlPointParameterfvARB(loc, v);
}

inline void COpenGLExtensionHandler::extGlStencilFuncSeparate(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask)
{
	if (pGlStencilFuncSeparate)
		pGlStencilFuncSeparate(frontfunc, backfunc, ref, mask);
	else if (pGlStencilFuncSeparateATI)
		pGlStencilFuncSeparateATI(frontfunc, backfunc, ref, mask);
}

inline void COpenGLExtensionHandler::extGlStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
	if (pGlStencilOpSeparate)
		pGlStencilOpSeparate(face, fail, zfail, zpass);
	else if (pGlStencilOpSeparateATI)
		pGlStencilOpSeparateATI(face, fail, zfail, zpass);
}

inline void COpenGLExtensionHandler::irrGlCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
		GLsizei height, GLint border, GLsizei imageSize, const void *data)
{
	if (pGlCompressedTexImage2D)
		pGlCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

inline void COpenGLExtensionHandler::irrGlCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
		GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
	if (pGlCompressedTexSubImage2D)
		pGlCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

inline void COpenGLExtensionHandler::irrGlBindFramebuffer(GLenum target, GLuint framebuffer)
{
	if (pGlBindFramebuffer)
		pGlBindFramebuffer(target, framebuffer);
	else if (pGlBindFramebufferEXT)
		pGlBindFramebufferEXT(target, framebuffer);
}

inline void COpenGLExtensionHandler::irrGlDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	if (pGlDeleteFramebuffers)
		pGlDeleteFramebuffers(n, framebuffers);
	else if (pGlDeleteFramebuffersEXT)
		pGlDeleteFramebuffersEXT(n, framebuffers);
}

inline void COpenGLExtensionHandler::irrGlGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	if (framebuffers)
		memset(framebuffers, 0, n * sizeof(GLuint));
	if (pGlGenFramebuffers)
		pGlGenFramebuffers(n, framebuffers);
	else if (pGlGenFramebuffersEXT)
		pGlGenFramebuffersEXT(n, framebuffers);
}

inline GLenum COpenGLExtensionHandler::irrGlCheckFramebufferStatus(GLenum target)
{
	if (pGlCheckFramebufferStatus)
		return pGlCheckFramebufferStatus(target);
	else if (pGlCheckFramebufferStatusEXT)
		return pGlCheckFramebufferStatusEXT(target);
	else
		return 0;
}

inline void COpenGLExtensionHandler::irrGlFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	if (pGlFramebufferTexture2D)
		pGlFramebufferTexture2D(target, attachment, textarget, texture, level);
	else if (pGlFramebufferTexture2DEXT)
		pGlFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
}

inline void COpenGLExtensionHandler::irrGlBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
	if (pGlBindRenderbuffer)
		pGlBindRenderbuffer(target, renderbuffer);
	else if (pGlBindRenderbufferEXT)
		pGlBindRenderbufferEXT(target, renderbuffer);
}

inline void COpenGLExtensionHandler::irrGlDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
	if (pGlDeleteRenderbuffers)
		pGlDeleteRenderbuffers(n, renderbuffers);
	else if (pGlDeleteRenderbuffersEXT)
		pGlDeleteRenderbuffersEXT(n, renderbuffers);
}

inline void COpenGLExtensionHandler::irrGlGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
	if (renderbuffers)
		memset(renderbuffers, 0, n * sizeof(GLuint));
	if (pGlGenRenderbuffers)
		pGlGenRenderbuffers(n, renderbuffers);
	else if (pGlGenRenderbuffersEXT)
		pGlGenRenderbuffersEXT(n, renderbuffers);
}

inline void COpenGLExtensionHandler::irrGlRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	if (pGlRenderbufferStorage)
		pGlRenderbufferStorage(target, internalformat, width, height);
	else if (pGlRenderbufferStorageEXT)
		pGlRenderbufferStorageEXT(target, internalformat, width, height);
}

inline void COpenGLExtensionHandler::irrGlFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	if (pGlFramebufferRenderbuffer)
		pGlFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
	else if (pGlFramebufferRenderbufferEXT)
		pGlFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, renderbuffer);
}

inline void COpenGLExtensionHandler::irrGlGenerateMipmap(GLenum target)
{
	if (pGlGenerateMipmap)
		pGlGenerateMipmap(target);
	else if (pGlGenerateMipmapEXT)
		pGlGenerateMipmapEXT(target);
}

inline void COpenGLExtensionHandler::irrGlActiveStencilFace(GLenum face)
{
	if (pGlActiveStencilFaceEXT)
		pGlActiveStencilFaceEXT(face);
}

inline void COpenGLExtensionHandler::irrGlDrawBuffer(GLenum mode)
{
	glDrawBuffer(mode);
}

inline void COpenGLExtensionHandler::irrGlDrawBuffers(GLsizei n, const GLenum *bufs)
{
	if (pGlDrawBuffersARB)
		pGlDrawBuffersARB(n, bufs);
	else if (pGlDrawBuffersATI)
		pGlDrawBuffersATI(n, bufs);
}

inline void COpenGLExtensionHandler::extGlGenBuffers(GLsizei n, GLuint *buffers)
{
	if (buffers)
		memset(buffers, 0, n * sizeof(GLuint));
	if (pGlGenBuffersARB)
		pGlGenBuffersARB(n, buffers);
}

inline void COpenGLExtensionHandler::extGlBindBuffer(GLenum target, GLuint buffer)
{
	if (pGlBindBufferARB)
		pGlBindBufferARB(target, buffer);
}

inline void COpenGLExtensionHandler::extGlBufferData(GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage)
{
	if (pGlBufferDataARB)
		pGlBufferDataARB(target, size, data, usage);
}

inline void COpenGLExtensionHandler::extGlDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	if (pGlDeleteBuffersARB)
		pGlDeleteBuffersARB(n, buffers);
}

inline void COpenGLExtensionHandler::extGlBufferSubData(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data)
{
	if (pGlBufferSubDataARB)
		pGlBufferSubDataARB(target, offset, size, data);
}

inline void COpenGLExtensionHandler::extGlGetBufferSubData(GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data)
{
	if (pGlGetBufferSubDataARB)
		pGlGetBufferSubDataARB(target, offset, size, data);
}

inline void *COpenGLExtensionHandler::extGlMapBuffer(GLenum target, GLenum access)
{
	if (pGlMapBufferARB)
		return pGlMapBufferARB(target, access);
	return 0;
}

inline GLboolean COpenGLExtensionHandler::extGlUnmapBuffer(GLenum target)
{
	if (pGlUnmapBufferARB)
		return pGlUnmapBufferARB(target);
	return false;
}

inline GLboolean COpenGLExtensionHandler::extGlIsBuffer(GLuint buffer)
{
	if (pGlIsBufferARB)
		return pGlIsBufferARB(buffer);
	return false;
}

inline void COpenGLExtensionHandler::extGlGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
	if (pGlGetBufferParameterivARB)
		pGlGetBufferParameterivARB(target, pname, params);
}

inline void COpenGLExtensionHandler::extGlGetBufferPointerv(GLenum target, GLenum pname, GLvoid **params)
{
	if (pGlGetBufferPointervARB)
		pGlGetBufferPointervARB(target, pname, params);
}

inline void COpenGLExtensionHandler::extGlProvokingVertex(GLenum mode)
{
	if (FeatureAvailable[IRR_ARB_provoking_vertex] && pGlProvokingVertexARB)
		pGlProvokingVertexARB(mode);
	else if (FeatureAvailable[IRR_EXT_provoking_vertex] && pGlProvokingVertexEXT)
		pGlProvokingVertexEXT(mode);
}

inline void COpenGLExtensionHandler::extGlProgramParameteri(GLuint program, GLenum pname, GLint value)
{
	if (queryFeature(EVDF_GEOMETRY_SHADER)) {
		if (pGlProgramParameteriARB)
			pGlProgramParameteriARB(program, pname, value);
		else if (pGlProgramParameteriEXT)
			pGlProgramParameteriEXT(program, pname, value);
	}
}

inline void COpenGLExtensionHandler::extGlGenQueries(GLsizei n, GLuint *ids)
{
	if (pGlGenQueriesARB)
		pGlGenQueriesARB(n, ids);
	else if (pGlGenOcclusionQueriesNV)
		pGlGenOcclusionQueriesNV(n, ids);
}

inline void COpenGLExtensionHandler::extGlDeleteQueries(GLsizei n, const GLuint *ids)
{
	if (pGlDeleteQueriesARB)
		pGlDeleteQueriesARB(n, ids);
	else if (pGlDeleteOcclusionQueriesNV)
		pGlDeleteOcclusionQueriesNV(n, ids);
}

inline GLboolean COpenGLExtensionHandler::extGlIsQuery(GLuint id)
{
	if (pGlIsQueryARB)
		return pGlIsQueryARB(id);
	else if (pGlIsOcclusionQueryNV)
		return pGlIsOcclusionQueryNV(id);
	return false;
}

inline void COpenGLExtensionHandler::extGlBeginQuery(GLenum target, GLuint id)
{
	if (pGlBeginQueryARB)
		pGlBeginQueryARB(target, id);
	else if (pGlBeginOcclusionQueryNV)
		pGlBeginOcclusionQueryNV(id);
}

inline void COpenGLExtensionHandler::extGlEndQuery(GLenum target)
{
	if (pGlEndQueryARB)
		pGlEndQueryARB(target);
	else if (pGlEndOcclusionQueryNV)
		pGlEndOcclusionQueryNV();
}

inline void COpenGLExtensionHandler::extGlGetQueryiv(GLenum target, GLenum pname, GLint *params)
{
	if (pGlGetQueryivARB)
		pGlGetQueryivARB(target, pname, params);
}

inline void COpenGLExtensionHandler::extGlGetQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
	if (pGlGetQueryObjectivARB)
		pGlGetQueryObjectivARB(id, pname, params);
	else if (pGlGetOcclusionQueryivNV)
		pGlGetOcclusionQueryivNV(id, pname, params);
}

inline void COpenGLExtensionHandler::extGlGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
	if (pGlGetQueryObjectuivARB)
		pGlGetQueryObjectuivARB(id, pname, params);
	else if (pGlGetOcclusionQueryuivNV)
		pGlGetOcclusionQueryuivNV(id, pname, params);
}

inline void COpenGLExtensionHandler::irrGlBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
	if (pGlBlendFuncSeparate)
		pGlBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
	else if (pGlBlendFuncSeparateEXT)
		pGlBlendFuncSeparateEXT(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

inline void COpenGLExtensionHandler::irrGlBlendEquation(GLenum mode)
{
	if (pGlBlendEquation)
		pGlBlendEquation(mode);
	else if (pGlBlendEquationEXT)
		pGlBlendEquationEXT(mode);
}

inline void COpenGLExtensionHandler::irrGlEnableIndexed(GLenum target, GLuint index)
{
	if (FeatureAvailable[IRR_EXT_draw_buffers2] && pGlEnableIndexedEXT)
		pGlEnableIndexedEXT(target, index);
}

inline void COpenGLExtensionHandler::irrGlDisableIndexed(GLenum target, GLuint index)
{
	if (FeatureAvailable[IRR_EXT_draw_buffers2] && pGlDisableIndexedEXT)
		pGlDisableIndexedEXT(target, index);
}

inline void COpenGLExtensionHandler::irrGlColorMaskIndexed(GLuint buf, GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
	if (FeatureAvailable[IRR_EXT_draw_buffers2] && pGlColorMaskIndexedEXT)
		pGlColorMaskIndexedEXT(buf, r, g, b, a);
}

inline void COpenGLExtensionHandler::irrGlBlendFuncIndexed(GLuint buf, GLenum src, GLenum dst)
{
	if (FeatureAvailable[IRR_ARB_draw_buffers_blend] && pGlBlendFunciARB)
		pGlBlendFunciARB(buf, src, dst);
	else if (FeatureAvailable[IRR_AMD_draw_buffers_blend] && pGlBlendFuncIndexedAMD)
		pGlBlendFuncIndexedAMD(buf, src, dst);
}

inline void COpenGLExtensionHandler::irrGlBlendFuncSeparateIndexed(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
	if (FeatureAvailable[IRR_ARB_draw_buffers_blend] && pGlBlendFuncSeparateiARB)
		pGlBlendFuncSeparateiARB(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
	else if (FeatureAvailable[IRR_AMD_draw_buffers_blend] && pGlBlendFuncSeparateIndexedAMD)
		pGlBlendFuncSeparateIndexedAMD(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

inline void COpenGLExtensionHandler::irrGlBlendEquationIndexed(GLuint buf, GLenum mode)
{
	if (FeatureAvailable[IRR_ARB_draw_buffers_blend] && pGlBlendEquationiARB)
		pGlBlendEquationiARB(buf, mode);
	else if (FeatureAvailable[IRR_AMD_draw_buffers_blend] && pGlBlendEquationIndexedAMD)
		pGlBlendEquationIndexedAMD(buf, mode);
}

inline void COpenGLExtensionHandler::irrGlBlendEquationSeparateIndexed(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
	if (FeatureAvailable[IRR_ARB_draw_buffers_blend] && pGlBlendEquationSeparateiARB)
		pGlBlendEquationSeparateiARB(buf, modeRGB, modeAlpha);
	else if (FeatureAvailable[IRR_AMD_draw_buffers_blend] && pGlBlendEquationSeparateIndexedAMD)
		pGlBlendEquationSeparateIndexedAMD(buf, modeRGB, modeAlpha);
}

inline void COpenGLExtensionHandler::extGlTextureSubImage2D(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
	if (Version >= 405 || FeatureAvailable[IRR_ARB_direct_state_access]) {
		if (pGlTextureSubImage2D)
			pGlTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, type, pixels);
	} else if (FeatureAvailable[IRR_EXT_direct_state_access]) {
		if (pGlTextureSubImage2DEXT)
			pGlTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, type, pixels);
	} else {
		GLint bound;
		switch (target) {
#ifdef GL_VERSION_3_0
		case GL_TEXTURE_1D_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY, &bound);
			break;
#elif GL_EXT_texture_array
		case GL_TEXTURE_1D_ARRAY_EXT:
			glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY_EXT, &bound);
			break;
#endif
		case GL_TEXTURE_2D:
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
			break;
#ifdef GL_VERSION_3_2
		case GL_TEXTURE_2D_MULTISAMPLE:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &bound);
			break;
#endif
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &bound);
			break;
#ifdef GL_VERSION_3_1
		case GL_TEXTURE_RECTANGLE:
			glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &bound);
			break;
#elif defined(GL_ARB_texture_rectangle)
		case GL_TEXTURE_RECTANGLE_ARB:
			glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &bound);
			break;
#endif
		default:
			return;
		}
		glBindTexture(target, texture);
		glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
		glBindTexture(target, bound);
	}
}

inline void COpenGLExtensionHandler::extGlTextureStorage2D(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	if (Version >= 405 || FeatureAvailable[IRR_ARB_direct_state_access]) {
		if (pGlTextureStorage2D)
			pGlTextureStorage2D(texture, levels, internalformat, width, height);
	} else if (FeatureAvailable[IRR_EXT_direct_state_access]) {
		if (pGlTextureStorage2DEXT)
			pGlTextureStorage2DEXT(texture, target, levels, internalformat, width, height);
	}

	else if (pGlTexStorage2D) {
		GLint bound;
		switch (target) {
#ifdef GL_VERSION_3_0
		case GL_TEXTURE_1D_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY, &bound);
			break;
#elif GL_EXT_texture_array
		case GL_TEXTURE_1D_ARRAY_EXT:
			glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY_EXT, &bound);
			break;
#endif
		case GL_TEXTURE_2D:
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
			break;
		case GL_TEXTURE_CUBE_MAP:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &bound);
			break;
#ifdef GL_VERSION_3_1
		case GL_TEXTURE_RECTANGLE:
			glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &bound);
			break;
#elif defined(GL_ARB_texture_rectangle)
		case GL_TEXTURE_RECTANGLE_ARB:
			glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &bound);
			break;
#endif
		default:
			return;
		}
		glBindTexture(target, texture);
		pGlTexStorage2D(target, levels, internalformat, width, height);
		glBindTexture(target, bound);
	}
}

inline void COpenGLExtensionHandler::extGlTextureStorage3D(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	if (Version >= 405 || FeatureAvailable[IRR_ARB_direct_state_access]) {
		if (pGlTextureStorage3D)
			pGlTextureStorage3D(texture, levels, internalformat, width, height, depth);
	} else if (FeatureAvailable[IRR_EXT_direct_state_access]) {
		if (pGlTextureStorage3DEXT)
			pGlTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
	} else if (pGlTexStorage3D) {
		GLint bound;
		switch (target) {
#ifdef GL_VERSION_3_0
		case GL_TEXTURE_2D_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &bound);
			break;
#elif GL_EXT_texture_array
		case GL_TEXTURE_2D_ARRAY_EXT:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY_EXT, &bound);
			break;
#endif
		case GL_TEXTURE_3D:
			glGetIntegerv(GL_TEXTURE_BINDING_3D, &bound);
			break;
#ifdef GL_VERSION_4_0
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &bound);
			break;
#elif defined(GL_ARB_texture_cube_map_array)
		case GL_TEXTURE_CUBE_MAP_ARRAY_ARB:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_ARB, &bound);
			break;
#endif
		default:
			return;
		}
		glBindTexture(target, texture);
		pGlTexStorage3D(target, levels, internalformat, width, height, depth);
		glBindTexture(target, bound);
	}
}

inline void COpenGLExtensionHandler::extGlGetTextureImage(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
	if (Version >= 405 || FeatureAvailable[IRR_ARB_direct_state_access]) {
		if (pGlGetTextureImage)
			pGlGetTextureImage(texture, level, format, type, bufSize, pixels);
	} else if (FeatureAvailable[IRR_EXT_direct_state_access]) {
		if (pGlGetTextureImageEXT)
			pGlGetTextureImageEXT(texture, target, level, format, type, pixels);
	} else {
		GLint bound;
		switch (target) {
#ifdef GL_VERSION_3_0
		case GL_TEXTURE_2D_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &bound);
			break;
#elif GL_EXT_texture_array
		case GL_TEXTURE_2D_ARRAY_EXT:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY_EXT, &bound);
			break;
#endif
		case GL_TEXTURE_3D:
			glGetIntegerv(GL_TEXTURE_BINDING_3D, &bound);
			break;
#ifdef GL_VERSION_4_0
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &bound);
			break;
#elif defined(GL_ARB_texture_cube_map_array)
		case GL_TEXTURE_CUBE_MAP_ARRAY_ARB:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_ARB, &bound);
			break;
#endif
		default:
			return;
		}
		glBindTexture(target, texture);
		glGetTexImage(target, level, format, type, pixels);
		glBindTexture(target, bound);
	}
}

inline void COpenGLExtensionHandler::extGlNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
	if (!needsDSAFramebufferHack) {
		if (Version >= 405 || FeatureAvailable[IRR_ARB_direct_state_access]) {
			pGlNamedFramebufferTexture(framebuffer, attachment, texture, level);
			return;
		} else if (FeatureAvailable[IRR_EXT_direct_state_access]) {
			pGlNamedFramebufferTextureEXT(framebuffer, attachment, texture, level);
			return;
		}
	}

	GLuint bound;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&bound));

	if (bound != framebuffer)
		pGlBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	pGlFramebufferTexture(GL_FRAMEBUFFER, attachment, texture, level);
	if (bound != framebuffer)
		pGlBindFramebuffer(GL_FRAMEBUFFER, bound);
}

inline void COpenGLExtensionHandler::extGlTextureParameteri(GLuint texture, GLenum pname, GLint param)
{
	if (pGlTextureParameteri)
		pGlTextureParameteri(texture, pname, param);
}

inline void COpenGLExtensionHandler::extGlTextureParameterf(GLuint texture, GLenum pname, GLfloat param)
{
	if (pGlTextureParameterf)
		pGlTextureParameterf(texture, pname, param);
}

inline void COpenGLExtensionHandler::extGlTextureParameteriv(GLuint texture, GLenum pname, const GLint *params)
{
	if (pGlTextureParameteriv)
		pGlTextureParameteriv(texture, pname, params);
}

inline void COpenGLExtensionHandler::extGlTextureParameterfv(GLuint texture, GLenum pname, const GLfloat *params)
{
	if (pGlTextureParameterfv)
		pGlTextureParameterfv(texture, pname, params);
}

inline void COpenGLExtensionHandler::extGlCreateTextures(GLenum target, GLsizei n, GLuint *textures)
{
	if (Version >= 405) {
		if (pGlCreateTextures)
			pGlCreateTextures(target, n, textures);
		else if (textures)
			memset(textures, 0, n * sizeof(GLuint));
	} else {
		glGenTextures(n, textures);
	}
}

inline void COpenGLExtensionHandler::extGlCreateFramebuffers(GLsizei n, GLuint *framebuffers)
{
	if (!needsDSAFramebufferHack) {
		if (Version >= 405) {
			pGlCreateFramebuffers(n, framebuffers);
			return;
		}
	}

	pGlGenFramebuffers(n, framebuffers);
}

inline void COpenGLExtensionHandler::extGlBindTextures(GLuint first, GLsizei count, const GLuint *textures, const GLenum *targets)
{
	const GLenum supportedTargets[] = {GL_TEXTURE_1D, GL_TEXTURE_2D // GL 1.x
			,
			GL_TEXTURE_3D // GL 2.x
#ifdef GL_VERSION_3_1
			,
			GL_TEXTURE_RECTANGLE
#elif defined(GL_ARB_texture_rectangle)
			,
			GL_TEXTURE_RECTANGLE_ARB
#endif
			,
			GL_TEXTURE_CUBE_MAP
#ifdef GL_VERSION_3_0
			,
			GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY // GL 3.x
#elif GL_EXT_texture_array
			,
			GL_TEXTURE_1D_ARRAY_EXT, GL_TEXTURE_2D_ARRAY_EXT
#endif
#ifdef GL_VERSION_3_1
			,
			GL_TEXTURE_BUFFER
#elif defined(GL_ARB_texture_buffer_object)
			,
			GL_TEXTURE_BUFFER_ARB
#elif defined(GL_EXT_texture_buffer_object)
			,
			GL_TEXTURE_BUFFER_EXT
#endif
#ifdef GL_VERSION_4_0
			,
			GL_TEXTURE_CUBE_MAP_ARRAY // GL 4.x
#elif defined(GL_ARB_texture_cube_map_array)
			,
			GL_TEXTURE_CUBE_MAP_ARRAY_ARB
#endif
#ifdef GL_VERSION_3_2
			,
			GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE_ARRAY
#endif
	};

	if (Version >= 404 || FeatureAvailable[IRR_ARB_multi_bind]) {
		if (pGlBindTextures)
			pGlBindTextures(first, count, textures);
	} else {
		GLint activeTex = 0;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);

		for (GLsizei i = 0; i < count; i++) {
			GLuint texture = textures ? textures[i] : 0;

			GLuint unit = first + i;
			irrGlActiveTexture(GL_TEXTURE0 + unit);

			if (texture)
				glBindTexture(targets[i], texture);
			else {
				for (size_t j = 0; j < sizeof(supportedTargets) / sizeof(GLenum); j++)
					glBindTexture(supportedTargets[j], 0);
			}
		}

		irrGlActiveTexture(activeTex);
	}
}

inline void COpenGLExtensionHandler::extGlGenerateTextureMipmap(GLuint texture, GLenum target)
{
	if (Version >= 405 || FeatureAvailable[IRR_ARB_direct_state_access]) {
		if (pGlGenerateTextureMipmap)
			pGlGenerateTextureMipmap(texture);
	} else if (FeatureAvailable[IRR_EXT_direct_state_access]) {
		if (pGlGenerateTextureMipmapEXT)
			pGlGenerateTextureMipmapEXT(texture, target);
	} else if (pGlGenerateMipmap) {
		GLint bound;
		switch (target) {
		case GL_TEXTURE_1D:
			glGetIntegerv(GL_TEXTURE_BINDING_1D, &bound);
			break;
#ifdef GL_VERSION_3_0
		case GL_TEXTURE_1D_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY, &bound);
			break;
#elif GL_EXT_texture_array
		case GL_TEXTURE_1D_ARRAY_EXT:
			glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY_EXT, &bound);
			break;
#endif
		case GL_TEXTURE_2D:
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
			break;
#ifdef GL_VERSION_3_0
		case GL_TEXTURE_2D_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &bound);
			break;
#elif GL_EXT_texture_array
		case GL_TEXTURE_2D_ARRAY_EXT:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY_EXT, &bound);
			break;
#endif
#ifdef GL_VERSION_3_2
		case GL_TEXTURE_2D_MULTISAMPLE:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &bound);
			break;
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, &bound);
			break;
#endif
		case GL_TEXTURE_3D:
			glGetIntegerv(GL_TEXTURE_BINDING_3D, &bound);
			break;
#ifdef GL_VERSION_3_1
		case GL_TEXTURE_BUFFER:
			glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &bound);
			break;
#elif defined(GL_ARB_texture_buffer_object)
		case GL_TEXTURE_BUFFER_ARB:
			glGetIntegerv(GL_TEXTURE_BINDING_BUFFER_ARB, &bound);
			break;
#elif defined(GL_EXT_texture_buffer_object)
		case GL_TEXTURE_BUFFER_EXT:
			glGetIntegerv(GL_TEXTURE_BINDING_BUFFER_EXT, &bound);
			break;
#endif
		case GL_TEXTURE_CUBE_MAP:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &bound);
			break;
#ifdef GL_VERSION_4_0
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &bound);
			break;
#elif defined(GL_ARB_texture_cube_map_array)
		case GL_TEXTURE_CUBE_MAP_ARRAY_ARB:
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_ARB, &bound);
			break;
#endif
#ifdef GL_VERSION_3_1
		case GL_TEXTURE_RECTANGLE:
			glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &bound);
			break;
#elif defined(GL_ARB_texture_rectangle)
		case GL_TEXTURE_RECTANGLE_ARB:
			glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &bound);
			break;
#endif
		default:
			os::Printer::log("DevSH would like to ask you what are you doing!!??\n", ELL_ERROR);
			return;
		}
		glBindTexture(target, texture);
		pGlGenerateMipmap(target);
		glBindTexture(target, bound);
	}
}

inline void COpenGLExtensionHandler::extGlSwapInterval(int interval)
{
	// we have wglext, so try to use that
#if defined(_IRR_WINDOWS_API_) && defined(_IRR_COMPILE_WITH_WINDOWS_DEVICE_)
#ifdef WGL_EXT_swap_control
	if (pWglSwapIntervalEXT)
		pWglSwapIntervalEXT(interval);
#endif
#endif
#ifdef _IRR_COMPILE_WITH_X11_DEVICE_
#if defined(GLX_MESA_swap_control)
	if (pGlxSwapIntervalMESA)
		pGlxSwapIntervalMESA(interval);
#elif defined(GLX_EXT_swap_control)
	Display *dpy = glXGetCurrentDisplay();
	GLXDrawable drawable = glXGetCurrentDrawable();
	if (pGlxSwapIntervalEXT)
		pGlxSwapIntervalEXT(dpy, drawable, interval);
#elif defined(GLX_SGI_swap_control)
	// does not work with interval==0
	if (interval && pGlxSwapIntervalSGI)
		pGlxSwapIntervalSGI(interval);
}
#endif
#endif
}
}

#endif
