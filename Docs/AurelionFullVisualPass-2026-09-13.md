# Full environment and cast pass

The user's updated priority is visual fidelity across both mission maps, new
environment assets where the existing meshes miss the layout reference, and
appropriate supporting-cast meshes. Cast references are Tharne / Tom Holland,
Lyessa / Eva Green and Lyric / Florence Pugh. The complete 90% goal remains open.

## What holds the estimate at about 64%

The existing slice rubric is 63.75/100. Its largest unresolved areas are complete
mission progression, representative packaged performance, combat/AI acceptance,
HUD readability and finished presentation. Environment and story presentation
currently occupy only 10 points in that rubric (6.5 supported); better meshes
alone cannot honestly produce a 26.25-point increase to 90.

Visual work now takes priority, while the following acceptance gaps remain:

- Rooms retain flat modular enclosures, inconsistent lighting and generic
  detail relative to the layout concepts' monumental white/gold architecture.
  Every room needs player-height and encounter-context review, including
  transitions, balconies, recesses, Wound, Crownmark, briefing and departures.
- Lyessa and Lyric use the same `SK_Sci-Fi_Female_2` mesh with material variants.
  Tharne uses `SK_ScifiSoldierUE4_unmasked_`. None is accepted against the new
  cast references. Faces, hair, wardrobe, skeleton compatibility, idle/walk,
  dialogue and rescue/cinematic poses must be reviewed together.
- Uninterrupted E4B fails when the Elite pursues and kills Tharne inside the west
  recess. Partner positioning succeeds, but the Elite leaves usable Thermal
  range. Survivor protection, readable recovery, Thermal/Core victory and
  earned M13 travel remain unqualified.
- HUD alert/caption overlap, combat effect clarity, final audio/cinematic
  presentation, both priorities, checkpoint/reload reliability and representative
  packaged GPU/input/audio performance still lack complete acceptance evidence.

## Actual inventory

Read-only `audit_aurelion_environment_cast.py` loaded M12 and M13, then restored
M12 without saving either map. It recorded 1,543 relevant actor rows in M12 and
1,258 in M13, including hidden collision/authoring actors.

| Map | Distinct static meshes | Static-mesh instances |
|---|---:|---:|
| M12 | 75 | 10,328 |
| M13 | 36 | 3,748 |

There are 95 distinct meshes in the union. The adjacent
`AurelionEnvironmentMeshReview-2026-09-13.json` tracks the full set for review;
inventory is not visual acceptance. No missing material slots were observed.
Most potentially visible engine primitives are intentional gold channels,
corruption, discontinuity surfaces or effects. Do not blindly replace every
cube or disable gameplay collision to improve a count.

The local MetaHuman Character plugin, optional preset content and texture
synthesis model loaded successfully. Existing MHC_Sydney was inspected and closed
without editing; it has a rig and is already associated with Selene assets.
Reuse of a protagonist face is not the supporting-cast solution. A separate
Tharne working asset begins custom authoring; it is not a likeness-qualified or
gameplay-ready replacement. The Orlando preset was applied to the independent
Tharne work asset, its Medium Stubble Beard and Medium Curly Mustache were
removed, and its Short Swept Up hair whitening, ombre and highlights were disabled.
The saved head remains a starting preset: face sculpt, eye/skin refinement,
mission wardrobe, rig, dialogue and locomotion integration are pending. It is
not accepted as the Tom Holland reference. Lyessa and Lyric custom authoring
remains pending.

MetaHuman authoring logged optional groom root-UV mismatches and a missing Body
Hidden Face Map on `WI_DefaultGarment`. These warnings remain recorded in the
editor log; this working character has not been qualified for runtime use.

## First implemented environment asset

The Blender recess panel is now imported as
`/Game/Aurelion/Environment/Blender/SM_Aurelion_RecessPanel_2m`, with four material
slots, 200 × 40 × 300 cm dimensions and one UCX convex hull. Unreal's
`get_simple_collision_count` excludes convex hulls; the corrected validation
checks zero primitive shapes and one convex hull. The Blender source remains
editable under `Art/Source/Aurelion`.

Ten visual panels clad the existing recess back walls: six across the 12 m west
wall and four across the 7 m east wall (the final east module is half-width).
They face inward at yaw 180, Y=22870, Z=-1200. The existing walls retain physical
collision; the cladding actors have collision disabled. Two local rect fills
use 350 lumens each, 1400 cm attenuation and a 600 × 50 cm source. An initial
1800-lumen preview was rejected as too bright. Both sides were visually reviewed.

