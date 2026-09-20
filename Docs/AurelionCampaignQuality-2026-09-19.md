# Campaign quality pass — 19 September

Following the reference HUD pass (`63f34338`), work moves to campaign interactions,
companion combat and environment readability.

## Mission controls versus corpse loot

The previous E2 replay admitted the east receiver at 190.82 cm with an aim error
below .001 degrees, but focused a nearby Enforcer corpse for roughly 58 seconds.
E3 also showed a Linkbound corpse displacing the required mission prompt. Native
selection weights priority by four before facing and normalized distance.

`prioritize_aurelion_controls_over_loot.py` sets `NPCInteractable` priority to -2
on the seven owned Aurelion enemy Blueprints. It preserves admission, ranges,
loot contents and interaction callbacks. In-reach admitted mission controls at
priority 0 should win while ordinary loot remains available elsewhere.

Authoring run `CampaignLootPriority-20260919-180507-d4c97fbe` backed up and saved
all seven assets. Its final assertion rejected in-memory map dirtiness caused
by Blueprint reinstancing after those saves. No map was saved: the creator's
M12 SHA256 remains
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
The script now records unsaved map packages and checks the disk hash instead.
The original failed authoring report is preserved.

Fresh `CampaignQualityRoute-20260919-180911-b8ff8cfb` read back -2 on all 24 placed
enemies. The initial observer also enumerated ten story NPCs at priority 0;
these were false positives in its `unexpected` list. Its class filter now
explicitly lists the seven enemy classes. Story NPCs were never edited.
Actual receiver/loot focus and companion combat qualification remain in progress.

The fresh route subsequently passed entry, E1, E2, meeting, rescue and E4 entry.
E2 completed in 70.16 seconds. The west receiver advanced from alignment at
62.50 s to its hold at 62.69 s; the east advanced from 68.19 s to 68.27 s.
Both native receiver receipts committed without the previous long focus delay.
Ordinary corpse focus was still visible away from controls. This is one successful
runtime exercise, not an exhaustive interaction qualification.

The E4A driver stopped after requesting a precision shot while Null Pulse still
owned `Narrative.State.Busy` and `Sov.State.EchoAbility.Active`. Those tags were
present in the recorded trigger frame but absent from the driver's readiness
filter. The filter now waits for both to clear. The failed report is preserved;
the correction has not yet completed a fresh E4A route. No weapon, ability,
resource or mission state was changed to force progress.

The passive camera observer reported no native/interface mismatches. Companion
observation recorded 501 Selene and 1,004 Tarrik samples, including Verity and
Velkorran wielded states. No companion damage or attack montage was recorded.
Protected boss/carrier targeting and staged commands occurred in this run;
ordinary companion combat remains explicitly unqualified. PIE and all retained
combat observers were stopped cleanly after the route failure.

## Receiver console material

`fix_receiver_status_nanite_usage.py` backed up and saved only
`M_AurelionKit_ReceiverStatus`, changing `used_with_nanite` from false to true.
This removes the missing authored usage on the Nanite console material. The
creator's M12 map hash remains unchanged. Fresh material readback is pending.

Fresh `CampaignQualityReadback-20260919-182337-ac9c614c` passed all seven enemy
default checks and confirmed the saved receiver Nanite usage flag. Process exit
was 0. Python syntax checks and `git diff --check` also passed.

Baseline full gate: `20260919-180215-9cdb063e`, build without SkipBuild and 719
passing automation tests. Post-change gate remains pending.

Post-change full gate `20260919-182516-2dc7bde8` passed the build invocation
without SkipBuild, 719 matching automation tests, report coverage and source
integrity. No tracked edits occurred during validation. The creator's map and
grenade edits, and the separate camera WIP, remain outside this commit.
