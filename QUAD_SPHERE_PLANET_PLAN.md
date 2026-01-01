# Quad-Sphere Planet Implementation Plan for Luanti

## Executive Summary

This document outlines the plan to transform Luanti from an infinite flat world into a finite spherical planet using **quad-sphere (cubed sphere)** mathematics. The quad-sphere approach maps a cube's six faces onto a sphere, providing:

- Uniform cell distribution (no polar singularities)
- Efficient coordinate addressing per face
- Natural LOD subdivision
- Seamless wrapping at edges

---

## Phase 1: Foundation - Coordinate System & Core Math

### 1.1 Quad-Sphere Mathematics Module

**New File:** `src/quadsphere/quadsphere.h`

Create the core mathematical foundation:

```cpp
// Cube face enumeration
enum class CubeFace : u8 {
    FRONT = 0,  // +Z
    BACK,       // -Z
    RIGHT,      // +X
    LEFT,       // -X
    TOP,        // +Y
    BOTTOM      // -Y
};

// Position on quad-sphere surface
struct QuadSpherePos {
    CubeFace face;      // Which cube face
    f32 u, v;           // Position on face [0, 1]
    f32 altitude;       // Height above/below sphere surface
};

// Core conversion functions
v3f cubeToSphere(CubeFace face, f32 u, f32 v, f32 radius);
QuadSpherePos sphereToCube(v3f worldPos, f32 radius);
v3f cartesianToQuadSphere(v3f cartesian, f32 radius);
```

**Key algorithms to implement:**
- **Gnomonic projection** - Map cube face coordinates to sphere surface
- **Inverse projection** - Convert world position back to face + UV
- **Face transition** - Handle crossing from one face to another
- **Normal calculation** - Surface normal at any point (for gravity)

### 1.2 Planet Configuration

**New File:** `src/quadsphere/planet_config.h`

```cpp
struct PlanetConfig {
    f32 radius = 6400.0f * BS;     // Planet radius in world units (~64km)
    s32 surface_blocks = 256;      // Blocks per face edge at surface
    s32 max_altitude = 64;         // Max blocks above surface
    s32 min_altitude = -64;        // Max blocks below surface (caves/core)
    v3f center = v3f(0, 0, 0);     // Planet center in world space
};
```

### 1.3 Block Addressing System

**Modify:** `src/mapblock.h`, create `src/quadsphere/sphere_block_pos.h`

Replace flat v3s16 block positions with sphere-aware addressing:

```cpp
struct SphereBlockPos {
    CubeFace face;          // Which cube face
    s16 u, v;               // Position on face grid
    s16 depth;              // Altitude layer (-64 to +64)

    // Convert to/from legacy v3s16 for compatibility
    v3s16 toLegacy() const;
    static SphereBlockPos fromLegacy(v3s16 pos);

    // Get world-space center of this block
    v3f getWorldCenter(const PlanetConfig& config) const;

    // Get neighboring blocks (handles face transitions)
    SphereBlockPos getNeighbor(Direction dir) const;
};
```

### 1.4 Compatibility Layer

**Strategy:** Maintain v3s16 internally but add conversion layer

- Keep existing MapBlock storage using v3s16
- Create mapping functions between SphereBlockPos and v3s16
- Use a virtual "unfolded cube" coordinate space internally:
  ```
  Face layout in v3s16 space:
       [TOP]
  [LEFT][FRONT][RIGHT][BACK]
       [BOTTOM]
  ```

---

## Phase 2: Map System Modifications

### 2.1 Sphere-Aware Map Sectors

**Modify:** `src/mapsector.h`, `src/map.h`

Current sectors are 2D (x, z) slices. For sphere:

```cpp
class SphereMapSector : public MapSector {
    CubeFace m_face;
    s16 m_u, m_v;  // Position on face

    // Override block access to handle sphere coordinates
    MapBlock* getBlockNoCreateNoEx(s16 depth);
};
```

### 2.2 Block Loading Strategy

**Modify:** `src/client/clientmap.h`, `src/servermap.h`

Change from Euclidean distance to **geodesic distance** for:
- Block loading radius
- View distance calculations
- Active block range

```cpp
// Calculate geodesic distance on sphere surface
f32 geodesicDistance(const SphereBlockPos& a, const SphereBlockPos& b,
                      const PlanetConfig& config);

// Get blocks within geodesic radius of player
std::vector<SphereBlockPos> getBlocksInGeodesicRange(
    const QuadSpherePos& playerPos,
    f32 range,
    const PlanetConfig& config);
```

### 2.3 Face Boundary Handling