`verify_aurelion_recess_panels.py` passed all ten placements and two lights and
confirmed unchanged bounds/positions for 811 existing zone collision actors.
The verifier also checks each panel's X position and full scale, and the light
units and attenuation. The placement script restores transforms when re-run.
The saved map is backed up in `Saved/Validation/Aurelion/RecessPanelPlacement-20260913`.
That directory contains raw before/after editor captures. The per-run import,
placement and inventory reports are under `CrucibleLightingValidated-20260913-101809-9be0c273`.

This is a completed first cladding pass, not the full environment pass or a fix
for the survivor-protection failure. Cast-in-place, both-priority gameplay,
packaged rendering and dynamic-light performance remain to be verified.

## Player-height entry review and relay fill

Raw views under `Saved/Validation/Aurelion/RoomReview-20260913` cover M12 Z00,
Z01, Z02, Z03, Z04, Z06, Z07, Z08 and Z09. These are stopped-editor views from
authored entry marks at approximately 165 cm above the floor, not gameplay
traversal or per-mesh acceptance. They expose the following priorities:

- Z00/Z02: broad, very bright upper surfaces; refine material scale and exposure
  transitions while retaining clear entrances and the exterior vista.
- Z01: readable axial route, raised side circulation and repeated structural
  rhythm. Preserve the layout; review the heavy metallic/gold finish in context.
- Z03/Z06: low dark ceilings, generic repeated wall/column forms and oversized
  instructional signs need architectural and signage work.
- Z04: dark cover silhouettes and walls lacked separation. Four broad downward
  rect fills now reveal the walls and balcony without the rejected upward
  ceiling hotspots. Existing cover, terminals and navigation geometry remain.
- Z07/Z09: route lines read clearly; floor reflection noise and repeated wall
  surfaces dominate. The Wound itself requires a separate closer review.
- Z08: the pale ceiling is visible, but remains a flat slab with bright edge
  pools and oversized signs. It still misses the reference's tall structural
  rhythm; the new recess cladding does not resolve that larger mismatch.

The saved relay change uses `M_Radiance_IvoryStone` on
`Aurelion_Art_M12_Z04_86_7c5bc5` and four ceiling-mounted rect fills at
X=5000/9000, Y=-11900/-10100, Z=600, pitch=-90, 1200 lumens each,
2300 cm radius and 1200 x 900 cm source. The historical actor labels retain
`ENVL_Z04_CeilingBounce_1..4`, although the selected lighting points downward.
`verify_relay_lighting.py` checks the actual parameters. The preview comparison
confirmed unchanged transforms and collision settings for 1714 existing actors
and their primitive components. Runtime and packaged GPU cost are not yet tested.

`review_aurelion_zone.py` moves only the stopped editor camera and clears editor
selection. It never starts PIE, advances objectives or changes gameplay actors.
M12 contains entry marks for later rooms whose production art is in M13; a view
from those marks in M12 is not evidence that M13 content is missing.
The camera helper now limits each map to its own authored-room entry views.

M13 Z11/Z12 entry views were subsequently captured in the actual M13 map, with
editor camera/sequence overlays visible. The briefing table and chairs are
present, but need arrangement and cast/cinematic review against the six-seat
brief. Departure's entry view reads as an empty floor facing a blank wall; the
ship anchors sit to either side outside this initial view. The inventory still
identifies the Dominion/Reformation hull and wing placeholders as basic cubes.
Ship art and departure staging therefore remain major unfinished visual items;
the entry view alone does not qualify their visibility or cinematic behavior.
Neither M13 nor its cinematics was modified by this review.

After reloading the saved M12 map from M13, both relay parameter verification
and the expanded recess placement/collision-baseline verification passed again.
The reload's Map Check reported zero errors and zero warnings; this is separate
from the known startup/navigation/optional MetaHuman warnings recorded earlier.

## Departure shuttle asset pass

Origin/main was fetched again and remains at `9bfb44e1`. Two newly authored
Blender shuttle exteriors now replace the box-shaped visual dressing on M13's
departure docks. The Dominion model has a broader armored oxblood/bronze cabin;
Reformation has a slimmer navy/slate cabin and technical outriggers, following
layout-plan page 20. These are first-pass static exteriors, not accepted final
hero assets or animated ships.

Dominion: 5,124 triangles, 16.60 x 14.0275 x 4.925 m. Reformation: 5,532 triangles,
15.3887 x 13.60 x 4.625 m. Both have five resolved Unreal material slots, UVs,
bottom pivots, no gameplay collision and unit actor scale. They sit at existing
X=-3800/+3800, Y=47500 dock anchors, Z=0, yaw=180. Bounds fit the 28 x 18 m docks
and remain below the old 550 cm upper envelope.

