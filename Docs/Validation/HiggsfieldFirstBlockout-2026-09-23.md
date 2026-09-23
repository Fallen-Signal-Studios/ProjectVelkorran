# Higgsfield first blockout review — 23 September 2026

Scene: [original 3D Jutsu blockout](https://higgsfield.ai/3d-jutsu/3bbaa0c7-dc68-487a-a44d-ea14bd67f87c). Reviewed in Compose and Plan views on 23 September. This is a visual review, not an Unreal import or dimensional audit.

## Decision

**Keep as spatial previsualization only. Do not replace any M12/M13 gameplay architecture with this scene.** The visible scene communicates a central machine and a paired approach rhythm, which can inform staging. It is still a coarse blockout. The camera preview is black, the main perspective does not give a readable player-eye route through the structure, and the editor reports: “Some shared scene edits were ignored because their data is invalid.” The plan view suggests an approach axis, but does not establish the adopted Z05 six-meter bridge, 18-meter shaft opening, safe guardrails, meeting platform, or combat camera clearances. The original project remains untouched.

I triggered its GLB export in the browser and saw the “Exported GLB” notification, but the export did not appear in the local Downloads/workspace locations checked. Therefore topology, dimensions, UVs, materials, collision and performance remain unverified. It is not an accepted engine asset.

## Next visual-development step

The accepted [Z08 breached containment pylon reference](../ArtReferences/AurelionArchitecture-2026-09-23/Z08-Breached-Containment-Pylon-Higgsfield-Reference.png) was generated before starting the [image-guided Z08 3D Jutsu study](https://higgsfield.ai/3d-jutsu/f215d455-626c-4ad8-b251-fb1cd98f8876). The new study asks for one noninteractive module rather than a replacement level, roughly 8 × 4 × 2 m, with a separate removable Eclipse overlay and a clear 4–6 m route in front. The room in the reference is a composition guide; the September layout contract remains the authority for playable geometry. Any eventual export must pass scale, topology, normals, UV, material-slot, collision, Nanite, lighting and PIE navigation checks before Unreal use.

The separate [Z05 measured atrium previsualization](https://higgsfield.ai/3d-jutsu/b1ecae1e-19a9-42f8-bfca-a9a449c49093) was still generating at the time of this review. Its incomplete current camera was not used as evidence for this decision.

At the later 23 September check, the image-guided Z08 study was still
generating after about 30 minutes. Its assistant reported that the removable
Eclipse overlay and a second camera render had been built, but the visible
Compose viewport remained an unreadable dark close-up and the page still
reported invalid shared scene edits. The assistant was waiting on a render.
No completed visual review, export, or Unreal import is claimed yet.

## Completed Z08 image-guided study

The [Z08 study](https://higgsfield.ai/3d-jutsu/f215d455-626c-4ad8-b251-fb1cd98f8876)
finished at revision 18. I inspected the corrected 1920×1080 still and its
actual GLB, preserved as [render](../../Art/References/Aurelion/Z08HiggsfieldPrevis/quarantine_crucible_final.png)
and [geometry](../../Art/References/Aurelion/Z08HiggsfieldPrevis/scene-r18.glb).
The corrected image puts the violet infection on the left and balcony on the
right. Its ivory field, black base and gold registers fit the requested
palette. The scene has separate `HERO_pylon_root`, `HERO_wall_root` and
`INF_overlay_root` hierarchies, which is useful for an authored removable
infection layer. The exported file is valid GLB 2.0, 338,728 bytes, SHA-256
`c5af27aab017ccbbf0c1c33534cfd46b86bf17e4d5b1fba26830ea4f6590d8fb`.

**Use it as previsualization, not as a production UE asset.** The GLB has 49
unique meshes and 8,556 unique triangles, but the whole hero pylon is only
728 instanced triangles; the wall is 2,356. It has nine constant-color
materials, no embedded images or textures, UV0 on only 9 of 49 mesh
primitives, and no UV1. The still crops the pylon crown and base and the
infection reads as sparse beads and hairline violet strands rather than
organic tissue. The browser also retains an invalid-shared-edits warning.
Those findings make topology, surface detail, authored UVs, material quality,
light response and player-eye composition insufficient for the requested
high-detail Aurelion kit. No GLB was imported into the game, no collision or
navigation was changed, and no 90% alignment credit is claimed. A detailed
Blender rebuild can use the separable hierarchy and approximate 8 m height
as a blockout guide, then must pass the project's normal export, lighting,
collision, Nanite and PIE route checks.
