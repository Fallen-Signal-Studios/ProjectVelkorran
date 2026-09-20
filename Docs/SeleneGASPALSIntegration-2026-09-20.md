# Selene GASP ALS integration

Requested by the creator on 20 September: use the GASPALS feminine locomotion
library for Selene when integrating remaining character animation work. Preserve
the earlier equipped-Verity Twin Blade stance and attack requirements.

Status: the retargeted feminine posture layer is integrated into Narrative
ABP_Biped and verified in controlled Selene PIE movement and weapon cases.
The supplied library supplies posture corrections over existing motion matching.
Full-route, companion combat and packaged acceptance remain outstanding.
The preparation notes below are historical; the completed integration and
subsequent transition checks are recorded in the final sections.

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

## Original integration plan (historical)

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

## Authorized native integration — implemented and checked in PIE

The user explicitly authorized C++ for Selene's animations. New source adds
`FAnimNode_SovSelenePosture`, an editor graph node, and the narrowly scoped
`ConfigureSeleneFemininePosture` authoring operation. The runtime node samples
character identity, movement, weapon and ability state on the game thread,
then evaluates the additive posture on the animation thread. It applies only
to Selene players/companions on the ground, unarmed and alive, outside cover,
equipping, sequencer and root-motion control. Blending takes approximately
0.167 seconds. Existing motion matching and weapon overlays remain authoritative.

The original ABP_Biped class must be preserved: weapon overlays cast to that
class, so replacing it with an unrelated duplicate would break their contract.
The helper inserts the posture between the existing lean output and PreLookAt
slot, before subsequent montage and foot-placement processing. It refuses an
unexpected graph edge, duplicate insertion, wrong asset or incompatible blend.

Build attempt `20260920-073526-b97cc7d6` compiled the new source but failed at
linking because creator editor PID 49944 holds the project DLLs open (LNK1104).
Automation did not run; this is not a passing build. The editor was left open
to preserve unsaved work. The user was asked to save and close it.
`bind_selene_feminine_posture.py` passed Python syntax checking but has NOT run.
No animation graph asset has been changed. At that point, the source was uncommitted pending
successful linking, Blueprint authoring and live validation.

Resume with the full build/automation gate after the editor closes, run the
scoped binding script in an isolated M13 editor, then verify actual Selene
idle/walk/crouch, stow/draw and Verity attacks using the authentic CP2 save.
Check Tarrik bypass and companion identity separately. Do not count this
source-only increment as completed gameplay integration.

### Completed after the editor closed

The user closed the creator editor, releasing the DLL lock. Build/test gate
`20260920-074049-f694392d` passed all 719 tests. Initial Blueprint authoring
reported an Editor-only graph-node module warning. The graph node now resides
in the dedicated `ProjectVelkorranAnimGraph` UncookedOnly module; runtime
implementation remains in ProjectVelkorran. The isolated backup was restored
before rebinding, preserving the original ABP_Biped identity and other graph edges.
Final binding `SelenePostureBindingFinal-20260920-074903-94b6ced5` compiled with
zero errors and zero warnings and saved only ABP_Biped. The first module build
needed an explicit BlueprintGraph dependency; the subsequent build passed.
This fixes the compiler classification warning; a packaged build is not claimed.

A PIE-only `PreviewSeleneFemininePosture` helper provides live blend diagnostics
and permits temporary A/B bypass on a live ABP_Biped instance. It does not save
or change the asset. Python cannot directly access the protected generated node
property. The first A/B test caught that access restriction, and a later test
caught a checker-only repeated UnCrouch/Crouch request in the same frame. The
canonical checker preserves crouch between adjacent crouch cases and uses a
fixed movement direction for its walk test.

Final run `SelenePostureVerified-20260920-080035-d8d1d482` restored the unchanged,
authentically earned CP2 save through the public save API. All eight cases passed:

