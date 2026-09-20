# Protagonist HUD runtime review

Update: the native magazine defect below is fixed and verified in
[HUD native refinement](AurelionHUDNativeRefinement-2026-09-20.md).
The original failing runs remain recorded below as historical evidence.

This increment changes validation scripts only. No gameplay source, widget asset,
character definition or map is changed. Selene's GASP posture assets remain
unbound; the source-scope decision is still pending.

## Fresh E1 attempt

`check_hud_protagonist_handoff.py` observes the normal existing entry/E1 input
driver. It does not inject possession, journal events, resources or HUD values.
The initial run `HUDProtagonistHandoff-20260920-070858-0e19a61b` stopped on a
verifier API error: ProgressBar exposes the `percent` property, not GetPercent.
It is not gameplay evidence. The corrected observer also waits for entry to
complete before reading the first HUD.

`HUDProtagonistHandoffRetry-20260920-071115-b3f8561d` passed the real entry check
and captured Tarrik with native health/shield 100/100 and ammunition 32/218.
The rendered image was inspected: gold identity/shield/radar, red health, and the
split ammo readout were visible. The actual embedded viewport was 1696 x 862;
this is not an immersive or ultrawide check.

The E1 continuation failed at 170.546 seconds after exhausting three bounded
native retries. Its final roster had drones 1–4 dead and drones 5–6 at 140 health.
No Selene handoff occurred in this run. This demonstrates a failed automated
route, not that a human cannot complete E1. Do not lower difficulty or change
the encounter to make this driver pass. Companion observers stopped without
errors; no companion damage event was observed during this pre-handoff segment.

## Earned Selene checkpoint review

`review_selene_checkpoint_hud.py` validates the earlier successful handoff report
from `SeleneShoulderHandoff-20260920-043213-8124ab93`, copies its two unchanged
checkpoint banks into a new isolated profile with SHA256 verification, and calls
the public save subsystem LoadSlot. It then checks restored, Verity and Staccato
states through the existing Narrative wield API. This is controlled weapon-state
coverage, not physical weapon-wheel input or a replacement E1 success claim.

`SeleneCheckpointHUD-20260920-071954-450ffcab` successfully restored Aurelion.CP2
with Selene and reproduced a failure: Verity displayed a firearm ammo panel.
The observer now retains each state and screenshot while accumulating this
presentation error, allowing the Staccato case to be checked independently.

Repeat `SeleneWeaponHUDCapture-20260920-072258-f2236067` completed all three
states and retained a **failed** presentation result. All three captures were
inspected. The actual viewport is 844 x 550 inside a 1280 x 720 editor capture.

| State | Actual wielded item | Ammo panel | Native / visible ammo |
|---|---|---|---|
| Restored CP2 | None | Hidden | Omitted |
| Verity | WI_Verity only | Incorrectly visible | 0 / 0 |
| Staccato | WI_Staccato only | Visible | 4 / 32 |

Selene's cyan/green theme, identity plate, health/shield fractions and weapon
readouts are present. This establishes the melee ammo defect visually and through
the native view. It does not qualify depleted health, physical input, all scales,
motion or complete mission behavior. The images also show a long stowed weapon
projecting across the lower-left view; inspect its attachment and scale before
claiming character-presentation completion.

## Native defect and proposed correction

`USovHolographicHUDWidget::ReadSnapshot` currently reads ammo whenever
`Pawn->GetWeapon(true)` returns any wielded weapon. Narrative's
`UWeaponItem::GetAmmoInClip_Implementation` returns zero when the weapon has no
ammo source. BuildView interprets every nonnegative number as a magazine, so an
ammo-free melee weapon can display `0 / 0` despite the source comment promising
that such weapons are omitted.

The native snapshot should populate ammo only when the weapon has a configured
RequiredAmmo class and a positive GetClipSize. Do not test whether an ammo item
currently exists: a completely empty firearm must still show its empty magazine.
Leave the snapshot's default -1 values for non-magazine weapons. Verify unarmed,
wielded Verity, loaded Staccato, exhausted Staccato and subsequent melee switching.
The existing automation named AmmoIsOmittedRatherThanShownAsZero tests only the
unarmed case, so its green result does not cover this defect.

This correction has not been applied. It falls under the pending native-source
decision required by the engineering handoff. Hiding the widget for one weapon
class would leave the shared snapshot and other non-magazine weapons incorrect.

## Validation

Baseline full gate: `20260920-070258-b90b6ed1`. Post-script gate:
`20260920-072427-da574b5c` passed build without SkipBuild, all 719 matching tests,
coverage and source integrity. This green suite does not override the failing
Verity runtime result above. Both new scripts compile with Python. M12 retains
SHA256 `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
All isolated review editors have finished; the creator's editor and worktree
edits remain untouched.
