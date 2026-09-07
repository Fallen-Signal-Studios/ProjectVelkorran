# E2 relay receiver proof

`RelayOverlook` / `M12_E2_RelayOverlook` now requires both native encounter victory and two distinct physical receivers:

| Native receiver actor | Required ReceiverId |
| --- | --- |
| `ASovCampaignRelayReceiver` | `M12_E2_ReceiverWest` |
| `ASovCampaignRelayReceiver` | `M12_E2_ReceiverEast` |

Bind each receiver's `EncounterObjective` to the E2 `ASovCampaignEncounterObjective`; add both actors to that objective's `RequiredReceivers` array. The beat's exact `RequiredReceiverIds` contract is authoritative. Missing, duplicate, foreign-world, uninitialized, replaced or renamed actors cannot satisfy it. Actor configuration and identity are checked at entry and again before the final receipt commits.

Both receivers need an ordinary accessible floor approach. The sniper overlook does not authorize a receiver that can only be operated from beyond interaction range. Defaults are a 250 cm interaction distance and a 0.5-second Narrative hold. Native admission caps configurable interaction range at 300 cm and checks current protagonist, encounter attempt, ASC readiness/ownership and an unobstructed visibility trace. A native text label and Narrative action prompt show disabled status. Custom primitive data index 0 changes from 0 to 1 for an authored optional material effect.

This implementation supplies close interaction only. It does not claim a projectile/damage disabling path. Verify both approaches with physical controls and the intended NPC/content placement on the work PC.

## Progression and save behavior

- Either order works: disable both receivers then defeat the formation, or defeat the formation and then disable the receivers.
- Killing every required enemy alone leaves the campaign beat unfinished. The director's existing save fence remains held while any receiver receipt is missing. Continue to the reachable receiver; restarting the encounter is unnecessary for this normal pending state.
- A completed interaction reserves a native request, lets normal Narrative interaction callbacks finish, then verifies current range, visibility, actor identity and attempt ownership again before accepting the receiver receipt. There is no public `SetDisabled` function or Blueprint writable proof flag.
- The campaign journal stores the exact disabled receiver IDs with the encounter ID and attempt ID. Generic `CompleteBeat`, a repeated interaction on one receiver or a generic terminal cannot manufacture the other receiver's receipt.
- Partial receiver progress is scoped to the current encounter attempt. Failure, retry, campaign restoration and retired readiness/ownership do not transfer its receipt into a new attempt. Normal saving is already unavailable during active combat or an uncommitted victory.
- Once the native encounter fact commits, both actors derive their disabled states from the serialized campaign journal. Native presentation updates after journal restoration; no parallel save file or independent checkpoint owner is introduced.
- If an actor disappears or an ownership callback retires the attempt, the pending victory cannot be committed or saved. Reload the verified encounter entry checkpoint and correct any missing receiver authoring.

## Verification

Four native test registrations cover both operation orders through Narrative input; duplicate/missing actor, distance and wall rejection; readiness and retry retirement; and receipt serialization, disabled-state restoration and rejection of a missing saved receiver ID. The tests are in `SovRelayReceiverRuntimeTests.cpp` under `ProjectVelkorran.Campaign.RelayReceiver`.

These native tests have not run in this environment. UE 5.7 Editor compilation, execution and physical reachability/visibility validation remain work-PC gates. There is no claim that the E2 level, mesh, material or navigation content has been authored from here.
