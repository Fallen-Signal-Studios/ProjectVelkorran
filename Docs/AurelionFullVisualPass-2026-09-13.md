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