The original hidden cube hull/wing actors were already non-rendering. The actual
box-shaped dressing was all 112 instances of `HierarchicalInstancedStaticMesh`
on `Aurelion_Art_M13_Z12_5_9dba0c`. Only that component is now hidden. Its separate
60-instance stone component stays visible. No original actor or instance was
deleted or moved; all 1,300 original actor transforms, component collision and
instance transforms were compared. Three UDS non-colliding editor billboards
follow the camera or sky reconstruction and are excluded only from transform
equality. Their component type and collision settings are still checked.

Two movable rect fills at X=-3800/+3800, Y=46400, Z=800, pitch=-25/yaw=90 use
4,500 lumens, 2400 cm radius and 1600 x 1200 cm sources. The first overhead
position left undersides too dark; the forward fill improves nose separation.
Rear surfaces are still dark. Each ship was reviewed in the stopped editor from
its side approach with Game View on. The central departure composition, boarding
and flight/cinematic behavior, earned M13 route and packaged GPU cost remain
unqualified. No gameplay alignment points are awarded by this asset pass.

Evidence: `Saved/Validation/Aurelion/Shuttles-20260913/` contains the map backup,
original component visibility, all 112 instance transforms, import dimensions,
physical baseline, placement report, verification report and the two actual
Unreal editor screenshots. `placement.json` records the historical unsaved
preview stage; the subsequent save/reload check is recorded separately.

The map was saved, unloaded via M12 and reloaded from disk. The read-only shuttle
verifier passed at 18:45:14 UTC; reload Map Check reported zero errors and zero
warnings. Both ships, all material slots, lights, docking envelopes, original
instance transforms and original collision settings passed persistence checks.

## Crucible upper-volume pass

The previous visual ceiling sat seven metres above the floor of the 70 x 48 m
Crucible. Its 216 floor-panel instances on `Aurelion_Art_M12_Z08_89_b5e7cb` are
now hidden, while their transforms and collision settings remain intact. A new
Blender-authored upper enclosure adds five angular white-stone portal ribs,
gold inlays, inset wall bays and roof coffers. The main roof reads approximately
22 m above the floor. The new mesh starts at the old wall head, leaving existing
floor-level circulation, balconies, cover and survivor recesses unchanged.

`SM_Aurelion_CrucibleVault` is 25,488 triangles, 71.4 x 49.4 x 15.4 m and has four
resolved material slots and no collision. It is placed at (0,20800,-500) cm with
unit scale and no rotation. Four movable rect wall washes at X=+/-2300,
Y=19600/22000, Z=300, pitch=18 and outward yaw use 2,500 lumens each, 3000 cm
radius and 1400 x 800 cm sources. The initial 9,000-lumen washes were reduced
after excessive brightness in the editor review. Existing lights were retained.

The oversized floating `Aurelion_Art_Sign_Z08_1cd400` component is hidden. A new
`WOUND GALLERY` destination label sits above the existing north exit at
(0,23160,-590), yaw=-90, world size 45, with dark lettering. The closed quarantine
barrier remains in place; the art pass does not open it or advance progression.

Entry, balcony-height and north-exit views were inspected in the stopped editor.
The taller enclosure is a substantial composition change, but is not complete
reference fidelity: bright wall pools, large-scale stone texture, further
structural detail, localized corruption and cinematic/combat framing remain.
No new gameplay or performance acceptance is claimed. E4B's survivor/Elite
failure remains unresolved and this architecture does not repair it.

`verify_crucible_vault.py` compares all 1,718 original actor transforms,
component collision and static-mesh instance transforms against the pre-change
snapshot. Camera/sky-controlled non-colliding UDS billboards have the same
explicit transform exclusions as the shuttle check. Evidence and the original
map backup are in `Saved/Validation/Aurelion/CrucibleVault-20260913`.

Save/reload result: the vault, all four lights, old-ceiling visibility and exit
sign passed persistence checks; Map Check reported zero errors and warnings.
The wider world-instance check failed for six `BP_AsteroidField_Globular`
actors: their four components each retain 84 instances, but all 2,016 instance
transforms change on reload. These are large changes, not rounding. No vault
script edits those fields. All other checked instance transforms remained
stable. The subsequent complete component comparison also found regenerated
non-colliding `BP_Star_C_1` render components. `verified.json` explicitly
separates `vault_status=PASS` from
`world_instance_invariance=FAIL_PROCEDURAL_ENVIRONMENT_DRIFT`. This is an open
environment determinism issue, not waived acceptance. The diagnostic script
records counts, samples and maximum differences in `reload-instance-differences.json`.
