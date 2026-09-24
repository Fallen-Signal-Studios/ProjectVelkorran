# M13 departure canopy source and engine preview

The last Higgsfield Z12 study was stopped after 29 minutes. Its camera view was
a coarse blockout and the scene repeatedly reported ignored invalid shared
edits. It yielded no accepted export. This pass returns the architecture to the
editable Blender source described below. The prior Z08 study was likewise
previsualization, not production geometry.

`Art/Source/Aurelion/build_z12_departure_canopy.py` derives five modules from
the approved `Art/References/Aurelion/Z12DepartureCanopy` concept: a 28 m
shallow vault rib, a separate terminal rib, a 7.5 m bearing foot, a 4 m coffer
cassette, and a 2.3 m hanging lens. It saves an editable `.blend`, separate FBX
meshes, a studio image and a manifest under `Art/Source/Aurelion/Z12DepartureCanopy`.
The modules have 3,572–26,132 triangles each and two UV channels. The retained
Unreal static meshes use existing Aurelion materials and Nanite with full
fallback geometry. They have no authored collision or navigation effect.

The kit fits the map's existing two 28 × 18 m scenic docks at X ±38 m, Y 475 m.
Its supports are at X ±13.2 m within each dock and its 5 m center lane remains
clear. The shuttles, dock floors, walls, roof, waypoints and physical map
geometry remain authoritative. The custom rib's 7.5 m bearings and 8.7 m crown
clear the shuttle hull, while the hanging lens is outside its wings.

`Scripts/Editor/preview_m13_departure_canopy.py` imports the owned assets,
spawns 102 **temporary, noncolliding** actors in M13, compares all original
actor transforms and collision before/after, and captures fixed editor views.
The final run is `Saved/Validation/Aurelion/Z12CanopyPendantPreview-20260923-152151-b67d1f35`.
It exited zero, wrote `canopy-preview.json` and three screenshots, and logged
no Python execution error. All 102 preview components reported NoCollision.
M12 and M13 map hashes stayed at `23555516…` and `74db4902…`; no map was saved.

The dock screenshots show a coherent canopy around both shuttles and visible
hanging lenses. They also show the present lighting is too dark for final
acceptance, and the player-height view from the playable lounge met the
original full side wall. A later [guarded placement](Validation/AurelionZ12CanopyPlacement-2026-09-23.md)
re-previewed the kit through the new Z12 viewports, then saved it to M13 and
verified the 102 visual actors after reload. Practical lighting, performance,
moving-camera and final art quality remain open; this is not a 90% TDD visual
pass.

Rebuild the source with Blender 4.5:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --background --factory-startup --python Art/Source/Aurelion/build_z12_departure_canopy.py
```
