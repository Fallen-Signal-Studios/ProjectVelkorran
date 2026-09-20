# Selene GASP ALS integration

Requested by the creator on 20 September: use the GASPALS feminine locomotion
library for Selene when integrating remaining character animation work. Preserve
the earlier equipped-Verity Twin Blade stance and attack requirements.

Status: inspected in Unreal; **not assigned to Selene, not gameplay qualified**.
No character, animation, plugin, or map asset was saved during this inspection.

## Verified integration boundary

The user-supplied content plugin mounts as `/GASPALS` and its assets load in UE
5.7. The feminine presentation consists of standing idle, standing movement, and
crouch poses selected by `ABP_OverlayBase_Feminine`, over the common GASP motion
matching databases. It is not a separate feminine motion matching database.

Its parent `ABP_OverlayBase_Base` expects `SandboxCharacter` and
`SandboxCharacterABP`; it implements GASP's `ALI_OverlayBase`. Its skeleton is
`/GASPALS/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin`.

Selene's `PD_Selene` currently resolves `Appearance_Selene`, whose base mesh is
Narrative's `SKM_Quinn` and whose animation class is Narrative `ABP_Biped`.
The character CDO mesh has no direct animation assignment because appearance
initialization owns that binding. Assigning an AnimBP only to the character CDO
is therefore insufficient.

Narrative `ABP_Biped` already has motion matching, a cached neutral base pose,
local/mesh-space locomotion deltas, linked weapon overlays, and FullBody,
UpperBody, and DefaultSlot montage paths. The GASP sandbox AnimBP is not a
drop-in replacement for these contracts.

## Remaining content work

1. Retarget feminine and corresponding neutral reference poses to Narrative's
   skeleton in project-owned assets. Check proportions, hand/foot placement,
   curves, and additive reference handling in the animation preview.
2. Adapt the feminine standing idle/moving/crouch selection into a Selene-owned
   copy of the existing Narrative animation graph. Keep the original neutral
   reference used to extract locomotion deltas; blindly replacing that reference
   would subtract the new posture instead of applying it. Preserve weapon
   layering, montage slots, root-motion handling, traversal and ragdoll paths.
3. Bind through a project-owned Selene appearance, including the companion's
   appearance path. Keep equipped-Verity Twin Blade presentation authoritative.
4. Verify in actual M12/M13 play: starts/stops, turns, strafing, crouching,
   weapon draw/stow, Verity attacks and ability casts, protagonist handoff,
   and companion follow/combat. Do not label asset loading as runtime acceptance.

Do not copy the plugin's sample map, game mode, camera, or collision defaults
into project configuration. Marketplace plugin content remains user supplied.

## Evidence

Read-only inspector: `Scripts/Editor/inspect_selene_gaspals.py`.
Latest isolated run:
`Saved/Validation/Aurelion/SeleneGASPCompatibility-20260920-061003-d36125c5`.
It contains asset/dependency inventory and Unreal text exports of the appearance,
Narrative base AnimBP, GASP sandbox AnimBP, and feminine overlay and defaults.
Process exited successfully. No runtime animation comparison was performed.

M12 retained SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
The existing 719-test build gate predates this read-only inspection; it does not
qualify this pending integration.
