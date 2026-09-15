# Z04 relay overlook masonry

The complete 188-instance vendor wall batch is replaced by 48 custom double-sided sections: 28 four-metre end bays, eighteen side bays measuring 38/9 metres, and two six-metre lintels. Three Blender meshes supply the batch, with unit-scale Unreal placements on the original art actor.

The new walls have continuous stone cores, dressed ashlar and relief fields, fine incisions, stepped socles and cornices, maintenance registers and narrow axial conductors on both sides. Their 50 cm thickness matches the native wall envelope. Wall height remains 7 m; the two 6 m-wide doorways retain 4.5 m clear height. Native wall, floor, ramp, guard and cover actors retain their original transforms, mesh references and collision modes.

## Evidence and limits

- Read-only survey `Z04ArchitectureAudit-20260914-222427-d25d509e` measured the 62 x 38 m room and captured its existing interior. The source folder retains the room and vendor-batch baselines.
- Three clean FBX round trips pass geometry, bounds, material-slot and two-UV-channel checks. All three scoped coplanar audits report zero overlaps after correcting a seam/cornice intersection.
- `check_z04_walls.py` verifies all 48 placements, independent end/side coverage, both door openings, unit scale, Nanite precision 10, full fallback geometry and no added mesh collision. It preserves the measured native room state.
- Preview `Z04WallsPreview-20260914-223048-2c9d697a` passed without Python errors. Exterior frontage, interior and wall detail were inspected.
- Saved run `Z04WallsSaved-20260914-223353-cfc20fc3` backed up the map and produced three reviewed saved views. Fresh run `Z04WallsFresh-20260914-223631-41edfa85` completed with 79 passing architecture reports, 3140 actors, a clean map and no Python errors.

The existing ceiling is extremely dark in the interior views. Its geometry and lighting need further work, along with Z04 floors, balcony, rails, piers, crates, consoles and annotations. This wall pass does not qualify final room quality, live traversal, Chaos destruction or packaged performance. The supported slice estimate remains 63.75%; the full 90% goal remains open.
