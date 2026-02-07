// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "clientmap.h"
#include "client.h"
#include "client/mesh.h"
#include "content_mapblock.h"
#include "mapblock_mesh.h"
#include <ICameraSceneNode.h>
#include <IMaterialRenderer.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <matrix4.h>
#include "mapsector.h"
#include "mapblock.h"
#include "node_visuals.h"
#include "nodedef.h"
#include "player.h" // CameraMode
#include "profiler.h"
#include "settings.h"
#include "camera.h"
#include "util/basic_macros.h"
#include "util/convex_polygon.h"
#include "util/tracy_wrapper.h"
#include "client/renderingengine.h"
#include <queue>

namespace {
	// data structure that groups block meshes by material
	struct MeshBufListMaps
	{
		// first = block pos
		using MeshBuf = std::pair<v3s16, scene::IMeshBuffer*>;

		using MeshBufList = std::vector<MeshBuf>;

		using MeshBufListMap = std::unordered_map<video::SMaterial, MeshBufList>;

		std::array<MeshBufListMap, MAX_TILE_LAYERS> maps;

		bool empty() const
		{
			for (auto &map : maps) {
				if (!map.empty())
					return false;
			}
			return true;
		}

		void clear()
		{
			for (auto &map : maps)
				map.clear();
		}

		void add(scene::IMeshBuffer *buf, v3s16 position, u8 layer)
		{
			assert(layer < MAX_TILE_LAYERS);

			// Append to the correct layer
			auto &map = maps[layer];
			const video::SMaterial &m = buf->getMaterial();
			auto &bufs = map[m]; // default constructs if non-existent
			bufs.emplace_back(position, buf);
		}

		void addFromBlock(v3s16 block_pos, MapBlockMesh *block_mesh,
			video::IVideoDriver *driver);
	};

	// reference to a mesh buffer used when rendering the map.
	struct DrawDescriptor {
		v3f m_pos; // world translation
		bool m_reuse_material:1;
		bool m_use_partial_buffer:1;
		union {
			scene::IMeshBuffer *m_buffer;
			const PartialMeshBuffer *m_partial_buffer;
		};

		DrawDescriptor(v3f pos, scene::IMeshBuffer *buffer, bool reuse_material = true) :
			m_pos(pos), m_reuse_material(reuse_material), m_use_partial_buffer(false),
			m_buffer(buffer)
		{}

		DrawDescriptor(v3f pos, const PartialMeshBuffer *buffer) :
			m_pos(pos), m_reuse_material(false), m_use_partial_buffer(true),
			m_partial_buffer(buffer)
		{}

		video::SMaterial &getMaterial();
		/// @return number of vertices drawn
		u32 draw(video::IVideoDriver* driver);
	};

	using DrawDescriptorList = std::vector<DrawDescriptor>;

	/// @brief Append vertices to a mesh buffer
	/// @note does not update bounding box!
	void appendToMeshBuffer(scene::SMeshBuffer *dst, const scene::IMeshBuffer *src, v3f translate)
	{
		const size_t vcount = dst->Vertices->Data.size();
		const size_t icount = dst->Indices->Data.size();

		assert(src->getVertexType() == video::EVT_STANDARD);
		const auto vptr = static_cast<const video::S3DVertex*>(src->getVertices());
		dst->Vertices->Data.insert(dst->Vertices->Data.end(),
			vptr, vptr + src->getVertexCount());
		// apply translation
		for (size_t j = vcount; j < dst->Vertices->Data.size(); j++)
			dst->Vertices->Data[j].Pos += translate;

		const auto iptr = src->getIndices();
		dst->Indices->Data.insert(dst->Indices->Data.end(),
			iptr, iptr + src->getIndexCount());
		// fixup indices
		if (vcount != 0) {
			for (size_t j = icount; j < dst->Indices->Data.size(); j++)
				dst->Indices->Data[j] += vcount;
		}
	}

	template <typename T>
	inline T subtract_or_zero(T a, T b) {
		return b >= a ? T(0) : (a - b);
	}

	// file-scope thread-local instances of the above two data structures, because
	// allocating memory in a hot path can be expensive.
	thread_local MeshBufListMaps tl_meshbuflistmaps;
	thread_local DrawDescriptorList tl_drawdescriptorlist;
}

void CachedMeshBuffer::drop()
{
	for (auto *it : buf)
		it->drop();
	buf.clear();
}

/*
	ClientMap
*/

static void on_settings_changed(const std::string &name, void *data)
{
	static_cast<ClientMap*>(data)->onSettingChanged(name, false);
}

static const std::string ClientMap_settings[] = {
	"trilinear_filter",
	"bilinear_filter",
	"anisotropic_filter",
	"transparency_sorting_group_by_buffers",
	"transparency_sorting_distance",
	"occlusion_culler",
	"enable_raytraced_culling",
};

ClientMap::ClientMap(
		Client *client,
		RenderingEngine *rendering_engine,
		MapDrawControl &control,
		s32 id
):
	Map(client),
	scene::ISceneNode(rendering_engine->get_scene_manager()->getRootSceneNode(),
		rendering_engine->get_scene_manager(), id),
	m_client(client),
	m_rendering_engine(rendering_engine),
	m_control(control),
	m_drawlist(MapBlockComparer(v3s16(0,0,0)))
{

	/*
	 * @Liso: Sadly C++ doesn't have introspection, so the only way we have to know
	 * the class is whith a name ;) Name property cames from ISceneNode base class.
	 */
	Name = "ClientMap";
	setAutomaticCulling(scene::EAC_OFF);

	for (const auto &name : ClientMap_settings)
		g_settings->registerChangedCallback(name, on_settings_changed, this);
	// load all settings at once
	onSettingChanged("", true);
}

void ClientMap::onSettingChanged(std::string_view name, bool all)
{
	if (all || name == "trilinear_filter")
		m_cache_trilinear_filter  = g_settings->getBool("trilinear_filter");
	if (all || name == "bilinear_filter")
		m_cache_bilinear_filter   = g_settings->getBool("bilinear_filter");
	if (all || name == "anisotropic_filter")
		m_cache_anistropic_filter = g_settings->getBool("anisotropic_filter");
	if (all || name == "transparency_sorting_group_by_buffers")
		m_cache_transparency_sorting_group_by_buffers =
				g_settings->getBool("transparency_sorting_group_by_buffers");
	if (all || name == "transparency_sorting_distance")
		m_cache_transparency_sorting_distance = g_settings->getU16("transparency_sorting_distance");
	if (all || name == "occlusion_culler")
		m_loops_occlusion_culler = g_settings->get("occlusion_culler") == "loops";
	if (all || name == "enable_raytraced_culling")
		m_enable_raytraced_culling = g_settings->getBool("enable_raytraced_culling");
}

ClientMap::~ClientMap()
{
	verbosestream << FUNCTION_NAME << std::endl;

	g_settings->deregisterAllChangedCallbacks(this);

	// avoid refcount warning from ~Map()
	clearDrawList();
	clearDrawListShadow();

	for (auto &it : m_dynamic_buffers)
		it.second.drop();
}

void ClientMap::updateCamera(v3f pos, v3f dir, f32 fov, v3s16 offset, video::SColor light_color)
{
	v3s16 previous_camera_offset = m_camera_offset;
	v3s16 previous_node = floatToInt(m_camera_position, BS) + m_camera_offset;
	v3s16 previous_block = getContainerPos(previous_node, MAP_BLOCKSIZE);

	m_camera_position = pos;
	m_camera_direction = dir;
	m_camera_fov = fov;
	m_camera_offset = offset;
	m_camera_light_color = light_color;

	v3s16 current_node = floatToInt(m_camera_position, BS) + m_camera_offset;
	v3s16 current_block = getContainerPos(current_node, MAP_BLOCKSIZE);

	// reorder the blocks when camera crosses block boundary
	if (previous_block != current_block)
		m_needs_update_drawlist = true;

	// reorder transparent meshes when camera crosses node boundary
	if (previous_node != current_node)
		m_needs_update_transparent_meshes = true;

	// drop merged mesh cache when camera offset changes
	if (previous_camera_offset != m_camera_offset) {
		for (auto &it : m_dynamic_buffers)
			it.second.drop();
		m_dynamic_buffers.clear();
	}
}

MapSector * ClientMap::emergeSector(v2s16 p2d)
{
	// Check that it doesn't exist already
	MapSector *sector = getSectorNoGenerate(p2d);

	// Create it if it does not exist yet
	if (!sector) {
		sector = new MapSector(this, p2d, m_gamedef);
		m_sectors[p2d] = sector;
	}

	return sector;
}

void ClientMap::OnRegisterSceneNode()
{
	if(IsVisible)
	{
		SceneManager->registerNodeForRendering(this, scene::ESNRP_SOLID);
		SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);
	}

	ISceneNode::OnRegisterSceneNode();
	// It's not needed to register this node to the shadow renderer
	// we have other way to find it
}

void ClientMap::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();
	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
	renderMap(driver, SceneManager->getSceneNodeRenderPass());
}

void ClientMap::getBlocksInViewRange(v3s16 cam_pos_nodes,
		v3s16 *p_blocks_min, v3s16 *p_blocks_max, float range)
{
	if (range <= 0.0f)
		range = m_control.wanted_range;

	v3s16 box_nodes_d = range * v3s16(1, 1, 1);
	// Define p_nodes_min/max as v3s32 because 'cam_pos_nodes -/+ box_nodes_d'
	// can exceed the range of v3s16 when a large view range is used near the
	// world edges.
	v3s32 p_nodes_min(
		cam_pos_nodes.X - box_nodes_d.X,
		cam_pos_nodes.Y - box_nodes_d.Y,
		cam_pos_nodes.Z - box_nodes_d.Z);
	v3s32 p_nodes_max(
		cam_pos_nodes.X + box_nodes_d.X,
		cam_pos_nodes.Y + box_nodes_d.Y,
		cam_pos_nodes.Z + box_nodes_d.Z);
	// Take a fair amount as we will be dropping more out later
	// Umm... these additions are a bit strange but they are needed.
	*p_blocks_min = v3s16(
			p_nodes_min.X / MAP_BLOCKSIZE - 3,
			p_nodes_min.Y / MAP_BLOCKSIZE - 3,
			p_nodes_min.Z / MAP_BLOCKSIZE - 3);
	*p_blocks_max = v3s16(
			p_nodes_max.X / MAP_BLOCKSIZE + 1,
			p_nodes_max.Y / MAP_BLOCKSIZE + 1,
			p_nodes_max.Z / MAP_BLOCKSIZE + 1);
}

