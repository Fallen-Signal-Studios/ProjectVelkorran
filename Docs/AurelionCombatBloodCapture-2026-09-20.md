# Native combat blood capture attempt

Run: `Saved/Validation/Aurelion/FreshCombatBlood-20260920-203531-fd1c6baf`.
The isolated editor (PID 44484) exited; no process or retry remains running.

Fresh E1, E2, Meeting, rescue and E4 entry passed through the normal input driver. E4 entry retained all seven protected people alive. The observer then selected Staccato through the normal wheel and focused Tarrik on `BP_AurelionLinkbound_C_13`, with 53.2 health and no invulnerable tag. A separate living Elite was the ordinary player-shot target.

The 100-second contact observation produced zero player or companion damage receipts. The trigger was requested at 1.156 seconds, but this version did not record ammo consumption, so it cannot distinguish a missed shot from an attack input that did not fire. Tarrik had both native melee abilities and a wielded weapon. Across 1710 samples his distance to the Linkbound ranged from 68.09 to 2403.18 cm, with no active montage captured. At 70.64 seconds the companion was stationary about 931 cm from the target, outside the eight-second opening contribution window. This does not overturn the previous successful fresh 43.48-damage receipt, and does not qualify general companion reliability.

No qualifying receipt means no screenshot was requested. The wrapper correctly reports failed with `No native hit screenshot was captured`; no in-mission blood visibility claim is made. Both mission hashes were preserved.

`check_fresh_combat_blood.py` enables an optional receipt-driven capture in the existing fresh-contact wrapper. It keeps PIE alive until the screenshot task completes or its bounded wait expires and removes its extra delegate during cleanup. Existing contact probes retain their prior defaults. The companion observer now records player ammo and tags per sample and aim/input state at the trigger, so the next attempt can distinguish weapon discharge from an input request. These added fields were syntax checked but were added after this live run; they require a new live run for behavioral qualification.

Two additional content leads appeared during the route, without being diagnosed or changed here:

- `NS_WeaponFire_Tracer_Reformation` repeatedly reports a missing source emitter `ref` and failed Age/Position/Previous.Position reads. Inspect its actual reader binding and active emitter names before editing; this may affect enemy-fire readability.
- A traversal-table warning and gameplay-tag property-map owner warnings occurred during protagonist transitions. Their relationship to visible locomotion defects is not yet established.

Final full gate `Saved/Validation/20260920-204835-2dc512b0` passed the build, all 722 matching automation tests, coverage and source integrity. It does not convert the failed live capture into a visual pass. No production C++, gameplay tuning, asset or map changes were made.

## September 21 continuation

`CombatBloodReadability-20260921-003948-d9069dfd` exited normally after the
entry-only check passed. The launch omitted the runner's retained-entry and
route-continuation switches. Its outer `fresh-tarrik-focus.json` remained
`running`; combat was never observed, so neither exit zero nor the entry pass
qualifies companion contact or blood visibility.

The corrected launch is `CombatBloodRoute-20260921-004217-8e25b8c9`, with
`-Visible -KeepEntryOpen -ContinueE1 -ContinueRoute`. It uses the same unchanged
probe and stops the ordinary route at E4 entry before observing contact. This
entry records the launch only; results remain pending.

The corrected run completed. `fresh-tarrik-focus.json` reports a native Tarrik
Edge hit on `BP_AurelionLinkbound_C_13`: 43.478264 applied health damage, no
shield damage, nonfatal. The target was alive and lacked invulnerability when
selected. This is further live evidence that this companion can close, swing,
and deal damage in E4. The observer ended on the first companion hit at seven
seconds; it did not establish reliable repeated contact.

The player-shot probe had fired at 1.062 seconds while Selene carried
`Narrative.State.Weapon.BlockFiring`, `Equipping`, and `Reloading`; ammo remained
one across all 133 samples and no player damage receipt occurred. The observer
now waits for all three tags to clear before sending the ordinary attack input
and records the blocked tags. This is a diagnostic timing correction; it is not
a production firing fix and needs a fresh runtime pass.

`native-companion-hit.png` exists, but the rendered frame shows an empty bridge
instead of the struck enemy. The image therefore cannot establish black-blood
visibility, even though the underlying health-damage receipt is valid. Camera
framing at the receipt and capture timing remain to diagnose. Both map hashes
were unchanged, and the editor process ended.

