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