class MapBlockFlags
{
public:
	static constexpr u16 CHUNK_EDGE = 8;
	static constexpr u16 CHUNK_MASK = CHUNK_EDGE - 1;
	static constexpr std::size_t CHUNK_VOLUME = CHUNK_EDGE * CHUNK_EDGE * CHUNK_EDGE; // volume of a chunk

	MapBlockFlags(v3s16 min_pos, v3s16 max_pos)
			: min_pos(min_pos), volume((max_pos - min_pos) / CHUNK_EDGE + 1)
	{
		chunks.resize(volume.X * volume.Y * volume.Z);
	}

	class Chunk
	{
	public:
		inline u8 &getBits(v3s16 pos)
		{
			std::size_t address = getAddress(pos);
			return bits[address];
		}

	private:
		inline std::size_t getAddress(v3s16 pos) {
			std::size_t address = (pos.X & CHUNK_MASK) + (pos.Y & CHUNK_MASK) * CHUNK_EDGE + (pos.Z & CHUNK_MASK) * (CHUNK_EDGE * CHUNK_EDGE);
			return address;
		}

		std::array<u8, CHUNK_VOLUME> bits;
	};

	Chunk &getChunk(v3s16 pos)
	{
		v3s16 delta = (pos - min_pos) / CHUNK_EDGE;
		std::size_t address = delta.X + delta.Y * volume.X + delta.Z * volume.X * volume.Y;
		Chunk *chunk = chunks[address].get();
		if (!chunk) {
			chunk = new Chunk();
			chunks[address].reset(chunk);
		}
		return *chunk;
	}
private:
	std::vector<std::unique_ptr<Chunk>> chunks;
	v3s16 min_pos;
	v3s16 volume;
};

void ClientMap::clearDrawList()
{
	for (auto &i : m_drawlist) {
		MapBlock *block = i.second;
		block->refDrop();
	}
	m_drawlist.clear();

	for (auto &block : m_keeplist)
		block->refDrop();
	m_keeplist.clear();

	m_needs_update_drawlist = true;
}

void ClientMap::updateDrawList()
{
	ScopeProfiler sp(g_profiler, "CM::updateDrawList()", SPT_AVG);

	clearDrawList();

	m_needs_update_drawlist = false;

	const v3s16 cam_pos_nodes = floatToInt(m_camera_position, BS);

	v3s16 p_blocks_min;
	v3s16 p_blocks_max;
	getBlocksInViewRange(cam_pos_nodes, &p_blocks_min, &p_blocks_max);

	// Number of blocks occlusion culled
	u32 blocks_occlusion_culled = 0;
	// Number of blocks frustum culled
	u32 blocks_frustum_culled = 0;

	const MeshGrid mesh_grid = m_client->getMeshGrid();

	// No occlusion culling when free_move is on and camera is inside ground
	// No occlusion culling for chunk sizes of 4 and above
	//   because the current occlusion culling test is highly inefficient at these sizes
	bool occlusion_culling_enabled = mesh_grid.cell_size < 4;
	if (m_control.allow_noclip) {
		MapNode n = getNode(cam_pos_nodes);
		if (n.getContent() == CONTENT_IGNORE || m_nodedef->get(n).visuals->solidness == 2)
			occlusion_culling_enabled = false;
	}

	const v3s16 camera_block = getContainerPos(cam_pos_nodes, MAP_BLOCKSIZE);
	assert(m_drawlist.empty());
	m_drawlist = decltype(m_drawlist)(MapBlockComparer(camera_block));

	auto is_frustum_culled = m_client->getCamera()->getFrustumCuller();

	// Uncomment to debug occluded blocks in the wireframe mode
	// TODO: Include this as a flag for an extended debugging setting
	// if (occlusion_culling_enabled && m_control.show_wireframe)
	// 	occlusion_culling_enabled = porting::getTimeS() & 1;

	const auto &add_to_drawlist = [this] (MapBlock *block) {
		block->refGrab();
		auto res = m_drawlist.emplace(block->getPos(), block);
		(void)res;
		assert(res.second); // must not already exist
	};

	// Set of mesh holding blocks, will be transferred to m_drawlist
	std::set<v3s16> shortlist;

	/*
	 When range_all is enabled, enumerate all blocks visible in the
	 frustum and display them.
	 */
	if (m_control.range_all || m_loops_occlusion_culler) {
		// Number of blocks currently loaded by the client
		u32 blocks_loaded = 0;
		// Number of blocks in rendering range
		u32 blocks_in_range = 0;
		assert(m_keeplist.empty());

		MapBlockVect sectorblocks;

		for (auto &sector_it : m_sectors) {
			const MapSector *sector = sector_it.second;
			v2s16 sp = sector->getPos();

			blocks_loaded += sector->size();
			if (!m_control.range_all) {
				if (sp.X < p_blocks_min.X || sp.X > p_blocks_max.X ||
						sp.Y < p_blocks_min.Z || sp.Y > p_blocks_max.Z)
					continue;
			}

			// Loop through blocks in sector
			for (const auto &entry : sector->getBlocks()) {
				MapBlock *block = entry.second.get();
				MapBlockMesh *mesh = block->mesh;

				// Calculate the coordinates for range and frustum culling
				v3f mesh_sphere_center;
				f32 mesh_sphere_radius;

				v3s16 block_pos = block->getPos();
				v3s16 block_pos_nodes = block->getPosRelative();

				if (mesh) {
					mesh_sphere_center = intToFloat(block_pos_nodes, BS)
							+ mesh->getBoundingSphereCenter();
					mesh_sphere_radius = mesh->getBoundingRadius();
				} else {
					mesh_sphere_center = intToFloat(block_pos_nodes, BS)
							+ v3f((MAP_BLOCKSIZE * 0.5f - 0.5f) * BS);
					mesh_sphere_radius = 0.0f;
				}

				// First, perform a simple distance check.
				if (!m_control.range_all &&
					mesh_sphere_center.getDistanceFrom(m_camera_position) >
						m_control.wanted_range * BS + mesh_sphere_radius)
					continue; // Out of range, skip.

				// Keep the block alive as long as it is in range.
				block->resetUsageTimer();
				blocks_in_range++;

				// Frustum culling
				// Only do coarse culling here, to account for fast camera movement.
				// This is needed because this function is not called every frame.
				float frustum_cull_extra_radius = 30.0f * BS;
				if (is_frustum_culled(mesh_sphere_center,
						mesh_sphere_radius + frustum_cull_extra_radius)) {
					blocks_frustum_culled++;
					continue;
				}

				// Raytraced occlusion culling - send rays from the camera to the block's corners
				if (!m_control.range_all && occlusion_culling_enabled && m_enable_raytraced_culling &&
						block &&
						isMeshOccluded(block, mesh_grid.cell_size, cam_pos_nodes)) {
					blocks_occlusion_culled++;
					continue;
				}

				if (mesh_grid.cell_size > 1) {
					// Block meshes are stored in the corner block of a chunk
					// (where all coordinate are divisible by the chunk size)
					// Deduplicate and add them later
					shortlist.emplace(mesh_grid.getMeshPos(block_pos));
					// All other blocks we can add to m_keeplist right away
					if (!mesh_grid.isMeshPos(block_pos) || !mesh) {
						m_keeplist.push_back(block);
						block->refGrab();
					}
				} else {
					if (mesh) {
						// add directly to the drawlist
						add_to_drawlist(block);
					} else { // ...or to m_keeplist
						m_keeplist.push_back(block);
						block->refGrab();
					}
				}
			}
		}

		g_profiler->avg("MapBlocks in range [#]", blocks_in_range);
		g_profiler->avg("MapBlocks loaded [#]", blocks_loaded);
	} else {
		// Blocks visited by the algorithm
		u32 blocks_visited = 0;
		// Block sides that were not traversed
		u32 sides_skipped = 0;

		std::queue<v3s16> blocks_to_consider;

		const v3s16 camera_mesh = mesh_grid.getMeshPos(camera_block);
		const v3s16 camera_cell = mesh_grid.getCellPos(camera_block);

		// Bits per block:
		// [ visited | 0 | 0 | 0 | 0 | Z visible | Y visible | X visible ]
		MapBlockFlags meshes_seen(mesh_grid.getCellPos(p_blocks_min), mesh_grid.getCellPos(p_blocks_max) + 1);

		// Start breadth-first search with the block the camera is in
		blocks_to_consider.push(camera_mesh);
		meshes_seen.getChunk(camera_cell).getBits(camera_cell) = 0x07; // mark all sides as visible

		// Recursively walk the space and pick mapblocks for drawing
		while (!blocks_to_consider.empty()) {
			v3s16 block_coord = blocks_to_consider.front();
			blocks_to_consider.pop();
			// We only iterate along the grid
			assert(mesh_grid.isMeshPos(block_coord));

			v3s16 cell_coord = mesh_grid.getCellPos(block_coord);
			auto &flags = meshes_seen.getChunk(cell_coord).getBits(cell_coord);

			// Only visit each cell once (it may have been queued up to three times)
			if ((flags & 0x80) == 0x80)
				continue;
			flags |= 0x80;

			blocks_visited++;

			MapBlock *block = getBlockNoCreateNoEx(block_coord);
			MapBlockMesh *mesh = block ? block->mesh : nullptr;

			// Calculate the coordinates for range and frustum culling
			v3f mesh_sphere_center;
			f32 mesh_sphere_radius;

			v3s16 block_pos_nodes = block_coord * MAP_BLOCKSIZE;

			if (mesh) {
				mesh_sphere_center = intToFloat(block_pos_nodes, BS)
						+ mesh->getBoundingSphereCenter();
				mesh_sphere_radius = mesh->getBoundingRadius();
			} else {
				mesh_sphere_center = intToFloat(block_pos_nodes, BS) +
					v3f((mesh_grid.cell_size * MAP_BLOCKSIZE * 0.5f - 0.5f) * BS);
				mesh_sphere_radius = 0.87f * mesh_grid.cell_size * MAP_BLOCKSIZE * BS;
			}

			// First, perform a simple distance check.
			if (mesh_sphere_center.getDistanceFrom(intToFloat(cam_pos_nodes, BS)) >
					m_control.wanted_range * BS + mesh_sphere_radius)
				continue; // Out of range, skip.

			// Frustum culling
			// Only do coarse culling here, to account for fast camera movement.
			// This is needed because this function is not called every frame.
			float frustum_cull_extra_radius = 30.0f * BS;
			if (is_frustum_culled(mesh_sphere_center,
					mesh_sphere_radius + frustum_cull_extra_radius)) {
				blocks_frustum_culled++;
				continue;
			}

			// Calculate the vector from the camera block to the current block
			// We use it to determine through which sides of the current block we can continue the search
			v3s16 look = block_coord - camera_mesh;

			// Occluded near sides will further occlude the far sides
			u8 visible_outer_sides = flags & 0x07;

			// Raytraced occlusion culling - send rays from the camera to the block's corners
			if (occlusion_culling_enabled && m_enable_raytraced_culling &&
					block && visible_outer_sides != 0x07 &&
					isMeshOccluded(block, mesh_grid.cell_size, cam_pos_nodes)) {
				blocks_occlusion_culled++;
				continue;
			}

			// Note that we don't fill m_keeplist, or call resetUsageTimer() here.
			// touchMapBlocks() exists to deal with that.
			if (mesh) {
				add_to_drawlist(block);
			}

			// Decide which sides to traverse next or to block away

			// First, find the near sides that would occlude the far sides
			// * A near side can itself be occluded by a nearby block (the test above ^^)
			// * A near side can be visible but fully opaque by itself (e.g. ground at the 0 level)

			// mesh solid sides are +Z-Z+Y-Y+X-X
			// if we are inside the block's coordinates on an axis,
			// treat these sides as opaque, as they should not allow to reach the far sides
			u8 block_inner_sides = (look.X == 0 ? 3 : 0) |
				(look.Y == 0 ? 12 : 0) |
				(look.Z == 0 ? 48 : 0);

			// get the mask for the sides that are relevant based on the direction
			u8 near_inner_sides = (look.X > 0 ? 1 : 2) |
					(look.Y > 0 ? 4 : 8) |
					(look.Z > 0 ? 16 : 32);

			// This bitset is +Z-Z+Y-Y+X-X (See MapBlockMesh), and axis is XYZ.
			// Get he block's transparent sides
			u8 transparent_sides = (occlusion_culling_enabled && block) ? ~block->solid_sides : 0x3F;

			// compress block transparent sides to ZYX mask of see-through axes
			u8 near_transparency =  (block_inner_sides == 0x3F) ? near_inner_sides : (transparent_sides & near_inner_sides);

			// when we are inside the camera block, do not block any sides
			if (block_inner_sides == 0x3F)
				block_inner_sides = 0;

			near_transparency &= ~block_inner_sides & 0x3F;

			near_transparency |= (near_transparency >> 1);
			near_transparency = (near_transparency & 1) |
					((near_transparency >> 1) & 2) |
					((near_transparency >> 2) & 4);

			// combine with known visible sides that matter
			near_transparency &= visible_outer_sides;

			// The rule for any far side to be visible:
			// * Any of the adjacent near sides is transparent (different axes)
			// * The opposite near side (same axis) is transparent, if it is the dominant axis of the look vector

			// Calculate vector from camera to mapblock center. Because we only need relation between
			// coordinates we scale by 2 to avoid precision loss.
			v3s16 precise_look = 2 * (block_pos_nodes - cam_pos_nodes) + mesh_grid.cell_size * MAP_BLOCKSIZE - 1;

			// dominant axis flag
			u8 dominant_axis = (abs(precise_look.X) > abs(precise_look.Y) && abs(precise_look.X) > abs(precise_look.Z)) |
						((abs(precise_look.Y) > abs(precise_look.Z) && abs(precise_look.Y) > abs(precise_look.X)) << 1) |
						((abs(precise_look.Z) > abs(precise_look.X) && abs(precise_look.Z) > abs(precise_look.Y)) << 2);

			// Queue next blocks for processing:
			// - Examine "far" sides of the current blocks, i.e. never move towards the camera
			// - Only traverse the sides that are not occluded
			// - Only traverse the sides that are not opaque
			// When queueing, mark the relevant side on the next block as 'visible'
			for (s16 axis = 0; axis < 3; axis++) {

				// Select a bit from transparent_sides for the side
				u8 far_side_mask = 1 << (2 * axis);

				// axis flag
				u8 my_side = 1 << axis;
				u8 adjacent_sides = my_side ^ 0x07;

				auto traverse_far_side = [&](s8 next_pos_offset) {
					// far side is visible if adjacent near sides are transparent, or if opposite side on dominant axis is transparent
					bool side_visible = ((near_transparency & adjacent_sides) | (near_transparency & my_side & dominant_axis)) != 0;
					side_visible = side_visible && ((far_side_mask & transparent_sides) != 0);

					v3s16 next_pos = block_coord;
					next_pos[axis] += next_pos_offset;

					v3s16 next_cell = mesh_grid.getCellPos(next_pos);

					// If a side is a see-through, mark the next block's side as visible, and queue
					if (side_visible) {
						auto &next_flags = meshes_seen.getChunk(next_cell).getBits(next_cell);
						next_flags |= my_side;
						blocks_to_consider.push(next_pos);
					} else {
						sides_skipped++;
					}
				};


				// Test the '-' direction of the axis
				if (look[axis] <= 0 && block_coord[axis] > p_blocks_min[axis])
					traverse_far_side(-mesh_grid.cell_size);

				// Test the '+' direction of the axis
				far_side_mask <<= 1;

				if (look[axis] >= 0 && block_coord[axis] < p_blocks_max[axis])
					traverse_far_side(+mesh_grid.cell_size);
			}
		}
		g_profiler->avg("MapBlock sides skipped [#]", sides_skipped);
		g_profiler->avg("MapBlocks examined [#]", blocks_visited);
	}

	// must populate either only to avoid duplicates
	assert(m_drawlist.empty() || shortlist.empty());
	for (auto pos : shortlist) {
		MapBlock *block = getBlockNoCreateNoEx(pos);
		if (block && block->mesh)
			add_to_drawlist(block);
	}

	g_profiler->avg("MapBlocks occlusion culled [#]", blocks_occlusion_culled);
	g_profiler->avg("MapBlocks frustum culled [#]", blocks_frustum_culled);
	g_profiler->avg("MapBlocks drawn [#]", m_drawlist.size());
}

