# Z08 custom containment pylon — first engine review

The [accepted Z08 reference](../ArtReferences/AurelionArchitecture-2026-09-23/Z08-Breached-Containment-Pylon-Higgsfield-Reference.png)
guided this authored Blender source. The final Higgsfield study was useful as
previsualization, but its exported GLB lacked production UVs and sufficient
surface detail, so it was not imported into Unreal. This work uses no further
Higgsfield generation or purchased asset.

`Art/Source/Aurelion/build_z08_containment_pylon.py` builds a measured
3.94 × 2.215 × 7.925 m clean pylon with 49,476 triangles, two UV channels and
one solid UCX collision hull. The left-side Eclipse growth is a separate
5,880-triangle mesh with two UV channels and no collision, so it can be removed
without replacing the structure. Gold instrumentation, nested stone cassettes
and a mechanical annulus stay exposed on the clean right side. Both FBX files
passed clean-process Blender import, mesh count, material-slot, triangle, UV
and collision census. The editable `.blend`, author script, manifest and studio
preview are preserved in `Art/Source/Aurelion/Z08ContainmentPylon/`.

The Unreal import created two static meshes under
`/Game/Aurelion/Environment/ArchitectureKit/Meshes` with existing Aurelion
stone, gold and Eclipse materials. The importer verified bounds, two UV
channels, one structural convex hull and zero overlay collision, assigned
lightmap channel 1 and enabled Nanite. They were placed only in
`/Game/Aurelion/ArtReview/L_Aurelion_Z08ContainmentPylon` with a scale figure;
the campaign maps were not edited. The lit front-side capture is
[here](AurelionZ08ContainmentPylon-2026-09-23/engine-review.png). Its source
render is `Art/Source/Aurelion/Z08ContainmentPylon/pylon-source.png`.

The initial Unreal screenshot showed the blank back because the review actors
had the wrong yaw. An isolated correction set both to 180 degrees. A second
capture was too dark to assess, so two local fill lights and a closer camera
were saved in the review map. The final capture reveals a coherent silhouette,
but the stone finish remains flat and the organic growth is too simple against
the concept. This is **a source candidate, not accepted AAA campaign
architecture**. It needs another sculpt/material pass, actual Z08 wall-side
fit, player-eye composition, route and collision checks, and a representative
combat/performance test before placement. No 90% TDD alignment credit is
claimed for this isolated review. All three editor scripts confirmed the
current M12 map file was unchanged; the latest lit review run is
`Z08PylonLitReview-20260923-171327-4c7c7c6d`.
