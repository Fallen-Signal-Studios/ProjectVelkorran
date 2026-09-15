# Verity Twin Blade animation pass

User direction: replace Verity's attacks with Twin Blade pack animation and derive
Selene's equipped-Verity idle/locomotion stance from that pack.

## Authored content

- UE 5.7 IK retarget pipeline: pack Manny to Narrative Quinn, with automatically
  characterized rigs and the normal retarget operation stack. Seven project-owned
  sequences: idle, forward walk/run, and attacks 01–04.
- Four `FullBody` montages reuse the existing Narrative attack notify, motion
  warping, sound and trail classes. Each uses one native damage window matching
  the pack's authored slash/trail interval. Damage application stays native.
- `GA_SovVerityTwinAttack` copies the existing Verity attack ability and changes
  its ordinary combo animation set. `WI_Verity` grants this copy. Existing
  first-person, shield and dual-wield alternatives remain as previously authored.
- `ABP_SovVerityOverlay` now derives from a project copy of the melee overlay.
  Its third-person stance blends the retargeted idle, walk and run using the
  existing thread-safe direction/Speed2D bindings. Base locomotion retains
  directional footwork. This is an equipped-Verity stance, not a global Selene
  animation replacement.
- Editor-only authoring helpers modify explicitly supplied assets under
  `/Game/Characters/Animation/VerityTwinBlades/`, require stopped PIE, and do not
  save until the Python authoring script checks their results.

## Evidence and remaining checks

- Windows Development Editor build passed (28.23 s; subsequent stance helper
  build 22.35 s). Existing engine deprecation warnings remain.
- `Saved/Validation/Aurelion/VerityTwinRetarget-20260913-162443-652fac77`:
  seven clips and three rig assets created successfully.
- `VerityTwinAttackAuthoring-20260913-162825-4d95ddd8`: four montages and the
  ability/item binding saved, zero Blueprint compiler errors/warnings.
- `VerityTwinStanceAuthoring-20260913-163104-f20e4ac0`: stance and overlay saved,
  zero Blueprint compiler errors/warnings.
- The first reload found the stance edit in a generated graph copy. The helper
  now selects only the persistent `Overlay` graph. `VerityTwinPersistentStance-20260913-163638-e97d7a9c`
  saved the correction with zero compiler errors/warnings (helper rebuild 5.15 s).
- `VerityTwinFinalReview-20260913-163758-ed69a294`: fresh reload verifies all four
  montage bindings, the weapon ability, the overlay parent, and the persistent
  stance blend-space reference. `attack-01-preview.png` records montage 01 at
  0.37 s on the Narrative mannequin; playback and scrubbing showed the retargeted
  pose changing without an obvious skeleton collapse. This is limited editor
  preview evidence, not Selene gameplay acceptance.

All four attacks, stance transitions and live enemy damage must still be checked
on Selene with the actual weapon mesh. The
companion command-target correction remains separate; the previous fresh route
stopped at the E1 stair/drone obstruction before reaching companion qualification.
This content pass does not establish 90% TDD alignment or a fresh route pass.

## Companion allowlist repair, 2026-09-15

Live E3 observation exposed a missed reference migration: both mission profiles
still allowlisted the old Verity primary, so the native handoff snapshot omitted
the new Twin Blade attack. Both saved Selene profiles now reference
`GA_SovVerityTwinAttack`; their other fields are unchanged. The original authoring
recipe now migrates this reference and the reload review asserts it in M12 and
M13. Fresh saved-asset verification passed in
`VerityCompanionFresh-20260915-013633-fbd1b1ad`, with no Python errors.
See `AurelionCompanionCombat-2026-09-13.md` for the preserved live failure,
repair evidence and pending new handoff/damage run. This repairs configuration;
it does not by itself qualify all four attacks or companion damage.