void ClientMap::touchMapBlocks()
{
	// This function is only needed when using the BFS culler, since it does not
	// look at all blocks in range.
	// compare to ClientMap::updateDrawList()
	if (m_control.range_all || m_loops_occlusion_culler)
		return;

	ScopeProfiler sp(g_profiler, "CM::touchMapBlocks()", SPT_AVG);

	const v3s16 cam_pos_nodes = floatToInt(m_camera_position, BS);

	v3s16 p_blocks_min;
	v3s16 p_blocks_max;
	getBlocksInViewRange(cam_pos_nodes, &p_blocks_min, &p_blocks_max);

	// Number of blocks currently loaded by the client
	u32 blocks_loaded = 0;
	// Number of blocks in rendering range
	u32 blocks_in_range = 0;

	for (const auto &sector_it : m_sectors) {
		const MapSector *sector = sector_it.second;
		v2s16 sp = sector->getPos();

		blocks_loaded += sector->size();
		if (!m_control.range_all) {
			if (sp.X < p_blocks_min.X || sp.X > p_blocks_max.X ||
					sp.Y < p_blocks_min.Z || sp.Y > p_blocks_max.Z)
				continue;
		}

		/*
			Loop through blocks in sector
		*/

		for (const auto &entry : sector->getBlocks()) {
			MapBlock *block = entry.second.get();
			MapBlockMesh *mesh = block->mesh;

			// Calculate the coordinates for range and frustum culling
			v3f mesh_sphere_center;
			f32 mesh_sphere_radius;

			v3s16 block_pos_nodes = block->getPosRelative();

			if (mesh) {
				mesh_sphere_center = intToFloat(block_pos_nodes, BS)
						+ mesh->getBoundingSphereCenter();
				mesh_sphere_radius = mesh->getBoundingRadius();
			} else {
				mesh_sphere_center = intToFloat(block_pos_nodes, BS)
						+ v3f((MAP_BLOCKSIZE * 0.5f - 0.5f) * BS);
				mesh_sphere_radius = 0.0f;
			}

			// First, perform a simple distance check.
			if (mesh_sphere_center.getDistanceFrom(m_camera_position) >
					m_control.wanted_range * BS + mesh_sphere_radius)
				continue; // Out of range, skip.

			// Keep the block alive as long as it is in range.
			block->resetUsageTimer();
			blocks_in_range++;
		}
	}

	g_profiler->avg("MapBlocks in range [#]", blocks_in_range);
	g_profiler->avg("MapBlocks loaded [#]", blocks_loaded);
}

