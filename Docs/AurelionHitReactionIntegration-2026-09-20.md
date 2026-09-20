# Eclipse hit reactions: prepared content and integration gap

Four project-owned recoil clips, montages, and Narrative animation sets are
prepared under `Content/Aurelion/Enemies/Animation`, with the `HitReaction`
suffix. They use each role's matching Parasites skeleton:

| Role | Pack sequence | Duration |
|---|---|---|
| Linkbound | Anim_Parazite_Get_Hit | 0.90 s |
| WallRunner | Anim_Spider_Get_hit | 0.73 s |
| Weaver | Anim_Alfa_Get_Hit | 1.30 s |
| Elite | Anim_Fat_Get_hit | 0.63 s |

These are **prepared, unbound assets**, not a completed hit-reaction feature.
The clips have root motion disabled and root locked; notifies are cleared. The
montages use DefaultSlot, a single nonlooping segment, and automatic blend out.
No map, live animation blueprint, ability, or damage binding was changed.

## Why the live binding is withheld

The existing Eclipse animation blueprints have attack sets but no flinch sets.
Their `OnDamagedBy` graph reports AI damage perception rather than playing a hit
reaction. The existing `GC_TakeDamage` cue's `CanFlinch` predicate checks only
`IsAlive && IsMovingOnGround`. Its default does not stop all montage groups, but
a new full-body montage in the same slot group can still interrupt an attack.

Native melee constructs `USovGameplayEffect_MeleeDamage` directly in
`SovGameplayAbility_Melee.cpp`; that effect does not publish the generic damage
cue. Adding flinch assets alone therefore neither solves melee reactions nor
establishes safe behavior during attacks, support casts, wall traversal, or death.

The next integration must consume resolved damage, respect guard/death and
attack/cast/traversal state, and distinguish a cosmetic recoil from a gameplay
stagger. It must preserve attack payload timing and wall-run orientation. Verify
idle/moving hits, active attacks and casts, wall traversal, protected targets,
guarded hits, lethal damage, and repeated impacts in actual runtime behavior.

The handoff forbids C++ edits. A scope question requesting targeted native fixes
was sent to the user; no such approval has been received at this writing.

## Authoring and evidence limits

`author_eclipse_hit_reaction_assets.py` refuses to overwrite existing assets.
`verify_eclipse_hit_reaction_assets.py` independently loads and checks the saved
clip, montage, skeleton, slot, root-motion policy, and animation-set reference.
Neither script tests a live damage trigger or proves visible animation quality.

The authoring run `EclipseHitReactionAssets-20260920-012327-829847d7` saved all 12
assets and its JSON before an engine animation-cache assertion during shutdown.
The authoring script now drains asset compilation between duplication,
configuration, and saving via the engine's registered
`Editor.AsyncAssetCompilationFinishAll` command. This revised authoring path has
not been rerun over the saved assets; it is not claimed as a proven engine fix.

Two readback attempts were invalid because the verifier initially required an
explicit DefaultSlot export and then tried a nonexposed Python property.
The corrected verifier recognizes the engine constructor's omitted default.

Fresh-load readback `EclipseHitReactionReadback-20260920-012931-8b22c7c8` passed
all four roles and the editor exited normally. Full gate
`20260920-013042-3b8f5e8f` passed the build, 719 automation tests, report coverage,
and source integrity without SkipBuild. This establishes asset integrity and
baseline regression status, not live reaction acceptance. M12's protected disk
hash is unchanged.
