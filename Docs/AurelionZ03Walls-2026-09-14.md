# Z03 sensor-gallery wall replacement

The original 68 vendor wall instances are replaced by 34 full-size sections on the same art actor: 28 four-metre bays, five 4.4-metre bays and one six-metre lintel. Two additional HISM components group the different module sizes. No actors or native collision objects are added or removed.

The Blender source is `Art/Source/Aurelion/Z03WallKit/Aurelion-Z03-Walls.blend`, reproducible with `build_z03_wall_kit.py`. The design uses dressed ivory courses, recessed dark fields, narrow gold conduits, stepped cornices and a concealed upper backing that closes the gap to the coffers. The 4 m and 4.4 m modules each contain 11,308 triangles; the lintel contains 4,336. All have two UV channels, Nanite position precision 10 and full fallback geometry.

The visible wall faces stay at the native interior boundary. The 55 cm upper backing occupies the ceiling depth; the north opening remains 600 cm wide and 450 cm high. Every placement uses unit scale. Native room walls, scanner poses, service-ramp poses and collision remain authoritative and unchanged. These perimeter modules are not approved for functional Chaos destruction.

## Verification and limits

- Clean Blender FBX round-trip passed for all three meshes: dimensions, origin, finite geometry, material slots and UVs.
- The scoped axis-aligned coplanar audit reports zero overlaps for each source mesh. This does not establish absence of all intersections or all rendering defects.
- Final preview `Z03WallFullClosure-20260914-204119-8958ae20` completed with all three views inspected. The close view verifies the corrected upper closure; earlier shallow backing was rejected after visual inspection.
- `check_z03_walls.py` verifies instance poses, bounds, unit scales, import settings, absence of added collision, doorway/interior clearance, and the preceding native-room preservation checks.
- Saved run `Z03WallsSaved-20260914-204532-290fcf10` backed up the map, saved it and captured three reviewed views. Capture warmup returned from 64 to its original 4 frames and the temporary camera was removed.
- Fresh run `Z03WallsFresh-20260914-204845-81683cda` completed with 74 passing architecture verification reports, 3140 actors, a clean map and no Python errors.

The room still contains generic piers, frozen cover, service-ramp rails, broad guidance strips and technical signage. Those remain further environment work. Live scanner traversal, final art acceptance and packaged performance are not qualified by these static checks. The supported slice estimate remains 63.75%; this pass does not establish 90% TDD alignment or AAA completion.