void MeshBufListMaps::addFromBlock(v3s16 block_pos, MapBlockMesh *block_mesh,
	video::IVideoDriver *driver)
{
	for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
		scene::IMesh *mesh = block_mesh->getMesh(layer);
		assert(mesh);

		u32 c = mesh->getMeshBufferCount();
		for (u32 i = 0; i < c; i++) {
			scene::IMeshBuffer *buf = mesh->getMeshBuffer(i);

			auto &material = buf->getMaterial();
			auto *rnd = driver->getMaterialRenderer(material.MaterialType);
			bool transparent = rnd && rnd->isTransparent();
			if (!transparent)
				add(buf, block_pos, layer);
		}
	}
}

namespace {
	// there is no convenient scope this would fit, so it's global
	struct {
		u32 total = 0, cache_miss = 0;

		inline void increment(bool hit)
		{
			total++;
			cache_miss += hit ? 0 : 1;
		}
		inline void commit(Profiler *profiler)
		{
			if (total == 0)
				return;
			float rate = (total - cache_miss) / (float)total;
			profiler->avg("CM::transformBuffers...: cache hit rate [%]", 100 * rate);
			*this = {0, 0};
		}
	} buffer_transform_stats;
}

/**
 * Copy a list of mesh buffers into the draw order, while potentially
 * merging some.
 * @param src buffer list
 * @param dst draw order
 * @param get_world_pos returns translation for a buffer
 * @param dynamic_buffers cache structure for merged buffers
 * @return number of buffers that were merged
 */
template <typename F>
static u32 transformBuffersToDrawOrder(
	const MeshBufListMaps::MeshBufList &src, DrawDescriptorList &draw_order,
		F get_world_pos, CachedMeshBuffers &dynamic_buffers)
{
	/**
	 * This is a tradeoff between time spent merging buffers and time spent
	 * due to excess drawcalls.
	 * Testing has shown that the ideal value is in the low hundreds, as extra
	 * CPU work quickly eats up the benefits (though alleviated by a cache).
	 * In MTG landscape scenes this was found to save around 20-40% of drawcalls.
	 *
	 * NOTE: if you attempt to test this with quicktune, it won't give you valid
	 * results since HW buffers stick around and Irrlicht handles large amounts
	 * inefficiently.
	 */
	const u32 target_min_vertices = g_settings->getU32("mesh_buffer_min_vertices");

	const auto draw_order_pre = draw_order.size();
	auto *driver = RenderingEngine::get_video_driver();

	// check if we can even merge anything
	u32 can_merge = 0;
	u32 total_vtx = 0, total_idx = 0;
	for (auto &pair : src) {
		if (pair.second->getVertexCount() < target_min_vertices) {
			can_merge++;
			total_vtx += pair.second->getVertexCount();
			total_idx += pair.second->getIndexCount();
		}
	}

	// iterate in reverse to get closest blocks first
	std::vector<std::pair<v3f, scene::IMeshBuffer*>> to_merge;
	for (auto it = src.rbegin(); it != src.rend(); ++it) {
		v3f translate = get_world_pos(it->first);
		auto *buf = it->second;
		if (can_merge < 2 || buf->getVertexCount() >= target_min_vertices) {
			draw_order.emplace_back(translate, buf);
			continue;
		}
		to_merge.emplace_back(translate, buf);
	}

	/*
	 * Tracking buffers, their contents and modifications would be quite complicated
	 * so we opt for something simple here: We identify buffers by their location
	 * in memory.
	 * This imposes the following assumptions:
	 * - buffers don't move in memory
	 * - vertex and index data is immutable
	 * - we know when to invalidate (invalidateMapBlockMesh does this)
	 */
	std::sort(to_merge.begin(), to_merge.end(), [] (const auto &l, const auto &r) {
		return static_cast<void*>(l.second) < static_cast<void*>(r.second);
	});
	// cache key is a string of sorted raw pointers
	std::string key;
	key.reserve(sizeof(void*) * to_merge.size());
	for (auto &it : to_merge)
		key.append(reinterpret_cast<const char*>(&it.second), sizeof(void*));

	// try to take from cache
	auto it2 = dynamic_buffers.find(key);
	if (it2 != dynamic_buffers.end()) {
		buffer_transform_stats.increment(true);
		const auto &use_mat = to_merge.front().second->getMaterial();
		assert(!it2->second.buf.empty());
		for (auto *buf : it2->second.buf) {
			// material is not part of the cache key, so make sure it still matches
			buf->getMaterial() = use_mat;
			draw_order.emplace_back(v3f(0), buf);
		}
		it2->second.age = 0;
	} else if (!key.empty()) {
		buffer_transform_stats.increment(false);
		// merge and save to cache
		auto &put_buffers = dynamic_buffers[key];
		scene::SMeshBuffer *tmp = nullptr;
		const auto &finish_buf = [&] () {
			if (tmp) {
				draw_order.emplace_back(v3f(0), tmp);
				total_vtx = subtract_or_zero(total_vtx, tmp->getVertexCount());
				total_idx = subtract_or_zero(total_idx, tmp->getIndexCount());

				// Upload buffer here explicitly to give the driver some
				// extra time to get it ready before drawing.
				tmp->setHardwareMappingHint(scene::EHM_STREAM);
				driver->updateHardwareBuffer(tmp->getVertexBuffer());
				driver->updateHardwareBuffer(tmp->getIndexBuffer());
			}
			tmp = nullptr;
		};

		for (auto &it : to_merge) {
			v3f translate = it.first;
			auto *buf = it.second;

			bool new_buffer = false;
			if (!tmp)
				new_buffer = true;
			else if (tmp->getVertexCount() + buf->getVertexCount() > U16_MAX)
				new_buffer = true;
			if (new_buffer) {
				finish_buf();
				tmp = new scene::SMeshBuffer();
				put_buffers.buf.push_back(tmp);
				assert(tmp->getPrimitiveType() == buf->getPrimitiveType());
				tmp->Material = buf->getMaterial();
				// preallocate approximately
				tmp->Vertices->Data.reserve(MYMIN(U16_MAX, total_vtx));
				tmp->Indices->Data.reserve(total_idx);
			}
			appendToMeshBuffer(tmp, buf, translate);
		}
		finish_buf();
		assert(!put_buffers.buf.empty());
	}

	// first call needs to set the material
	if (draw_order.size() > draw_order_pre)
		draw_order[draw_order_pre].m_reuse_material = false;

	return can_merge < 2 ? 0 : can_merge;
}

void ClientMap::renderMap(video::IVideoDriver* driver, s32 pass)
{
	ZoneScoped;

	const bool is_transparent_pass = pass == scene::ESNRP_TRANSPARENT;

	std::string prefix;
	if (pass == scene::ESNRP_SOLID)
		prefix = "renderMap(SOLID): ";
	else
		prefix = "renderMap(TRANS): ";

	/*
		Get animation parameters
	*/
	const float animation_time = m_client->getAnimationTime();
	const int crack = m_client->getCrackLevel();
	const u32 daynight_ratio = m_client->getEnv().getDayNightRatio();

	const v3f camera_position = m_camera_position;

	const auto mesh_grid = m_client->getMeshGrid();
	// Gets world position from block map position
	const auto get_block_wpos = [&] (v3s16 pos) -> v3f {
		return intToFloat(mesh_grid.getMeshPos(pos) * MAP_BLOCKSIZE - m_camera_offset, BS);
	};

	u32 merged_count = 0;

	// For limiting number of mesh animations per frame
	u32 mesh_animate_count = 0;

	/*
		Update transparent meshes
	*/
	if (is_transparent_pass)
		updateTransparentMeshBuffers();

	/*
		Collect everything we need to draw
	*/
	TimeTaker tt_collect("");

	MeshBufListMaps &grouped_buffers = tl_meshbuflistmaps;
	DrawDescriptorList &draw_order = tl_drawdescriptorlist;
	grouped_buffers.clear();
	draw_order.clear();

	auto is_frustum_culled = m_client->getCamera()->getFrustumCuller();

	for (auto &i : m_drawlist) {
		const v3s16 block_pos = i.first;
		MapBlock *block = i.second;
		MapBlockMesh *block_mesh = block->mesh;

		// If the mesh of the block happened to get deleted, ignore it
		if (!block_mesh)
			continue;

		// Do exact frustum culling
		// (The one in updateDrawList is only coarse.)
		v3f mesh_sphere_center = intToFloat(block->getPosRelative(), BS)
				+ block_mesh->getBoundingSphereCenter();
		f32 mesh_sphere_radius = block_mesh->getBoundingRadius();
		if (is_frustum_culled(mesh_sphere_center, mesh_sphere_radius))
			continue;

		// Mesh animation
		if (pass == scene::ESNRP_SOLID) {
			// 50 nodes is pretty arbitrary but it should work somewhat nicely
			float distance_sq = camera_position.getDistanceFromSQ(mesh_sphere_center);
			bool faraway = distance_sq >= std::pow(BS * 50 + mesh_sphere_radius, 2.0f);

			if (block_mesh->isAnimationForced() || !faraway ||
					mesh_animate_count < (m_control.range_all ? 200 : 50)) {

				bool animated = block_mesh->animate(faraway, animation_time,
					crack, daynight_ratio);
				if (animated)
					mesh_animate_count++;
			} else {
				block_mesh->decreaseAnimationForceTimer();
			}
		}

		/*
			Get the meshbuffers of the block
		*/
		if (is_transparent_pass) {
			// In transparent pass, the mesh will give us
			// the partial buffers in the correct order
			for (auto &buffer : block_mesh->getTransparentBuffers())
				draw_order.emplace_back(get_block_wpos(block_pos), &buffer);
		} else {
			// Otherwise, group them
			grouped_buffers.addFromBlock(block_pos, block_mesh, driver);
		}
	}

	assert(!is_transparent_pass || grouped_buffers.empty());
	for (auto &map : grouped_buffers.maps) {
		for (auto &list : map) {
			merged_count += transformBuffersToDrawOrder(
				list.second, draw_order, get_block_wpos, m_dynamic_buffers);
		}
	}

	g_profiler->avg(prefix + "collecting [ms]", tt_collect.stop(true));

	TimeTaker tt_draw("");

	core::matrix4 m; // Model matrix
	u32 vertex_count = 0;
	u32 drawcall_count = 0;
	u32 material_swaps = 0;
	u32 array_texture_use = 0;

	// Render all mesh buffers in order
	drawcall_count += draw_order.size();

	for (auto &descriptor : draw_order) {
		if (!descriptor.m_reuse_material) {
			auto &material = descriptor.getMaterial();

			// Apply filter settings
			material.forEachTexture([this] (auto &tex) {
				setMaterialFilters(tex, m_cache_bilinear_filter, m_cache_trilinear_filter,
						m_cache_anistropic_filter);
			});
			material.Wireframe = m_control.show_wireframe;

			// pass the shadow map texture to the buffer texture
			ShadowRenderer *shadow = m_rendering_engine->get_shadow_renderer();
			if (shadow && shadow->is_active()) {
				auto &layer = material.TextureLayers[ShadowRenderer::TEXTURE_LAYER_SHADOW];
				layer.Texture = shadow->get_texture();
				layer.TextureWrapU = video::E_TEXTURE_CLAMP::ETC_CLAMP_TO_EDGE;
				layer.TextureWrapV = video::E_TEXTURE_CLAMP::ETC_CLAMP_TO_EDGE;
				// Do not enable filter on shadow texture to avoid visual artifacts
				// with colored shadows.
				// Filtering is done in shader code anyway
				layer.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
				layer.MagFilter = video::ETMAGF_NEAREST;
				layer.AnisotropicFilter = 0;
			}

			driver->setMaterial(material);
			++material_swaps;
			if (auto *tex = material.getTexture(0); tex && tex->getType() == video::ETT_2D_ARRAY)
				++array_texture_use;

			material.TextureLayers[ShadowRenderer::TEXTURE_LAYER_SHADOW].Texture = nullptr;
		}

		m.setTranslation(descriptor.m_pos);
		driver->setTransform(video::ETS_WORLD, m);

		vertex_count += descriptor.draw(driver);
	}

	g_profiler->avg(prefix + "draw meshes [ms]", tt_draw.stop(true));

	if (pass == scene::ESNRP_SOLID) {
		g_profiler->avg("renderMap(): animated meshes [#]", mesh_animate_count);
		g_profiler->avg(prefix + "merged buffers [#]", merged_count);

		u32 cached_count = 0;
		for (auto it = m_dynamic_buffers.begin(); it != m_dynamic_buffers.end(); ) {
			// prune aggressively since every new/changed block or camera
			// rotation can have big effects
			if (++it->second.age > 1) {
				it->second.drop();
				it = m_dynamic_buffers.erase(it);
			} else {
				cached_count += it->second.buf.size();
				it++;
			}
		}
		g_profiler->avg(prefix + "merged buffers in cache [#]", cached_count);

		buffer_transform_stats.commit(g_profiler);
	}

	if (pass == scene::ESNRP_TRANSPARENT) {
		g_profiler->avg("renderMap(): transparent buffers [#]", draw_order.size());
	}

	g_profiler->avg(prefix + "vertices drawn [#]", vertex_count);
	g_profiler->avg(prefix + "drawcalls [#]", drawcall_count);
	g_profiler->avg(prefix + "material swaps [#]", material_swaps);
	if (material_swaps && array_texture_use) {
		int percent = (100.0f * array_texture_use) / material_swaps;
		g_profiler->avg(prefix + "array texture use [%]", percent);
	}
}

