# M13 departure wall reconstruction

The departure side walls used the same unrotated four-metre panel as the north
and south walls, compressed to 50 cm in X and repeated every 27.9 cm in Y.
This produced the visible fin-like side wall behind the protagonists. The front
and back rows also presented opposite faces with the same orientation.

The saved M13 wall dressing now uses 120 correctly oriented, unit-scale custom
coffers instead of 416 distorted stock panels. Three authored sizes fit the native
walls: 4.49, 4.19 and 3.99 metres wide, all 2.99 m high and 24.9 cm deep. Panels face
both sides of each wall. The six-metre-wide south doorway remains open; its existing
lintel is unchanged. Each module lies inside its corresponding native wall bounds.

The modules extend the existing Aurelion chamber kit with recessed stone panels,
chamfered frames, gold channels, keyed joints and relief registers. Each has 9,056
triangles, two UV channels, Nanite enabled and full fallback geometry. Existing
PavingIvory, Gold and Reveal materials are reused. No visual collision or navigation
influence is added. Native room bodies, all other actor transforms/collision,
departure actors, lighting, furniture, shuttles and mission semantics are preserved.

Editable Blender source, FBX files, a studio render and manifests are under
`Art/Source/Aurelion/Z12DepartureWalls`. Rebuild with:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --background --factory-startup --python Art/Source/Aurelion/build_z12_departure_coffers.py
```

`audit_m13_departure_props.py` records visible geometry and fixed editor views.
`audit_m13_departure_wall_fit.py` captures the full original wall batch and native
room bounds without rendering. `fit_m13_departure_walls.py` validates that exact
baseline, imports only the owned meshes and previews their placement. The save
wrapper requires the reviewed preview's mesh hashes and placements, backs up M13,
saves only M13 and checks exact instance readback after reloading.

Evidence under `Saved/Validation/Aurelion`:

- `DeparturePropsAudit-20260920-224032-491ebbef`: original scene inspected;
  the bench is existing furniture, while the malformed wall is the larger visual defect.
- `DepartureWallFit-20260920-224351-7925b41d`: complete 416-instance baseline
  and five native wall bodies captured read-only.
- `DepartureCofferPreview-20260920-224809-2ba00a13`: 120-module preview passed
  bounds and preservation checks; south/background, north and side views inspected.
- `DepartureCofferSaved-20260920-225020-8248eef7`: saved and reloaded exact
  preview geometry, unchanged protected M12, and clean exit without timeout.
  The saved background view was inspected.
- `DepartureCofferLive-20260920-225212-eeb4ca21`: earned CP9 public load passed
  the existing semantic, identity, inventory/resource and HUD checks. All three
  saved component batches (32/40/48 instances) and native wall poses matched.
  Three ordinary PIE images were captured; player and background images inspected.
  The temporary camera was removed, both maps stayed unchanged and the editor
  exited zero without timeout. The player/HUD view shows the reconstructed walls
  without the former fins. Selene's face is black in the background frame: this
  is a separate unresolved character-rendering failure, not a full visual pass.

Saved M13 SHA256: `2465BC4913F09C081E21175ECB6E472B987419FB4A500B9BF0D96C9BA7F97710`.
Protected M12 SHA256: `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.

The static and ordinary PIE views show improved wall proportions and kit consistency.
The inspected ordinary PIE frames have cleaner panel edges than the editor high-res
captures. Remaining furniture,
lintel, floor and lighting refinement, GPU profiling, moving-camera quality and
final AAA/90% TDD acceptance are not established by this placement change.

Validation: the full build without SkipBuild, all 726 matching automation tests,
report coverage and source integrity passed in `Saved/Validation/20260920-230607-4f9b6952`.
The pre-change full baseline was `20260920-221732-79c3404f`.