**New File:** `src/quadsphere/face_seams.h`

Handle seamless transitions between cube faces:

```cpp
class FaceSeamHandler {
public:
    // Get the block on the adjacent face when crossing boundary
    SphereBlockPos crossBoundary(const SphereBlockPos& from, Direction dir);

    // Get neighbor blocks for mesh generation (may span faces)
    std::vector<SphereBlockPos> getNeighborhood(const SphereBlockPos& center);

    // Rotate coordinates when transitioning between faces
    void transformCoordinates(CubeFace fromFace, CubeFace toFace,
                              s16& u, s16& v);
};
```

---

## Phase 3: Rendering System

### 3.1 Spherical Mesh Generation

**Modify:** `src/client/mapblock_mesh.h`, `src/client/mapblock_mesh.cpp`

Current mesh generation places vertices on a flat grid. Changes needed:

```cpp
class SphereMeshMakeData : public MeshMakeData {
    PlanetConfig m_planet;
    SphereBlockPos m_sphere_pos;

    // Transform flat vertex positions to sphere surface
    v3f transformVertex(v3f localPos) const;

    // Calculate proper normals for lighting on curved surface
    v3f calculateSphereNormal(v3f worldPos) const;
};
```

**Vertex transformation:**
```cpp
v3f SphereMeshMakeData::transformVertex(v3f localPos) const {
    // 1. Convert local block position to face UV
    // 2. Apply gnomonic projection to get sphere point
    // 3. Apply altitude offset along normal
    // 4. Return world-space position
}
```

### 3.2 Spherical Frustum Culling

**Modify:** `src/client/camera.h`, `src/client/clientmap.cpp`

Replace planar frustum with sphere-aware culling:

```cpp
class SphereFrustumCuller {
    v3f m_camera_pos;      // World position
    v3f m_view_dir;        // View direction (tangent to sphere)
    f32 m_fov;
    PlanetConfig m_planet;

public:
    // Check if a sphere block is potentially visible
    bool isBlockVisible(const SphereBlockPos& block) const;

    // Account for horizon occlusion (blocks behind curve)
    bool isAboveHorizon(const SphereBlockPos& block) const;

    // Get visible block range efficiently
    std::vector<SphereBlockPos> getVisibleBlocks(f32 maxDistance) const;
};
```

**Horizon calculation:**
```cpp
bool SphereFrustumCuller::isAboveHorizon(const SphereBlockPos& block) const {
    v3f blockCenter = block.getWorldCenter(m_planet);
    v3f toBlock = blockCenter - m_camera_pos;
    v3f toCenter = m_planet.center - m_camera_pos;

    // Block is above horizon if angle from camera-to-center
    // is less than horizon angle
    f32 horizonAngle = acos(m_planet.radius / toCenter.getLength());
    f32 blockAngle = acos(toCenter.dotProduct(toBlock) /
                          (toCenter.getLength() * toBlock.getLength()));

    return blockAngle < horizonAngle + BLOCK_ANGULAR_SIZE;
}
```

### 3.3 Level of Detail (LOD) System

**New File:** `src/client/sphere_lod.h`

Quad-sphere naturally supports LOD through quad-tree subdivision:

```cpp
class SphereLOD {
public:
    // Calculate appropriate LOD level based on distance
    u8 getLODLevel(const SphereBlockPos& block,
                   const QuadSpherePos& camera) const;

    // Get mesh resolution for LOD level
    u8 getMeshResolution(u8 lodLevel) const;

    // Handle LOD transitions at boundaries
    void stitchLODBoundary(MapBlockMesh* highRes, MapBlockMesh* lowRes);
};
```

### 3.4 Skybox and Atmosphere

**Modify:** `src/client/sky.h`, `src/client/sky.cpp`

Current skybox is a surrounding cube. For planet view:

```cpp
class PlanetSky : public Sky {
    // When on surface: sky dome above, ground below
    // When in space: see planet as sphere

    void renderAtmosphericScattering();  // Blue sky gradient
    void renderStars(f32 timeOfDay);     // Visible stars
    void renderHorizon();                // Curved horizon line
};
```

---

## Phase 4: Physics and Player

### 4.1 Spherical Gravity

**Modify:** `src/client/localplayer.cpp`, `src/collision.h`

Make gravity point toward planet center:

```cpp
v3f calculateGravityDirection(v3f playerPos, const PlanetConfig& planet) {
    v3f toCenter = planet.center - playerPos;
    return toCenter.normalize();
}

// In LocalPlayer::move()
v3f gravityDir = calculateGravityDirection(m_position, m_planet_config);
v3f gravityAccel = gravityDir * (movement_gravity * physics_override.gravity);
m_speed += gravityAccel * dtime;
```