void ClientMap::invalidateMapBlockMesh(MapBlockMesh *mesh)
{
	// find all buffers for this block
	MeshBufListMaps tmp;
	tmp.addFromBlock(v3s16(), mesh, getSceneManager()->getVideoDriver());

	std::vector<void*> to_delete;
	void *maxp = 0;
	for (auto &it : tmp.maps) {
		for (auto &it2 : it) {
			for (auto &it3 : it2.second) {
				void *const p = it3.second; // explicit downcast
				to_delete.push_back(p);
				maxp = std::max(maxp, p);
			}
		}
	}
	if (to_delete.empty())
		return;

	// we know which buffers were used to produce a merged buffer
	// so go through the cache and drop any entries that match
	const auto &match_any = [&] (const std::string &key) {
		assert(key.size() % sizeof(void*) == 0);
		void *v;
		for (size_t off = 0; off < key.size(); off += sizeof(void*)) {
			// no alignment guarantee so *(void**)&key[off] is not allowed!
			memcpy(&v, &key[off], sizeof(void*));
			if (v > maxp) // early exit, since it's sorted
				break;
			if (CONTAINS(to_delete, v))
				return true;
		}
		return false;
	};
	for (auto it = m_dynamic_buffers.begin(); it != m_dynamic_buffers.end(); ) {
		if (match_any(it->first)) {
			it->second.drop();
			it = m_dynamic_buffers.erase(it);
		} else {
			it++;
		}
	}
}

static bool getVisibleBrightness(Map *map, const v3f &p0, v3f dir, float step,
	float step_multiplier, float start_distance, float end_distance,
	const NodeDefManager *ndef, u32 daylight_factor, float sunlight_min_d,
	int *result, bool *sunlight_seen)
{
	int brightness_sum = 0;
	int brightness_count = 0;
	float distance = start_distance;
	dir.normalize();
	v3f pf = p0;
	pf += dir * distance;
	int noncount = 0;
	bool nonlight_seen = false;
	bool allow_allowing_non_sunlight_propagates = false;
	bool allow_non_sunlight_propagates = false;
	// Check content nearly at camera position
	{
		v3s16 p = floatToInt(p0 /*+ dir * 3*BS*/, BS);
		MapNode n = map->getNode(p);
		if(ndef->getLightingFlags(n).has_light &&
				!ndef->getLightingFlags(n).sunlight_propagates)
			allow_allowing_non_sunlight_propagates = true;
	}
	// If would start at CONTENT_IGNORE, start closer
	{
		v3s16 p = floatToInt(pf, BS);
		MapNode n = map->getNode(p);
		if(n.getContent() == CONTENT_IGNORE){
			float newd = 2*BS;
			pf = p0 + dir * 2*newd;
			distance = newd;
			sunlight_min_d = 0;
		}
	}
	for (int i=0; distance < end_distance; i++) {
		pf += dir * step;
		distance += step;
		step *= step_multiplier;

		v3s16 p = floatToInt(pf, BS);
		MapNode n = map->getNode(p);
		ContentLightingFlags f = ndef->getLightingFlags(n);
		if (allow_allowing_non_sunlight_propagates && i == 0 &&
				f.has_light && !f.sunlight_propagates) {
			allow_non_sunlight_propagates = true;
		}

		if (!f.has_light || (!f.sunlight_propagates && !allow_non_sunlight_propagates)){
			nonlight_seen = true;
			noncount++;
			if(noncount >= 4)
				break;
			continue;
		}

		if (distance >= sunlight_min_d && !*sunlight_seen && !nonlight_seen)
			if (n.getLight(LIGHTBANK_DAY, f) == LIGHT_SUN)
				*sunlight_seen = true;
		noncount = 0;
		brightness_sum += decode_light(n.getLightBlend(daylight_factor, f));
		brightness_count++;
	}
	*result = 0;
	if(brightness_count == 0)
		return false;
	*result = brightness_sum / brightness_count;
	/*std::cerr<<"Sampled "<<brightness_count<<" points; result="
			<<(*result)<<std::endl;*/
	return true;
}

int ClientMap::getBackgroundBrightness(float max_d, u32 daylight_factor,
		int oldvalue, bool *sunlight_seen_result)
{
	ScopeProfiler sp(g_profiler, "CM::getBackgroundBrightness", SPT_AVG);
	static v3f z_directions[50] = {
		v3f(-100, 0, 0)
	};
	static f32 z_offsets[50] = {
		-1000,
	};

	if (z_directions[0].X < -99) {
		for (u32 i = 0; i < ARRLEN(z_directions); i++) {
			// Assumes FOV of 72 and 16/9 aspect ratio
			z_directions[i] = v3f(
				0.02 * myrand_range(-100, 100),
				1.0,
				0.01 * myrand_range(-100, 100)
			).normalize();
			z_offsets[i] = 0.01 * myrand_range(0,100);
		}
	}

	int sunlight_seen_count = 0;
	float sunlight_min_d = max_d*0.8;
	if(sunlight_min_d > 35*BS)
		sunlight_min_d = 35*BS;
	std::vector<int> values;
	values.reserve(ARRLEN(z_directions));
	for (u32 i = 0; i < ARRLEN(z_directions); i++) {
		v3f z_dir = z_directions[i];
		core::CMatrix4<f32> a;
		a.buildRotateFromTo(v3f(0,1,0), z_dir);
		v3f dir = a.rotateAndScaleVect(m_camera_direction);
		int br = 0;
		float step = BS*1.5;
		if(max_d > 35*BS)
			step = max_d / 35 * 1.5;
		float off = step * z_offsets[i];
		bool sunlight_seen_now = false;
		bool ok = getVisibleBrightness(this, m_camera_position, dir,
				step, 1.0, max_d*0.6+off, max_d, m_nodedef, daylight_factor,
				sunlight_min_d,
				&br, &sunlight_seen_now);
		if(sunlight_seen_now)
			sunlight_seen_count++;
		if(!ok)
			continue;
		values.push_back(br);
		// Don't try too much if being in the sun is clear
		if(sunlight_seen_count >= 20)
			break;
	}
	int brightness_sum = 0;
	int brightness_count = 0;
	std::sort(values.begin(), values.end());
	u32 num_values_to_use = values.size();
	if(num_values_to_use >= 10)
		num_values_to_use -= num_values_to_use/2;
	else if(num_values_to_use >= 7)
		num_values_to_use -= num_values_to_use/3;
	u32 first_value_i = (values.size() - num_values_to_use) / 2;

	for (u32 i=first_value_i; i < first_value_i + num_values_to_use; i++) {
		brightness_sum += values[i];
		brightness_count++;
	}

	int ret = 0;
	if(brightness_count == 0){
		MapNode n = getNode(floatToInt(m_camera_position, BS));
		ContentLightingFlags f = m_nodedef->getLightingFlags(n);
		if(f.has_light){
			ret = decode_light(n.getLightBlend(daylight_factor, f));
		} else {
			ret = oldvalue;
		}
	} else {
		ret = brightness_sum / brightness_count;
	}

	*sunlight_seen_result = (sunlight_seen_count > 0);
	return ret;
}

