# Selene GASP ALS integration

Requested by the creator on 20 September: use the GASPALS feminine locomotion
library for Selene when integrating remaining character animation work. Preserve
the earlier equipped-Verity Twin Blade stance and attack requirements.

Status: feminine/reference poses retargeted and visually reviewed in Unreal;
**not assigned to Selene, not gameplay qualified**. No character, plugin, or map
asset was saved. Ten project-owned pose/rig assets and five additive posture
assets have been created. Live graph wiring remains unfinished.

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

1. Retargeting and initial pose comparison are complete (details below). Confirm
   curve/additive behavior as part of the actual graph integration.
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

## Retarget and pose review

`retarget_selene_gaspals_poses.py` created seven one-frame sequences (three
feminine poses and four neutral references), source/target IK rigs, and an IK
retargeter under `/Game/Characters/Animation/SeleneGASPALS`. The output skeleton
is Narrative's skeleton used by Quinn and Selene. The clips remain non-additive;
no incompatible pose has been assigned to a live character.

Authoring: `SeleneFeminineRetarget-20260920-063040-611a9f8c`. The original vendor
poses remain unchanged. The engine initially generated the new clips under
`/Game`, then the script relocated them to the owned folder before saving.

`review_selene_gaspals_poses.py` renders the three source/target pairs in an
unsaved studio world. The first run (`SeleneFemininePoseReview-20260920-063417-7cc3b65d`)
showed A-poses: transient PlayAnimation was insufficient for this editor preview.
It is **not** retarget evidence. The corrected run
`SeleneFeminineEvaluated-20260920-063718-6356e3a9` uses OverrideAnimationData,
realtime viewport updates and explicit animation ticking. Its image was inspected:
idle weight shift, standing movement posture, and kneeling crouch are present on
both rigs without an obvious collapsed joint. Direct AnimPose evaluation also
confirms distinct poses; the retargeted crouch pelvis is at 41.73 cm versus about
94.6 cm standing. This is static mannequin evidence, not MetaHuman/gameplay proof.

The existing `CharacterAttributes.UnarmedAnimLayer` hook in
`NarrativeCharacterVisual.cpp` applies an overlay when weapons are stowed and
replaces it with the weapon overlay on wield. Evaluate this hook for the feminine
unarmed presentation before replacing the main animation Blueprint. It offers a
content path that can preserve equipped Verity. Any implementation must select
idle/moving/crouch correctly and preserve first-person behavior; assigning one
static pose to every state is not the intended result.

The editor's Python wrapper does not expose `function_graphs` or
`ubergraph_pages` on AnimBlueprint. Those failed reflection queries are recorded
in the review report. Existing native helpers are scoped to Verity or Eclipse
authoring and should not be repurposed through misleading asset locations.

Baseline full build/719-test gate: `20260920-062623-43147590`. Post-change full
gate `20260920-064038-145895ad` passed the build without SkipBuild, all 719 matching
automation tests, report coverage and source integrity. Python syntax and
whitespace checks passed. Protected M12 hash is unchanged. These checks do not
prove gameplay integration of the new unbound assets.

## Additive posture preparation

`author_selene_feminine_deltas.py` creates four local-space additive sequences:
standing idle/moving and crouching idle/moving. Each uses its matching retargeted
neutral pose as the reference. The two crouching deltas share the feminine crouch
target but use distinct idle/moving references. Control curves are removed from
these deltas, and root-motion extraction is disabled. The blend's axes are speed
(0–100 cm/s, clamped above that) and crouch amount (0–1). These are posture
corrections, not replacement walk cycles or an alternate motion-matching database.

The initial authoring run `SelenePostureDeltas-20260920-065558-b1fdd829` saved
incorrect null reference-pose assignments. The independent reload check
`SeleneDeltaEvaluation-20260920-065718-0082ec51` correctly failed. Unreal clears
RefPoseSeq while additive mode is AAT_None; the canonical authoring script now
sets additive mode and reference type before the reference asset, with readback.
The guarded repair `SeleneDeltaReferenceRepair-20260920-065936-c6a4bd5d` backs up
and corrects only these four newly created assets.

`SeleneBlendResample-20260920-070043-030d6e59` opens the owned Blend Space in its
asset editor to build runtime interpolation data, saves that asset, then verifies
the repaired poses. Assigning SampleData through Python alone does not resample
the blend. Its export contains two triangles covering all four samples. All four
additive reference reconstructions match their target across 361 bones within
0.1 cm / 0.1 degrees (measured maximum errors below 1e-12 cm / 1e-5 degrees).
`verify_selene_feminine_deltas.py` checks those references, reconstruction, removed
curves, sample types and serialized runtime triangles in a fresh editor session.

The existing editor-only authoring helpers are explicitly scoped to Verity and
Eclipse. Python can read persistent animation-node structs but cannot access graph
pins or protected graph node lists. A request for a narrowly scoped editor-only
Selene graph helper is pending under the repository handoff's source restriction.
No C++ changes, appearance binding, graph changes or runtime qualification have
been made in this increment. Do not count these assets as a playable integration.

Final fresh reload `SeleneDeltaVerifiedReload-20260920-070517-337cf44a` passed
all four 361-bone reconstructions and confirmed two persisted interpolation
triangles. The preceding final-reload attempt caught a verifier encoding issue
(Unreal exported this ASCII-only asset as UTF-8); the verifier now detects the
UTF-16 BOM and otherwise reads UTF-8. This was a checker fix, not an asset change.
Full post-asset gate `20260920-070258-b90b6ed1` passed the build, all 719 tests,
coverage and source integrity. Python compilation and whitespace checks passed.
M12 retains the protected hash recorded above; existing user edits remain intact.