### 4.2 Player Orientation

**Modify:** `src/client/localplayer.h`

Player "up" changes based on position on sphere:

```cpp
class LocalPlayer {
    // Current local coordinate frame
    v3f m_local_up;      // Points away from planet center
    v3f m_local_forward; // Movement direction (tangent to sphere)
    v3f m_local_right;   // Cross product

    void updateLocalFrame() {
        m_local_up = (m_position - m_planet.center).normalize();
        // Maintain forward direction relative to up
        m_local_right = m_local_forward.crossProduct(m_local_up).normalize();
        m_local_forward = m_local_up.crossProduct(m_local_right).normalize();
    }
};
```

### 4.3 Collision Detection on Sphere

**Modify:** `src/collision.cpp`

Collision boxes need to align with local "up":

```cpp
collisionMoveResult sphereCollisionMoveSimple(
    Environment *env,
    const aabb3f &box,
    const v3f &localUp,  // NEW: local coordinate frame
    f32 dtime,
    v3f *pos_f,
    v3f *speed_f,
    v3f accel_f
) {
    // Transform collision calculations to local tangent plane
    // Rotate AABB to align with local up direction
    // Perform collision in local space
    // Transform results back to world space
}
```

---

## Phase 5: World Generation

### 5.1 Sphere Mapgen Base Class

**New File:** `src/mapgen/mapgen_sphere.h`

```cpp
class MapgenSphere : public Mapgen {
protected:
    PlanetConfig m_planet;

    // Convert block position to latitude/longitude for biome calculation
    void blockToLatLon(const SphereBlockPos& pos, f32& lat, f32& lon);

    // Height at a point on sphere surface
    virtual f32 getSurfaceHeight(f32 lat, f32 lon) = 0;

    // Generate terrain for a block
    void generateTerrain(SphereBlockPos pos, MapBlock* block);

public:
    virtual void makeChunk(BlockMakeData *data) override;
};
```

### 5.2 Spherical Noise Functions

**New File:** `src/mapgen/sphere_noise.h`

Standard Perlin noise creates seams at cube edges. Solutions:

```cpp
class SphereNoise {
public:
    // Noise that tiles correctly on sphere surface
    f32 noise3D_sphere(v3f spherePoint, const NoiseParams& params);

    // Alternative: Use 4D noise with 3D sphere embedded in 4D
    f32 noise4D_sphereProjected(f32 u, f32 v, CubeFace face,
                                 const NoiseParams& params);
};
```

**Seamless noise approach:**
```cpp
// Use 3D position on unit sphere as noise input
f32 SphereNoise::noise3D_sphere(v3f spherePoint, const NoiseParams& params) {
    // spherePoint is normalized position on sphere
    // Use it directly as 3D noise coordinates
    return noise3d_perlin(spherePoint.X * params.scale,
                          spherePoint.Y * params.scale,
                          spherePoint.Z * params.scale,
                          params.seed, params.octaves, params.persist);
}
```

### 5.3 Biome Distribution

**Modify:** `src/mapgen/mg_biome.h`

Biomes based on latitude (climate zones):

```cpp
class SphereBiomeGen : public BiomeGen {
    // Temperature decreases with latitude
    f32 getTemperature(f32 latitude, f32 altitude);

    // Humidity varies by region
    f32 getHumidity(f32 latitude, f32 longitude);

    // Select biome based on climate
    const Biome* getBiomeAtPoint(f32 lat, f32 lon, f32 altitude);
};
```

---

## Phase 6: Networking and Multiplayer

### 6.1 Position Synchronization

**Modify:** `src/network/networkprotocol.h`

Current protocol sends v3f positions. Options:
1. Keep v3f (world coordinates) - minimal changes
2. Send QuadSpherePos for precision at large distances

Recommendation: Keep v3f for compatibility, planet radius is small enough.

### 6.2 Block Request Protocol

**Modify:** `src/network/clientpackethandler.cpp`, `src/network/serverpackethandler.cpp`

Block requests need sphere-aware priority:
- Prioritize blocks near player on geodesic path
- Handle face transitions in block streaming

---

## Phase 7: Integration and Polish

### 7.1 Compatibility with Existing Mods

**Strategy:** Provide compatibility API

```lua
-- New sphere API for mods
minetest.get_sphere_position(pos)  -- Returns {face, u, v, altitude}
minetest.get_gravity_direction(pos)  -- Returns unit vector toward center
minetest.get_surface_normal(pos)  -- Returns local "up" direction
minetest.geodesic_distance(pos1, pos2)  -- Distance along surface
```

