# Tarrik command attack admission

The previous checkpoint probe used one 210-second deadline for bootstrap plus
load. `TarrikCommandContact-20260920-101047-0e30a71b` received native SUCCESS
but never reached its ready contact stage before that shared bound expired.
The bootstrap now gives its existing bootstrap and load phases separate
210-second bounds and records pawn readiness/pending flags every two seconds.
It still requires the native ready state and never clears those flags itself.

`TarrikCommandReady-20260920-101857-3b359e20` restored the same earned E4A
checkpoint, used the authored retry and started the command-only observer.
It completed 366 samples without errors, attack montages or source-damage
receipts. Its first target distance was 3007.02 cm; the closest of 26 opening
allowance samples was 371.10 cm. No player attack was supplied.

An unsaved Blueprint preview changed only Tarrik's OpeningContributionSeconds
from 8 to 16. `TarrikOpeningPreview-20260920-102408-df1d909e` read back 16 on the
actual companion, recorded 256 samples and 51 opening samples, and ended without
errors or damage. The opening minimum distance was 131.69 cm, within the earlier
authored 180 cm light-attack range. Native melee descriptions still showed both
granted attacks inactive and no montage. Thus the opening timer alone does not
explain attack inactivity. The preview was not saved and is not a balance fix.

The observer now records native attack-candidate descriptors once per second,
including their range/activation/visibility checks and failure tags, alongside
the controller's direct-target admission. This queries the existing selection
API but never selects or activates an attack. It is diagnostic qualification,
not proof of ordinary companion combat or final visual quality.

`TarrikAttackAdmission-20260920-103134-a6ec6384` failed before observation:
the retry adapter timed out with only two Interact frames, while the focused
retry reported "Finish the current action first". The adapter had queried
admission on every held frame; the interaction's own busy state then caused
it to send a release. That made the test dependent on frame duration.
The adapter now latches an admitted hold, maintains input until native restore
begins, and retains a ten-second failure bound. It does not bypass admission,
complete the request directly, or clear the native interaction state.

`TarrikHeldRetryAdmission-20260920-103748-88895aa2` completed retry with five
movement, nine look and two Interact frames, then finished 770 contact samples
and 93 candidate checks without observer errors. It recorded no montage and no
companion damage. Eighteen candidate checks marked Tarrik melee available;
examples at 10.203, 13.421, 14.437 and 15.484 seconds had direct-target admission,
in-range, line-of-sight and CanActivate all true. This was the unsaved 16-second
preview, not the saved eight-second configuration.

The next investigation must inspect command activity ownership, curated-class
membership and companion-specific protection/contribution gates between an
available candidate and actual dispatch. Candidate availability alone does not
prove those additional gates should authorize an attack. This evidence does not
justify replacing animations, inflating reach or saving the timing experiment.
All isolated runs ended and exited; no companion Blueprint was saved.

A concrete next lead is saved grant provenance. The generation-18 checkpoint
was earned at 18:18 on 19 September, before
`CompanionPrimaryRepair-20260919-183329-94f445f0` replaced Tarrik's curated legacy
sword ability with `GA_Tarrik_MeleeLight`. `CaptureProxySnapshot` stores
CopiedGrants and `PrepareProxyFromSnapshot` rebuilds CuratedAbilities from the saved
snapshot. Command dispatch requires exact membership in that list. The loaded
companion's current list has not yet been recorded, so stale grants are a
supported hypothesis, not a verified root cause. Inspect that list before
changing dispatch, reach, timing or mission content.

Full gate `20260920-104320-f1997d15` passed the build without SkipBuild, all
720 tests, coverage and source integrity. Python compilation and whitespace
checks passed. No tracked edits occurred during the gate. The preceding full
baseline `20260920-101610-87372b25` also passed all 720 tests. M12's protected
hash remains `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