| Case | Posture weight | Observation |
|---|---:|---|
| Unarmed idle | 1 | Feminine posture active |
| Idle bypass | 0 | Original posture comparison |
| Crouch | 1 | Crouch amount 1 |
| Crouch bypass | 0 | Same foot placement visible in comparison |
| Movement | 1 | Actual speed 210 cm/s |
| Verity drawn | 0 | AM_VerityTwin_01 observed after Narrative attack input |
| Staccato drawn | 0 | Firearm owns the stance |
| Re-stowed | 1 | Feminine posture returns |

Idle, bypass, crouch, crouch-bypass and movement screenshots were visually
inspected. The change is visible in the upper-body posture without a new collapsed
limb or changed crouch foot placement. The embedded viewport is small; this is
not full-screen cinematic quality approval. Existing stowed-weapon protrusion
and the Verity 0/0 ammunition HUD bug remain visible and are not fixed here.

Build/test gate `20260920-075540-7a5a5c6a` passed all 719 tests after the diagnostic
helper. Final post-checker gate `20260920-080421-63f9ba55` passed the build, all 719 tests, report coverage and source integrity.
Protected M12 SHA256 remains B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5.
No user ability assets, map, GASPALS plugin files, or save banks were overwritten.

Scope limits: this integrates the supplied GASPALS feminine posture library over
existing motion matching, not a separate female motion-matching database. The
companion identity branch exists but Selene-as-companion combat was not exercised
in this checkpoint. Airborne, traversal, full attack chains, damage delivery,
controller input, full-route acceptance and packaged execution remain outside
this narrow verification. This does not establish 90% TDD or AAA completion.

## Airborne and continuous reversal qualification

`review_selene_locomotion_transitions.py` restores the same earned CP2 through
the public save API, then exercises jump, landing, walking, a continuous reversal
and a stop. Jump uses Narrative's existing Jump input tag; walking uses the
character movement input API. No teleport, ability grants or asset writes occur.
This remains scripted input coverage, not physical keyboard/controller coverage.

Final run `SeleneContinuousReversal-20260920-121143-21a5b01a` exited successfully
and passed all six state assertions. Reversal began with 210 cm/s of forward
velocity and reached 210 cm/s in the opposite requested direction. The posture
weight reached zero while airborne, returned to one after landing, remained one
through grounded movement, and settled at zero speed after stopping. Jump,
reverse-movement and stopped screenshots were inspected at the embedded
844 x 550 viewport size. They show the expected poses; they do not resolve every
frame of the pivot or establish full-screen animation quality.

The first attempt failed on a Python-only lookup (`get_character_movement` is
not exposed); the corrected checker obtains CharacterMovementComponent by class.
Intermediate run `SeleneTransitionReadback-20260920-120112-86f972f6` passed jump
and landing, but paused between direction changes. It is not continuous-pivot
evidence. The final checker removes that pause and asserts positive forward
velocity immediately before requesting reverse input.

Stowed weapon geometry still crowds Selene's head and shoulder in these frames.
Read-only `SeleneHolsterInventory-20260920-120636-3b67fbd6` records project weapon
CDO attachment maps. Staccato has BackA/BackB offsets with Z translations of
28.73/22.69 cm, but live weapon/socket placement must be identified before
choosing an offset correction. No holster configuration was changed here.
The earlier Verity ammunition HUD defect was separately fixed in `606e4905`.

Full baseline gate `20260920-115622-4390b09b` and post-checker gate
`20260920-121358-b25465b3` both passed the build without SkipBuild, all 720 matching
automation tests, report coverage and source integrity. This increment changes
only validation scripts and this evidence document, not production animation
assets or native source.

Subsequent refinement: the saved Staccato BackB fit now carries the rifle more
upright along Selene's back. See [holster fit evidence](SeleneStaccatoHolsterFit-2026-09-20.md)
for the scoped asset change and fresh movement/draw/stow validation.