std::vector<std::pair<ConvexPolygon, video::SColor>>
ClientMap::getPostFxPolygons()
{
	using mat4 = core::matrix4;
	using v4f = std::array<f32, 4>;

	auto vector_cross_4d = [](const v4f &v1, const v4f &v2, const v4f &v3) -> v4f
	{
		using varr3 = std::array<f32, 3>;
		using mat33 = std::array<varr3, 3>;

		auto det_3d = [](const mat33 &m) {
			return m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
					- m[0][1] * (m[1][0] * m[2][2] - m[2][0] * m[1][2])
					+ m[0][2] * (m[1][0] * m[2][1] - m[2][0] * m[1][1]);
		};

		auto index33 = [&](int i1, int i2, int i3) -> mat33 {
			return {
				varr3{v1[i1], v1[i2], v1[i3]},
				varr3{v2[i1], v2[i2], v2[i3]},
				varr3{v3[i1], v3[i2], v3[i3]}
			};
		};

		return v4f{
			+det_3d(index33(   1, 2, 3)),
			-det_3d(index33(0,    2, 3)),
			+det_3d(index33(0, 1,    3)),
			-det_3d(index33(0, 1, 2   )),
		};
	};

	auto mat4_mult_v4f = [](const mat4 &m, const v4f &v) {
		v4f ret;
		m.transformVec4(ret.data(), v.data());
		return ret;
	};

	auto get_post_color = [&](const ContentFeatures &features) {
		video::SColor post_color = features.post_effect_color;

		if (features.post_effect_color_shaded) {
			auto apply_light = [](u32 color, u32 light) {
				return core::clamp(core::round32(color * light / 255.0f), 0, 255);
			};
			post_color.setRed(apply_light(post_color.getRed(), m_camera_light_color.getRed()));
			post_color.setGreen(apply_light(post_color.getGreen(), m_camera_light_color.getGreen()));
			post_color.setBlue(apply_light(post_color.getBlue(), m_camera_light_color.getBlue()));
		}

		// Solid nodes are full of darkness.
		if (features.visuals->solidness == 2 && !m_control.allow_noclip)
			post_color = video::SColor(255, 0, 0, 0);

		return post_color;
	};

	const std::optional<LiquidWaveParams> liquid_wave_params =
			g_settings->getBool("enable_waving_water") ?
			LiquidWaveParams{
				g_settings->getFloat("water_wave_height"),
				g_settings->getFloat("water_wave_length"),
				g_settings->getFloat("water_wave_speed"),
				(float)(m_client->getEnv().getFrameTime() % 1000000) / 100000.f,
			}
			: std::optional<LiquidWaveParams>(std::nullopt);

	const bool smooth_lighting = g_settings->getBool("smooth_lighting");

	// transposed inversed view-(quasi)projection matrix
	// This matrix moves the near plane from camera-offset-local world-space to
	// clip-space.
	// (It's the transposed inversed because we want to transform planes.)
	// (Only the near plane is transformed correctly (we don't care about the
	// rest of the space), therefore "(quasi)projection".)
	// (We also don't care for positive scalar multiples, so the adjugate would
	// work too.)
	const mat4 vp_mat_ti = [&]() {
		auto camera_node = m_client->getCamera()->getCameraNode();

		const mat4 view_mat = camera_node->getViewMatrix();

		// The projection matrix moves the near-left-up corner of the frustum
		// to (-1,1,-1), and near-right-down to (1,-1,-1). But we need (0,0,0)
		// and (1,1,0). coord_transf moves the near plane accordingly.
		// (We only care about the near plane, so this scale + translate is ok.)
		const mat4 coord_transf = mat4(
				0.5f,  0.0f, 0.0f, 0.5f,
				0.0f, -0.5f, 0.0f, 0.5f,
				0.0f,  0.0f, 1.0f, 1.0f,
				0.0f,  0.0f, 0.0f, 1.0f
			).getTransposed();
		const mat4 proj_mat = coord_transf * camera_node->getProjectionMatrix();

		return mat4(proj_mat * view_mat, mat4::EM4CONST_INVERSE_TRANSPOSED);
	}();

	std::vector<std::pair<ConvexPolygon, video::SColor>> polygons;

	// Go through all 8 nodes that the near plane possibly instersects with

	// the one in the (-1,-1,-1) corner
	const v3s16 minus_corner(
			std::floor(m_camera_position.X/BS),
			std::floor(m_camera_position.Y/BS),
			std::floor(m_camera_position.Z/BS)
		);

	for (const v3s16 &corner_dir : {
				v3s16(0, 0, 0),
				v3s16(1, 0, 0),
				v3s16(0, 1, 0),
				v3s16(1, 1, 0),
				v3s16(0, 0, 1),
				v3s16(1, 0, 1),
				v3s16(0, 1, 1),
				v3s16(1, 1, 1),
			}) {
		const v3s16 npos = minus_corner + corner_dir;

		const MapNode node = getNode(npos);

		const ContentFeatures &features = m_nodedef->get(node);
		const video::SColor post_color = get_post_color(features);

		if (post_color.getAlpha() == 0)
			continue;

		const ConvexPolygon screen_polygon{{
				{0.0f, 0.0f}, // upper-left
				{1.0f, 0.0f}, // upper-right
				{1.0f, 1.0f}, // lower-right
				{0.0f, 1.0f}, // lower-left
			}};

		// the (inverse and inverse transposed) world matrix for the node
		// Transforms planes from node-local space to camera-offset-local world
		// BS space.
		mat4 world_mat_i = mat4::EM4CONST_IDENTITY;
		world_mat_i.setTranslation(intToFloat(-npos + m_camera_offset, 1.0f));
		world_mat_i.setScale(1.0f/BS);
		const mat4 world_mat_ti(world_mat_i, mat4::EM4CONST_TRANSPOSED);

		const mat4 wvp_mat_ti = vp_mat_ti * world_mat_ti;

		auto clip_polygon_by_objsp_plane = [&](ConvexPolygon &polyg,
				const v4f &clip_plane) -> bool {
			v4f clip_plane_clipspace = mat4_mult_v4f(wvp_mat_ti, clip_plane);

			// The clip plane (and all points p in the clip plane) are at z=0.
			// So the c component in the following inequation drops away:
			// a*p.X + b*p.Y + c*p.Z + d >= 0
			v3f clip_line(clip_plane_clipspace[0], clip_plane_clipspace[1], clip_plane_clipspace[3]);

			polyg = polyg.clip(clip_line);
			return polyg.vertices.size() >= 3;
		};

		auto clip_polygon_by_objsp_planes = [&](ConvexPolygon &polyg,
				const std::vector<v4f> &clip_planes) -> bool {
			for (const v4f &clip_plane : clip_planes)
				if (!clip_polygon_by_objsp_plane(polyg, clip_plane))
					return false;
			return true;
		};

		if (features.drawtype == NDT_NORMAL) {
			// draw full cube
			ConvexPolygon polygon = screen_polygon;
			if (!clip_polygon_by_objsp_planes(polygon, {
						{ 2.0f, 0.0f, 0.0f, 1.0f},
						{-2.0f, 0.0f, 0.0f, 1.0f},
						{0.0f,  2.0f, 0.0f, 1.0f},
						{0.0f, -2.0f, 0.0f, 1.0f},
						{0.0f, 0.0f,  2.0f, 1.0f},
						{0.0f, 0.0f, -2.0f, 1.0f},
					}))
				continue;
			polygons.emplace_back(std::move(polygon), post_color);

		} else if (features.drawtype == NDT_LIQUID
				|| features.drawtype == NDT_FLOWINGLIQUID) {
			ConvexPolygon polygon = screen_polygon;
			// clip by all but top
			if (!clip_polygon_by_objsp_planes(polygon, {
						{ 2.0f, 0.0f, 0.0f, 1.0f},
						{-2.0f, 0.0f, 0.0f, 1.0f},
						{0.0f,  2.0f, 0.0f, 1.0f},
						{0.0f, 0.0f,  2.0f, 1.0f},
						{0.0f, 0.0f, -2.0f, 1.0f},
					}))
				continue;

			// The top side of the water has two triangles, separated by the
			// quad diagonal, so we need two convex polygons for the two convex
			// polytopes that have these triangles as top

			// if there is no top face, the full cube is filled with liquid
			auto top_corners_heights = getLiquidTopFaceHeights(npos, liquid_wave_params)
					.value_or(std::array{0.5f, 0.5f, 0.5f, 0.5f});

			std::array<v4f, 4> vertices{
				v4f{-0.5f, top_corners_heights[0], -0.5f, 1.0f},
				v4f{ 0.5f, top_corners_heights[1], -0.5f, 1.0f},
				v4f{-0.5f, top_corners_heights[2],  0.5f, 1.0f},
				v4f{ 0.5f, top_corners_heights[3],  0.5f, 1.0f},
			};

			// For smooth lit solid nodes, the quad diagonal depends on lighting
			QuadDiagonal quad_diag = QuadDiagonal::Diag02;
			if (features.drawtype == NDT_LIQUID && smooth_lighting) {
				// same as light_dirs[light_indices[0][0..4]] in content_mapblock.cpp
				constexpr v3s16 corners[4] = {
						v3s16(-1,  1,  1),
						v3s16( 1,  1,  1),
						v3s16( 1,  1, -1),
						v3s16(-1,  1, -1),
					};
				LightPair lights[4];
				for (int k = 0; k < 4; ++k) {
					lights[k] = LightPair(getSmoothLightSolid(npos, v3s16(0,1,0),
							corners[k], m_nodedef,
							[this](v3s16 p) { return getNode(p); }
						));
				}
				quad_diag = LightPair::quadDiagonalForFace(lights);
			}
			v4f triangle_split_plane;
			v4f triangle_split_plane_neg;
			std::array<int, 6> indices;
			if (quad_diag == QuadDiagonal::Diag02) {
				triangle_split_plane     = {-1.0f, 0.0f, -1.0f, 0.0f};
				triangle_split_plane_neg = { 1.0f, 0.0f,  1.0f, 0.0f};
				indices = {0, 1, 2,  1, 3, 2};
			} else {
				triangle_split_plane     = { 1.0f, 0.0f, -1.0f, 0.0f};
				triangle_split_plane_neg = {-1.0f, 0.0f,  1.0f, 0.0f};
				indices = {0, 1, 3,  0, 3, 2};
			}

			ConvexPolygon polygon1 = polygon;
			ConvexPolygon &polygon2 = polygon;

			if (clip_polygon_by_objsp_plane(polygon1, triangle_split_plane)
					&& clip_polygon_by_objsp_plane(polygon1,
							vector_cross_4d(vertices[indices[0]], vertices[indices[1]],
									vertices[indices[2]]))) {
				polygons.emplace_back(std::move(polygon1), post_color);
			}

			if (clip_polygon_by_objsp_plane(polygon2, triangle_split_plane_neg)
					&& clip_polygon_by_objsp_plane(polygon2,
							vector_cross_4d(vertices[indices[3]], vertices[indices[4]],
									vertices[indices[5]]))) {
				polygons.emplace_back(std::move(polygon2), post_color);
			}

		} else {
			// for other drawtypes: draw color fullscreen if camera in node
			if (floatToInt(m_camera_position, BS) != npos)
				continue;
			polygons.emplace_back(screen_polygon, post_color);
		}
	}

	return polygons;
}

