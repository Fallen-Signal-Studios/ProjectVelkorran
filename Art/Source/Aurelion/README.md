# Aurelion recess wall candidate

Source-art candidate for the layout plan's white stone and narrow gold channels
(Aurelion_Level_Layout_Plan.pdf, pages 20 and 23). Ten panels are now installed in M12.
The panel does not resolve survivor targeting or prove a safe recess entrance.

`SM_Aurelion_RecessPanel_2m.blend` contains the editable mesh and studio scene;
the FBX contains only the panel and one named UCX box. The PNG is a Blender
studio preview, not an Unreal gameplay screenshot. Four material slots separate
stone, gold, shadow channels and warm light inlays. The imported asset uses three
existing Radiance materials and a dedicated warm emissive inlay material.

Dimensions are 2 m wide, 0.4 m deep and 3 m tall, with a bottom-centre pivot.
The detailed front faces local -Y. UV0 uses packed islands; Unreal lightmap UVs
and final rendering settings remain pending. Simple collision fills the complete
envelope. Do not scale this panel onto existing wall segments without checking
their dimensions, passage clearance and both priority/cinematic exit paths.

Rebuild using Blender 4.5:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Aurelion/build_recess_panel.py
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Aurelion/verify_recess_panel.py
```

The clean-process FBX round-trip verifies two mesh objects, metre dimensions,
bottom-centre visual pivot, UVs, four material slots and eight collision vertices.
See the adjacent verification JSON for exported triangle count. The preview was
visually inspected on 2026-09-13. Unreal placement and preserved physical-wall
checks passed; runtime navigation and performance acceptance remain open.

## Departure shuttle exteriors

`build_departure_shuttles.py` authors two editable Blender scenes and FBX static
meshes following the layout plan's page 20 faction direction. Dominion uses a
broader oxblood cabin and bronze trim; Reformation uses a slimmer navy cabin,
slate outriggers and exposed connections. Each has five material slots and UV0.
Source JSON records dimensions, triangle count, palette and exported-file hash.
Run with the same Blender CLI above, substituting this script's filename.

The two `*-preview.png` files are Blender studio renders. The models are imported
under `/Game/Aurelion/Environment/Blender/Shuttles` and placed in M13 on the
existing departure docks. They contain no collision, interiors or flight rigs.
Their four landing pads define the bottom pivot. Unreal actors use yaw 180.
Import verifies centimetre bounds against source metre bounds and resolves all
material slots. Placement preserves the original physical actors and hides only
the isolated 112-instance old ship dressing component; the adjacent 60-instance
stone component remains visible. Original hidden cube actors are retained.

The September 23 detail revision follows the [paired generated reference](../../../Docs/ArtReferences/AurelionArchitecture-2026-09-23/Z12-Departure-Shuttles-Detail.png): both source hulls gained tapered prows, forward glazing, layered armor and landing/nacelle fittings while retaining their dock envelopes and zero collision. `verify_departure_shuttles.py` round-trips both current FBXs through fresh Blender import and checks the two UV channels, five material slots, dimensions and triangle counts. The [engine review](../../../Docs/Validation/AurelionM13ShuttleRefinement-2026-09-23.md) distinguishes this incremental improvement from final hero quality.

These are first-pass exterior assets, not final hero-asset acceptance. Cockpit,
surface detail, rear lighting, central approach composition, boarding/flight
animation and earned-route cinematic review remain open. See
`Docs/AurelionFullVisualPass-2026-09-13.md` for editor evidence and limitations.

## Crucible upper enclosure

`build_crucible_vault.py` authors `SM_Aurelion_CrucibleVault` as a static upper
enclosure above the existing seven-metre wall head. Five angular portal ribs,
inset bays and longitudinal roof coffers follow the architectural direction on
layout-plan page 23. The enclosure is 71.4 x 49.4 x 15.4 m, 25,488 triangles,
with four material slots and no collision. Local Z0 is the wall-head datum;
placement in M12 is (0,20800,-500) cm with unit scale. Existing floor is Z=-1200.
The principal visual roof is approximately 22 m above that floor.

The Blender preview includes lower walls/floor and studio lights for context;
these are excluded from the exported FBX. Unreal uses the existing Radiance
stone, gold, dark trim and warm-inlay materials. The former 216-instance low
ceiling is hidden without deletion or movement. Original navigation geometry,
balconies, cover and survivor recesses remain authoritative. This is a reviewed
architectural proposal; wall-light balance, material scale, full-route combat
and packaged GPU cost still need acceptance.
