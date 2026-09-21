# Custom Aurelion departure seating

Replaced the two decorative Radiance picnic-table forms in M13's departure
lounge with a custom three-place Aurelion seat. The new silhouette uses swept
ivory supports, suspended dark seating, segmented backrests, arm contact pads,
gold channels and keyed rear joints. Both seats face inward.

The editable Blender source, generator, FBX, studio render, manifest and original
placement census are in `Art/Source/Aurelion/Z12DepartureSeat`. The generator is
`build_z12_departure_seat.py`; use Blender 4.5 with `--background --factory-startup`
to avoid unrelated user add-on startup. The first build used the installed user
startup and logged add-on initialization/cleanup warnings, but the mesh export,
blend save and render completed successfully with exit zero.

The mesh is 0.94 × 3.533 × 0.9185 metres, 14,500 triangles, with two UV channels,
Nanite enabled, precision 10 and full fallback geometry. It uses existing owned
PavingIvory, Gold and Reveal materials. Procedural studio shading is not claimed
as an exported texture set. No marketplace asset was changed.

`fit_m13_departure_seats.py` imports the owned mesh and previews the replacement.
It verifies the two old meshes and poses, keeps the new horizontal bounds inside
the original furniture footprint, checks floor contact and preserves all other
actor transforms and collision settings. These seats were already decorative
with no collision; that behavior remains. Interactive sitting is not authored.

Preview `DepartureSeatPreview-20260920-233355-52950f29` passed all fit checks and
produced three inspected editor views: east seat, west seat and Selene's departure
background. `save_m13_departure_seats.py` requires that exact reviewed mesh hash,
map hashes and placement result. Saved run
`DepartureSeatSaved-20260920-233612-ab3b2dfb` backed up M13, saved only M13,
reloaded it and verified the mesh, poses, disabled collision/navigation influence,
unchanged other actors and unchanged M12 hash. It exited zero and the saved east
seat frame was inspected.

Live run `DepartureSeatLive-20260920-233909-bee76ae5` loaded the original earned
CP9 banks through the public save loader. It passed the existing journal,
evidence, identity, inventory/ammunition, resource, settled exit-position and HUD
checks. The new wrapper verifies both saved seats' actual mesh paths, poses,
visibility and disabled collision/navigation influence; the reused wall verifier
also verifies the previously saved custom wall modules and native wall poses.
Four gameplay frames were inspected: player, east seat, west seat and Selene's
background. Seating and HUD were readable; Selene's skin was normal in this run.
The camera was removed, map hashes were unchanged and the run exited zero.
The report retains `passed_requires_visual_review`; the frame review is recorded
here separately from its programmatic checks.

Validation: baseline full gate `20260920-230607-4f9b6952` passed before these
changes. Final full gate `20260920-234211-dbea61d3` passed the build without
SkipBuild, all 726 matching automation tests (99 warnings), exact report coverage
and unchanged source integrity. No packaged-build or GPU benchmark claim.

These are local visual improvements, not qualification of the entire level's
artistic quality, GPU performance, companion behavior or the 90% TDD objective.
The intermittent Selene skin defect remains separately documented in
`AurelionSeleneHeldColorProbe-2026-09-20.md`.