video::SColorf ClientMap::renderPostFx()
{
	auto polygons = getPostFxPolygons();

	video::IVideoDriver *driver = SceneManager->getVideoDriver();

	for (const auto &[polygon, post_color] : polygons) {
		polygon.draw(driver, post_color);
	}

	// Color for wield item
	// We take the weighted (by area and alpha) sum of all polygons.
	// The result is alpha-premultiplied.
	video::SColorf color_sum(0.0f, 0.0f, 0.0f, 0.0f);

	for (const auto &[polygon, post_color] : polygons) {
		video::SColorf color = post_color;

		// (screen size and aspect ratio don't matter for relative screen area)
		f32 area = polygon.area();
		color.a *= area;

		color_sum.r += color.a * color.r;
		color_sum.g += color.a * color.g;
		color_sum.b += color.a * color.b;
		color_sum.a = color_sum.a + color.a;
	}

	if (color_sum.a > 1.0f) {
		f32 reci_a = 1.0f / color_sum.a;
		color_sum.a *= reci_a;
		color_sum.r *= reci_a;
		color_sum.g *= reci_a;
		color_sum.b *= reci_a;
	}

	return color_sum;
}

void ClientMap::PrintInfo(std::ostream &out)
{
	out<<"ClientMap: ";
}

void ClientMap::renderMapShadows(video::IVideoDriver *driver,
		ModifyMaterialCallback cb, s32 pass, int frame, int total_frames)
{
	bool is_transparent_pass = pass != scene::ESNRP_SOLID;
	std::string prefix;
	if (is_transparent_pass)
		prefix = "renderMap(SHADOW TRANS): ";
	else
		prefix = "renderMap(SHADOW SOLID): ";

	const auto mesh_grid = m_client->getMeshGrid();
	// Gets world position from block map position
	const auto get_block_wpos = [&] (v3s16 pos) -> v3f {
		return intToFloat(mesh_grid.getMeshPos(pos) * MAP_BLOCKSIZE - m_camera_offset, BS);
	};

	MeshBufListMaps &grouped_buffers = tl_meshbuflistmaps;
	DrawDescriptorList &draw_order = tl_drawdescriptorlist;
	grouped_buffers.clear();
	draw_order.clear();

	std::size_t count = 0;
	std::size_t meshes_per_frame = m_drawlist_shadow.size() / total_frames + 1;
	std::size_t low_bound = is_transparent_pass ? 0 : meshes_per_frame * frame;
	std::size_t high_bound = is_transparent_pass ? m_drawlist_shadow.size() : meshes_per_frame * (frame + 1);

	// transparent pass should be rendered in one go
	if (is_transparent_pass && frame != total_frames - 1) {
		return;
	}

	for (const auto &i : m_drawlist_shadow) {
		// only process specific part of the list & break early
		++count;
		if (count <= low_bound)
			continue;
		if (count > high_bound)
			break;

		v3s16 block_pos = i.first;
		MapBlock *block = i.second;

		// If the mesh of the block happened to get deleted, ignore it
		if (!block->mesh)
			continue;

		/*
			Get the meshbuffers of the block
		*/
		if (is_transparent_pass) {
			// In transparent pass, the mesh will give us
			// the partial buffers in the correct order
			for (auto &buffer : block->mesh->getTransparentBuffers())
				draw_order.emplace_back(get_block_wpos(block_pos), &buffer);
		} else {
			// Otherwise, group them
			grouped_buffers.addFromBlock(block_pos, block->mesh, driver);
		}
	}

	for (auto &map : grouped_buffers.maps) {
		for (auto &list : map) {
			transformBuffersToDrawOrder(
				list.second, draw_order, get_block_wpos, m_dynamic_buffers);
		}
	}

	TimeTaker draw("");

	core::matrix4 m; // Model matrix
	u32 drawcall_count = 0;
	u32 vertex_count = 0;
	u32 material_swaps = 0;

	// Render all mesh buffers in order
	drawcall_count += draw_order.size();

	bool translucent_foliage = g_settings->getBool("enable_translucent_foliage");

	// For translucent leaves, we want to use backface culling instead of frontface.
	std::vector<video::E_MATERIAL_TYPE> leaves_material;
	if (translucent_foliage) {
		auto *shdsrc = m_client->getShaderSource();
		// Find out all materials used by leaves so we can identify them
		leaves_material.reserve(m_nodedef->m_leaves_materials.size());
		for (u32 shader_id : m_nodedef->m_leaves_materials)
			leaves_material.push_back(shdsrc->getShaderInfo(shader_id).material);
	}

	for (auto &descriptor : draw_order) {
		if (!descriptor.m_reuse_material) {
			video::SMaterial local_material = descriptor.getMaterial();
			bool is_foliage = translucent_foliage && CONTAINS(leaves_material, local_material.MaterialType);
			cb(local_material, is_foliage);
			driver->setMaterial(local_material);
			++material_swaps;
		}

		m.setTranslation(descriptor.m_pos);

		driver->setTransform(video::ETS_WORLD, m);
		vertex_count += descriptor.draw(driver);
	}

	// restore the driver material state
	video::SMaterial clean;
	clean.BlendOperation = video::EBO_ADD;
	driver->setMaterial(clean);
	// This is somehow needed to fully reset the rendering state, or later operations
	// will be broken. (TODO why?)
	driver->draw3DLine(v3f(), v3f(), video::SColor(0));

	g_profiler->avg(prefix + "draw meshes [ms]", draw.stop(true));
	g_profiler->avg(prefix + "vertices drawn [#]", vertex_count);
	g_profiler->avg(prefix + "drawcalls [#]", drawcall_count);
	g_profiler->avg(prefix + "material swaps [#]", material_swaps);
}

void ClientMap::clearDrawListShadow()
{
	for (auto &i : m_drawlist_shadow) {
		MapBlock *block = i.second;
		block->refDrop();
	}
	m_drawlist_shadow.clear();
}

/*
	Custom update draw list for the pov of shadow light.
*/
void ClientMap::updateDrawListShadow(v3f shadow_light_pos, v3f shadow_light_dir, float radius, float length)
{
	ScopeProfiler sp(g_profiler, "CM::updateDrawListShadow()", SPT_AVG);

	clearDrawListShadow();

	// Number of blocks currently loaded by the client
	u32 blocks_loaded = 0;
	// Number of blocks with mesh in rendering range
	u32 blocks_in_range_with_mesh = 0;

	for (auto &sector_it : m_sectors) {
		const MapSector *sector = sector_it.second;
		if (!sector)
			continue;
		blocks_loaded += sector->size();

		/*
			Loop through blocks in sector
		*/
		for (const auto &entry : sector->getBlocks()) {
			MapBlock *block = entry.second.get();
			MapBlockMesh *mesh = block->mesh;
			if (!mesh) {
				// Ignore if mesh doesn't exist
				continue;
			}

			v3f block_pos = intToFloat(block->getPosRelative(), BS) + mesh->getBoundingSphereCenter();
			v3f projection = shadow_light_pos + shadow_light_dir * shadow_light_dir.dotProduct(block_pos - shadow_light_pos);
			if (projection.getDistanceFrom(block_pos) > (radius + mesh->getBoundingRadius()))
				continue;

			blocks_in_range_with_mesh++;

			// This block is in range. Reset usage timer.
			block->resetUsageTimer();

			// Add to set
			if (m_drawlist_shadow.emplace(block->getPos(), block).second) {
				block->refGrab();
			}
		}
	}

	g_profiler->avg("SHADOW MapBlock meshes in range [#]", blocks_in_range_with_mesh);
	g_profiler->avg("SHADOW MapBlocks drawn [#]", m_drawlist_shadow.size());
	g_profiler->avg("SHADOW MapBlocks loaded [#]", blocks_loaded);
}

void ClientMap::reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks)
{
	g_profiler->avg("CM::reportMetrics loaded blocks [#]", all_blocks);
}

