# Companion traversal chooser assignment

Both `BP_AurelionSeleneCompanion` and `BP_AurelionTarrikCompanion` had `TraversalTable=None`, while their corresponding player Blueprints assigned Narrative's `CHT_TraversalAnims_Biped`. The missing table causes `ANarrativeCharacter::TryAttachWarp` to return before probing the ledge or selecting a montage. The recent fresh-route log contained this warning, but did not identify the calling actor; the default inventory establishes the companion omissions independently.

Assigned the existing biped traversal chooser to both companion Blueprints. Its context is `ANarrativeCharacter` and its output is `UAnimMontage`, matching the native traversal evaluation. Each companion's definition resolves the same base skeleton and animation class as its corresponding player: Selene uses Quinn/Narrative skeleton; Tarrik uses Manny/Mannequin skeleton; both use Narrative ABP_Biped. No GASP sandbox controller, movement mode, new animation graph or C++ was substituted.

## Verification

- Read-only inventory: `Saved/Validation/Aurelion/CompanionTraversalReadback-20260920-210716-bffe2aa0`. Both players assigned, both companions missing. The earlier `CompanionTraversalInspect-20260920-210558-408c1ea0` stopped on an incorrect Python mesh accessor and is not accepted inventory evidence.
- Authoring: `CompanionTraversalSave-20260920-210935-ad8fb0e1` under the same parent directory. Each original Blueprint is backed up there. Both Blueprints compiled and saved; corresponding appearance skeleton/AnimBP equality was checked before any mutation. Mission hashes were preserved.
- Fresh editor reload: `CompanionTraversalVerified-20260920-211058-a6fcffde`. All four defaults resolve the same chooser. Before/after exported companion CDO text differs only in `TraversalTable=None` becoming the chooser reference; no other exported companion property changed.
- Baseline full build/test gate: `Saved/Validation/20260920-210204-36466a31`, 722 tests. Final gate `Saved/Validation/20260920-211240-dfbb6387` passed the full build, all 722 matching automation tests, coverage and source integrity.

This is a verified content assignment repair. It does not prove successful live vault/climb selection, foot placement, motion warping, clearance, companion catch-up or route completion. Those still require live follow/traversal qualification. It also does not resolve the separately documented restored weapon-grant issue or cinematic startup race.

Reproduce with `Scripts/Editor/inspect_companion_traversal.py` and `fix_companion_traversal_choosers.py`. The authoring script refuses to replace a chooser that is already assigned, saves only the two companion Blueprints, and preserves both mission map files.
