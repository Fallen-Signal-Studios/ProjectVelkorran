# Companion melee contact investigation

Selene's native light-attack AI range is now 100 cm instead of the inherited
180 cm. In the same earned E4B checkpoint, her observed attack stand-off fell
from 138.11 cm to 84.60 cm. The authoring recipe preserves this range. This is
a verified approach improvement, **not a verified damage fix**: both contact
observations recorded no player or companion damage events.

Only `GA_Selene_MeleeLight` changed in content. Its attack graph, damage,
montages and player inputs are unchanged. Tarrik's range remains unmodified.

## Evidence and limits

- Before: `CompanionContactProbe-20260919-185728-ea3e5b81/ContactProbe-190822-103196`.
  The public checkpoint load restored generation 20, `M12_E4_QuarantineCrucibleB`,
  successfully. An ordinary focus command selected the hostile Weaver. Selene
  approached and played `AM_VerityTwin_01` at 138.11 cm. The primary blade edge
  remained clear of the target in the recorded samples. This observation only
  recorded the primary edge; secondary-edge estimates were derived from its
  known collinear geometry, not directly sampled.
- After: `SeleneReachContact-20260919-191632-8c1dafb7/ContactProbe-191716-580202`.
  A fresh editor loaded the same unmodified checkpoint banks. The normal focus
  command and weapon-wheel/trigger probe ran without Python errors. Selene
  played the same opening montage at 84.60 cm. Both blade edges were sampled.
  The bounded 100-second observation ended with no damage receipts.
- Neither observation qualifies encounter completion, post-load stability, or
  the full M12–M13 experience. The player remained stationary and the encounter
  subsequently failed. A supplied ordinary trigger did not yield player damage;
  no damage, resources, journal entries or actor transforms were manufactured.
- Earlier failed setup attempts are retained separately, including an invalid
  target-selection attempt that selected the companion. Those are excluded
  from combat conclusions. Selection now requires native hostile attitude.

## Faster reproduction

`start_earned_companion_contact_probe.py` copies the two genuine earned
checkpoint banks into the runner's isolated save profile, backing up existing
destination banks and verifying SHA256. The original profile is untouched.
Override `SOV_EARNED_COMPANION_SAVE_DIRECTORY` for another compatible earned
checkpoint. Native `load_slot` and its success callback are authoritative;
`list_slots` caches summaries and does not refresh after an external copy.

Use a fresh dedicated editor process. Repeated PIE sessions exposed retained
local-player objects to the existing input-owner lookup. The observer now
releases its world/input references when stopping. Several attempted alternative
Python accessors were unavailable; none remain in the implementation.

## Remaining work

No additional range or contact-window edits are justified yet. Compare the
companion's actual animation/weapon attachment and root-motion behavior with
the player version: the earlier protagonist test
`NativeMeleePIE-20260917-102244-f44ca6ba` recorded Selene light damage at 140 cm
against an Enforcer. That fixture teleported its target and does not establish
companion behavior. Investigate the difference before widening sweep radii,
changing attack timing, or claiming the no-damage report resolved.

The creator's M12 map retained SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
Baseline full gate: `20260919-185242-7bc8b752` (719 matching tests).
Post-change full gate `20260919-192204-16ecc078` passed: build without SkipBuild,
719 matching automation tests, report coverage and source integrity. No tracked
edits occurred during that gate. Python syntax and whitespace checks also passed.