void ClientMap::updateTransparentMeshBuffers()
{
	ScopeProfiler sp(g_profiler, "CM::updateTransparentMeshBuffers", SPT_AVG);
	u32 sorted_blocks = 0;
	u32 unsorted_blocks = 0;
	bool transparency_sorting_enabled = m_cache_transparency_sorting_distance > 0;
	f32 sorting_distance = m_cache_transparency_sorting_distance * BS;

	// Update the order of transparent mesh buffers in each mesh
	for (auto it = m_drawlist.begin(); it != m_drawlist.end(); it++) {
		MapBlock *block = it->second;
		MapBlockMesh *blockmesh = block->mesh;
		if (!blockmesh)
			continue;

		if (m_needs_update_transparent_meshes ||
				blockmesh->getTransparentBuffers().size() == 0) {
			bool do_sort_block = transparency_sorting_enabled;

			if (do_sort_block) {
				v3f mesh_sphere_center = intToFloat(block->getPosRelative(), BS)
						+ blockmesh->getBoundingSphereCenter();
				f32 mesh_sphere_radius = blockmesh->getBoundingRadius();
				f32 distance_sq = m_camera_position.getDistanceFromSQ(mesh_sphere_center);

				if (distance_sq > std::pow(sorting_distance + mesh_sphere_radius, 2.0f))
					do_sort_block = false;
			}

			if (do_sort_block) {
				blockmesh->updateTransparentBuffers(m_camera_position, block->getPos(),
						m_cache_transparency_sorting_group_by_buffers);
				++sorted_blocks;
			} else {
				blockmesh->consolidateTransparentBuffers();
				++unsorted_blocks;
			}
		}
	}

	g_profiler->avg("CM::Transparent Buffers - Sorted", sorted_blocks);
	g_profiler->avg("CM::Transparent Buffers - Unsorted", unsorted_blocks);
	m_needs_update_transparent_meshes = false;
}

std::optional<std::array<f32, 4>>
ClientMap::getLiquidTopFaceHeights(v3s16 pos,
		const std::optional<LiquidWaveParams> &wave_params)
{
	std::optional<std::array<f32, 4>> heights_opt = std::nullopt;

	const content_t node_c = getNode(pos).getContent();
	const ContentFeatures &node_f = m_nodedef->get(node_c);
	const content_t node_above_c = getNode(pos + v3s16(0, 1, 0)).getContent();

	if (node_f.drawtype == NDT_LIQUID) {
		if (MapblockMeshGenerator::isSolidFaceDrawn(node_c, node_f, node_above_c,
				m_nodedef))
			heights_opt = std::array<f32, 4>{0.5f, 0.5f, 0.5f, 0.5f};

	} else if (node_f.drawtype == NDT_FLOWINGLIQUID) {
		using LiquidNeighborData = MapblockMeshGenerator::LiquidData::NeighborData;

		content_t c_flowing = node_f.liquid_alternative_flowing_id;
		content_t c_source = node_f.liquid_alternative_source_id;
		u8 liquid_range = node_f.liquid_range;
		auto get_node_rel = [&](v3s16 relpos) -> MapNode {
			return getNode(pos + relpos);
		};

		std::array<std::array<LiquidNeighborData, 3>, 3> neighbors =
				MapblockMeshGenerator::getLiquidNeighborsData(c_source, c_flowing,
						liquid_range, get_node_rel);

		if (!neighbors[1][1].top_is_same_liquid) {
			std::array<std::array<f32, 2>, 2> corners =
					MapblockMeshGenerator::calculateLiquidCornerLevels(c_source,
							c_flowing, neighbors);

			heights_opt = {
					corners[0][0], corners[0][1],
					corners[1][0], corners[1][1]
				};
		}
	}

	if (!heights_opt || !wave_params || node_f.waving != 3)
		return heights_opt;

	// do the wave
	// Note: The same thing is implemented in the nodes vertex shader. Keep
	//       them consistent!
	auto wave_func = [&wave_params](v3f wavePos) {
		using v4f = std::array<f32, 4>;

		// Why does C++ still not have a normal mod function?!
		auto mod = [](f32 a, f32 b) {
			return a - b * std::floor(a/b);
		};

		//
		// Simple, fast noise function.
		// See: https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
		//
		auto perm = [&mod](v4f v) -> v4f {
			auto f = [&](f32 x) { return mod(((x * 34.0f) + 1.0f) * x, 289.0f); };
			return v4f{f(v[0]), f(v[1]), f(v[2]), f(v[3])};
		};

		auto snoise = [&perm, &mod](v3f p) -> f32 {
			auto v4add = [](v4f v1, v4f v2) {
				return v4f{v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2], v1[3] + v2[3]};
			};
			auto v4adds = [](v4f v, f32 s) {
				return v4f{v[0] + s, v[1] + s, v[2] + s, v[3] + s};
			};
			auto v4scale = [](v4f v, f32 s) {
				return v4f{v[0] * s, v[1] * s, v[2] * s, v[3] * s};
			};
			auto v3schur = [](v3f v1, v3f v2) {
				return v3f{v1.X * v2.X, v1.Y * v2.Y, v1.Z * v2.Z};
			};

			v3f a = v3f{std::floor(p.X), std::floor(p.Y), std::floor(p.Z)};
			v3f d = p - a;
			d = v3schur(v3schur(d, d), v3f(3.0f) - 2.0f * d);

			v4f b = v4add(v4f{a.X, a.X, a.Y, a.Y}, v4f{0.0f, 1.0f, 0.0f, 1.0f});
			v4f k1 = perm(v4f{b[0], b[1], b[0], b[1]});
			v4f k2 = perm(v4add(v4f{k1[0], k1[1], k1[0], k1[1]}, v4f{b[2], b[2], b[3], b[3]}));

			v4f c = v4adds(k2, a.Z);
			v4f k3 = perm(c);
			v4f k4 = perm(v4adds(c, 1.0f));

			auto f1 = [&](f32 x) { return mod(x * (1.0f / 41.0f), 1.0f); };
			v4f o1 = {f1(k3[0]), f1(k3[1]), f1(k3[2]), f1(k3[3])};
			v4f o2 = {f1(k4[0]), f1(k4[1]), f1(k4[2]), f1(k4[3])};

			v4f o3 = v4add(v4scale(o2, d.Z), v4scale(o1, 1.0f - d.Z));
			v2f o4 = v2f{o3[1], o3[3]} * d.X + v2f{o3[0], o3[2]} * (1.0f - d.X);

			return o4.Y * d.Y + o4.X * (1.0f - d.Y);
		};

		// The waves are slightly compressed along the z-axis to get
		// wave-fronts along the x-axis.
		wavePos.X /= wave_params->length * 3.0f;
		wavePos.Z /= wave_params->length * 2.0f;
		wavePos.Z += wave_params->animation_timer * wave_params->speed * 10.0f;
		return (snoise(wavePos) - 1.0f) * wave_params->height * 5.0f;
	};

	const v3f pos_bs = intToFloat(pos, BS);
	auto &heights = *heights_opt;
	heights[0] += wave_func(pos_bs + BS * v3f(-0.5f, heights[0], -0.5f)) * (1.0f/BS);
	heights[1] += wave_func(pos_bs + BS * v3f( 0.5f, heights[1], -0.5f)) * (1.0f/BS);
	heights[2] += wave_func(pos_bs + BS * v3f(-0.5f, heights[2],  0.5f)) * (1.0f/BS);
	heights[3] += wave_func(pos_bs + BS * v3f( 0.5f, heights[3],  0.5f)) * (1.0f/BS);

	return heights_opt;
}

video::SMaterial &DrawDescriptor::getMaterial()
{
	return (m_use_partial_buffer ? m_partial_buffer->getBuffer() : m_buffer)->getMaterial();
}

u32 DrawDescriptor::draw(video::IVideoDriver* driver)
{
	if (m_use_partial_buffer) {
		m_partial_buffer->draw(driver);
		return m_partial_buffer->getBuffer()->getVertexCount();
	} else {
		driver->drawMeshBuffer(m_buffer);
		return m_buffer->getVertexCount();
	}
}

bool ClientMap::isMeshOccluded(MapBlock *mesh_block, u16 mesh_size, v3s16 cam_pos_nodes)
{
	if (mesh_size == 1)
		return isBlockOccluded(mesh_block, cam_pos_nodes);

	v3s16 min_edge = mesh_block->getPosRelative();
	v3s16 max_edge = min_edge + mesh_size * MAP_BLOCKSIZE -1;
	bool check_axis[3] = { false, false, false };
	u16 closest_side[3] = { 0, 0, 0 };

	for (int axis = 0; axis < 3; axis++) {
		if (cam_pos_nodes[axis] < min_edge[axis])
			check_axis[axis] = true;
		else if (cam_pos_nodes[axis] > max_edge[axis]) {
			check_axis[axis] = true;
			closest_side[axis] = mesh_size - 1;
		}
	}

	std::vector<bool> processed_blocks(mesh_size * mesh_size * mesh_size);

	// scan the side
	for (u16 i = 0; i < mesh_size; i++)
	for (u16 j = 0; j < mesh_size; j++) {
		v3s16 offsets[3] = {
			v3s16(closest_side[0], i, j),
			v3s16(i, closest_side[1], j),
			v3s16(i, j, closest_side[2])
		};
		for (int axis = 0; axis < 3; axis++) {
			v3s16 offset = offsets[axis];
			int block_index = offset.X + offset.Y * mesh_size + offset.Z * mesh_size * mesh_size;
			if (check_axis[axis] && !processed_blocks[block_index]) {
				processed_blocks[block_index] = true;
				v3s16 block_pos = mesh_block->getPos() + offset;
				MapBlock *block;

				if (mesh_block->getPos() == block_pos)
					block = mesh_block;
				else
					block = getBlockNoCreateNoEx(block_pos);

				if (block && !isBlockOccluded(block, cam_pos_nodes))
					return false;
			}
		}
	}

	return true;
}