### 7.2 World Format Migration

**New feature:** Convert existing flat worlds to sphere cap

```cpp
class WorldMigrator {
    // Map a flat region onto one face of sphere
    void migrateRegionToFace(const v3s16& minCorner,
                              const v3s16& maxCorner,
                              CubeFace targetFace);
};
```

### 7.3 Debug and Visualization Tools

```cpp
// Debug rendering
void renderCubeWireframe();  // Show underlying cube structure
void renderFaceBoundaries(); // Highlight face edges
void renderGravityVectors(); // Show gravity direction
void renderHorizonLine();    // Show calculated horizon
```

---

## Implementation Order

### Sprint 1: Core Math (Estimated: Foundation work)
1. [ ] Create `src/quadsphere/` directory structure
2. [ ] Implement `quadsphere.h` - core projection math
3. [ ] Implement `planet_config.h` - configuration
4. [ ] Implement `sphere_block_pos.h` - block addressing
5. [ ] Unit tests for all coordinate conversions

### Sprint 2: Rendering (Visual feedback)
1. [ ] Modify `mapblock_mesh.cpp` for vertex transformation
2. [ ] Implement basic spherical frustum culling
3. [ ] Test with single cube face (flat, no curvature yet)
4. [ ] Enable full curvature rendering
5. [ ] Implement horizon culling

### Sprint 3: Physics (Playable)
1. [ ] Implement spherical gravity in `localplayer.cpp`
2. [ ] Update player orientation (local up vector)
3. [ ] Modify collision detection for sphere
4. [ ] Test walking around curved surface

### Sprint 4: Map System (Persistent world)
1. [ ] Modify `MapSector` for sphere addressing
2. [ ] Implement face boundary transitions
3. [ ] Update block loading/unloading for geodesic distance
4. [ ] Test with multiple faces active

### Sprint 5: World Generation (Content)
1. [ ] Create `MapgenSphere` base class
2. [ ] Implement seamless sphere noise
3. [ ] Port one existing mapgen (v7) to sphere
4. [ ] Implement latitude-based biomes

### Sprint 6: Polish (Release ready)
1. [ ] Skybox and atmosphere rendering
2. [ ] LOD system implementation
3. [ ] Mod API for sphere functions
4. [ ] Performance optimization
5. [ ] Documentation and examples

---

## Technical Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Precision loss at large coordinates | Rendering artifacts | Use planet-center-relative coordinates |
| Seams at cube face edges | Visual artifacts | Careful boundary handling, extra overlap |
| Performance with curved frustum | Low FPS | Hierarchical culling, caching |
| Mod compatibility breakage | User frustration | Compatibility layer, gradual migration |
| Collision edge cases | Players falling through world | Extensive testing, fallback systems |

---

## File Structure Summary

```
src/
├── quadsphere/
│   ├── quadsphere.h          # Core projection math
│   ├── quadsphere.cpp
│   ├── planet_config.h       # Planet parameters
│   ├── sphere_block_pos.h    # Block addressing
│   ├── sphere_block_pos.cpp
│   ├── face_seams.h          # Boundary handling
│   └── face_seams.cpp
├── mapgen/
│   ├── mapgen_sphere.h       # Sphere mapgen base
│   ├── mapgen_sphere.cpp
│   └── sphere_noise.h        # Seamless noise
├── client/
│   ├── sphere_frustum.h      # Spherical culling
│   ├── sphere_lod.h          # LOD system
│   └── [modified existing files]
└── [modified existing files]
```

---

## Success Criteria

1. **Visual**: Standing on surface, horizon curves visibly at distance
2. **Physics**: Walking continuously, arrive back at start point
3. **Seamless**: No visible seams at cube face boundaries
4. **Performance**: 60 FPS with 200+ block view distance
5. **Generation**: Coherent terrain across entire planet surface

---

## References

- [Cube-to-Sphere Projection](https://mathproofs.blogspot.com/2005/07/mapping-cube-to-sphere.html)
- [Spherical Cube Mapping](https://www.rorydriscoll.com/2012/01/15/cubemap-texel-solid-angle/)
- [Geodesic Distance on Sphere](https://www.movable-type.co.uk/scripts/latlong.html)
- [Seamless Noise on Spheres](http://www.iquilezles.org/www/articles/spherenoise/spherenoise.htm)

---

*This plan provides a roadmap for transforming Luanti into a spherical planet game. Each phase builds on the previous, allowing for incremental testing and validation.*
