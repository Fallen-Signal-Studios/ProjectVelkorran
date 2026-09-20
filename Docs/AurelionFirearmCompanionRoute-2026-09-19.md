# Firearm and companion campaign verification — 19 September 2026

Run: `FirearmCompanionRoute-20260919-223152-de264556`, fresh isolated M12 profile,
UE 5.7.4 with Aura disabled, saved content at `daf38c8f` plus the preserved
creator worktree. The previous goal turn made implementation progress: it saved
and committed the owned firearm hierarchy and passed its focused runtime check.
This run extends qualification into actual campaign possession and combat.

## Verified in this run

- Normal input-driven entry, E1, Selene E2, meeting, rescue, E4 entry, E4A and
  E4B passed their native receipts. E1 needed two native retries; this is not a
  death-free run. E4B ended in native victory with all protected people alive.
- After each natural firearm equip, Tarrik and Selene's two linked owned rifle
  instances followed Far/FirstPerson/Balanced as false/true/false. A controlled
  higher-priority Aim request selected third person; releasing it restored first
  person. Each check restored the original preference synchronously. This is
  Blueprint/API boundary coverage, not physical camera-input qualification.
- The live HUD switched from Tarrik gold/red to Selene cyan/green across the
  handoff. On-screen review of Meeting and DropBlackBars showed readable complete
  captions without the removed decorative overlay clipping.
- Selene drew Verity, played `AM_VerityTwin_01`, and dealt three native
  source-attributed hits totaling 54.742689 health damage. The first hit killed
  a Linkbound for 26.200001 health damage (elapsed 632.329 s).
- Tarrik drew Velkorran and played `AM_Sword_3P_1H_Attack_1_Tarrik`. Nine sampled
  attack frames followed a Linkbound from 110.18 to 188.87 cm. No Tarrik damage
  receipt occurred in those samples. Draw and montage activation work; reliable
  contact remains unqualified. Distance movement alone does not prove a defect
  or justify increasing attack range or damage.
- Native M12 completion and CP6 travel preserved journal, logical identities,
  inventory, magazines and resources. All thirteen M13 receipts then passed
  through normal controls, including independent assents, paired physical lift,
  full scenes, CP7/8/9 and separate departures; the 22 M12 receipts were unchanged.
- The real final CP9 checkpoint reloaded through the public save owner into a new
  M13 world. One native success callback confirmed all 35 raw journal rows and
  evidence, identities, inventory/resources, completed lift and separate exits;
  restored departure state remained stable for 4.125 seconds.
  Evidence: `CP9Reload/checkpoint-reload.json` within the run directory.

## Limits and next work

The passive weapon-cache probe cannot read the native collision cache fields
through Python; those inaccessible fields are recorded, not treated as empty
caches. No traces, hits or damage were manufactured. This run does not prove
AAA animation quality, physical keyboard/gamepad behavior, or the remaining
architecture/destruction/content scope. The successful lift ride had separated
riders and does not resolve the previously reproduced capsule-overlap clearance
failure. Debug signage, graybox areas and dark/unfinished M13 spaces remain.
The goal remains active.

Baseline full build/automation gate: `20260919-222729-f5af0b47` (719 tests).
Post-increment gate `20260919-225515-17c21d1a` passed the build invocation without
SkipBuild, all 719 matching tests, report coverage and source integrity. No
tracked files changed during that gate. Creator-owned map/grenade work remains
excluded; the M12 map retains its protected SHA256.