`CombatBloodReadyFire-20260922-155038-11ec4140` exercised the corrected probe
through real E1, E2, meeting, rescue, and E4 entry. All four route segments
reported passed. Selene's Staccato fire waited until BlockFiring, Equipping and
Reloading cleared; the trigger frame had 1 round and a 0.1045-degree aim error.
Ammo fell to 0 and normal reload returned it to 4. No player damage receipt
followed that single shot. Tarrik's focus command played one actual sword
montage near 146 cm, but the Linkbound moved out of reach during windup and no
health damage resolved. The observer ended after 100 seconds with no companion
receipt and no screenshot. The wrapper's `failed` status is correct. This run
does not establish that Staccato's projectile or Tarrik's sustained attack is
broken; it narrows the next check to repeated valid shots, moving-target hit
registration, and companion approach after confirmed player contribution.

`CombatRepeatShots-20260922-162107-b26beba3` completed ordinary fresh E1,
E2, Meeting, E3 rescue, the E4 setup scenes and native E4A entry. In the active
encounter, Selene's first ready Staccato shot consumed its last round without a
receipt; after a normal reload, her next shot dealt 77.419357 nonfatal health
damage to the living Elite. Tarrik then closed on the commanded Linkbound,
played the active native 1H sword montage, and dealt 43.478264 Edge health
damage 2.58 seconds after Selene's hit. The native companion target stayed
alive. Both mission map hashes were unchanged. This qualifies one player shot
and one player-funded companion follow-up in the fresh route; it does not
qualify repeated autonomous pressure or every ability.

The generated `native-companion-hit.png` exists but shows an exterior editor
view with no combatants. It does not prove visible black blood. The observer's
new camera ray probe also attempted to unpack a `HitResult` as a tuple and
reported that diagnostic error, without affecting gameplay or the receipts.
The next diagnostic uses the live PIE `Shot showui` viewport path and handles
the Python `HitResult` return directly. A normally retried earned E4A replay
is the targeted validation; no screenshot is counted as a visual pass before
manual review.

`EarnedE4ARetryViewportCapture-20260922-163537-55ddeff7` copied the actual
E4A checkpoint banks from that fresh route into an isolated profile. The
encounter began in Failed after load and was activated by the authored Retry
encounter interaction through ordinary input. In the resulting active fight,
three recorded camera rays hit the living Elite's collision; Selene dealt a
nonfatal health hit, and Tarrik later dealt health damage to the Linkbound.
The new `Shot showui` capture contains the game viewport, Selene, the nearby
Linkbound, and the holographic HUD within editor chrome. It fixes the earlier
wrong-viewport evidence gap. The frame is too dark and the blood is not
visibly separable from the target and background, so black-blood readability
remains open. A close-up of the apparent top-left objective overlap showed
Unreal Editor's transient `Preparing Niagara System` compile message drawn
over the objective row, not a second gameplay HUD string. A separate earned
E4A layout probe (`E4AObjectiveLayoutInspection2-20260922-164232-2d341734`)
found one owned accessibility presentation, one visible objective TextBlock,
and a clean objective panel in the next game viewport frame. No native HUD
layout change is warranted by that editor-only capture. Both map hashes are
unchanged.

## September 22 native first-hit repair

The first active E4A blood gate trace found the actual suppression, not merely
a dark material: the Linkbound and Elite damage listeners both received valid
native health receipts, but their selected `NS_Aurelion_BloodBlackSlash` and
`NS_Aurelion_BloodBlackBurst` systems were loaded with `IsReadyToRun=false`.
`USovBloodFeedbackComponent` discarded both hits before spawning Niagara. The
earlier studio authoring check waited for Niagara compilation explicitly, so
it did not catch this PIE startup timing.

`SovBloodFeedbackComponent` now lets a valid Niagara system spawn even while
compilation is pending; the engine component already waits for readiness and
retries activation. In editor builds, the async asset-load callback requests
and finishes compilation before the encounter's first hit. The code still
rejects an invalid system and retains the existing live-damage, rate, reduced
effects, pool, and finite-lifetime gates. No map or Niagara asset was saved.

`E4ABloodAwaitActivation-20260922-170546-5160c492` showed custom black Burst
and Slash components created at real Elite and Linkbound hits after removing
the premature ready gate, though Slash still began inactive while compiling.
`E4ABloodPrewarmFrame-20260922-171120-1b709f03` then reported `ready=1`
for the selected black system at both first health hits. The final close run,
`E4ABlackSlashCloseCapture-20260922-172136-bdb95db9`, recorded a living
Linkbound struck by Tarrik, `NS_Aurelion_BloodBlackSlash ready=1`, and a live
PIE frame with a distinct black spray against the light floor behind the
target. The frame includes editor chrome and an unrelated incoming-damage
caption; it is evidence of the in-game effect rather than presentation art.
The earned checkpoint was activated through the normal Retry encounter input
in each run. Both mission maps remained unchanged. Red blood, other enemy
roles, reduced-effects quality and sustained multi-hit readability remain
separate visual coverage work.
