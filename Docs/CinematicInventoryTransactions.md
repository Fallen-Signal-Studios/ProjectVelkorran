# Native cinematic inventory and equipment transactions

Source follow-up for TDD v2 §14.13. `USovCampaignCinematicComponent`, `UNarrativeInventoryComponent`, `UEquipmentComponent` and Narrative item instances remain the owners. There is no cinematic inventory subsystem, duplicate currency ledger, or arbitrary Blueprint rollback system. Unreal 5.7/UHT/UBT and runtime automation have **not** run in this environment.

## Authored finite contract

`InventoryPostconditions` supports at most 32 named Grant/Remove operations, each bound to a declared cinematic participant. Each operation selects an **exact item class** and a positive quantity no greater than its maximum stack size or 100,000. Removal can name an existing `ItemGUID`; without a GUID, that exact class must resolve to exactly one owned stack. It never selects the first matching subclass or spans ambiguous stacks. Multiple operations may not target the same existing instance. Multiple grants use distinct IDs and distinct objects.

The item's class defaults must explicitly approve `bAllowCampaignCinematicGrant`, `bAllowCampaignCinematicRemoval` and/or `bAllowCampaignCinematicEquipment`. All three default false. This is an authoring permission boundary, not a promise to reverse arbitrary custom item callbacks. Active or busy items are rejected. Approved native weapon resource, finite-ammo, recharge and exertion behavior is not removed or replaced.

`EquipmentPostconditions` supports at most 16 unique participant/slot pairs. It declares the expected previous exact class and optional GUID, plus the replacement exact class and optional GUID. An empty previous class requires an empty slot; an empty replacement explicitly clears it. A replacement must be an unambiguously owned, unequipped instance or the exact new object identified by this participant's `ReplacementGrantId`. It cannot also be removed or used in another equipment change. Both displaced and replacement item classes must approve equipment changes, and the replacement must support the exact authored slot.

Equipment-changing participants must declare Holster or DrawRequiredWeapon, not Keep. `RequiredWeapon` remains the entry precondition. A matching explicit slot replacement determines the exit weapon identity; DrawRequiredWeapon can therefore draw a newly granted weapon when the entry was unarmed. Final draw requires a valid authored hand attachment. Removing equipped gear requires an explicit equipment clear/replacement for that same item.

## Preparation and application

After streaming and participant readiness, preparation creates strong-reference journals and validates projected capacity/weight for the complete batch. New grants are detached objects until commit. They always require a dedicated stack; they never merge into a player's pre-existing stack. No item quantity, equipment slot, resource, or receipt changes during preparation.

Both natural completion and an authorized skip use the same path, after Sequencer stops and restores its evaluated tracks:

1. Holster affected participants through the existing native wield setter and finish bounded native presentation handoffs.
2. Clear explicitly displaced equipment slots.
3. Remove exact quantities/objects, then insert complete dedicated grants. Remove-before-grant permits a full-capacity replacement.
4. Equip the exact selected replacement instances and complete their already-preloaded visual creation.
5. Apply final participant transforms and wield state, followed by typed transit postconditions.
6. Revalidate every final inventory/equipment, participant and transit postcondition. The existing campaign owner consumes one request/playback-generation receipt and records the beat. It checks the final postconditions again at receipt consumption.

Duplicate apply, duplicate receipt commit, and rollback of a committed transaction are rejected. Existing first-view proof, skip permission, prerequisites, journal reconstruction and post-beat checkpoint scheduling remain in force. Inventory journals do not manufacture viewing credit. The receipt regression stages only the already-covered playback-proof boundary, then invokes the production native-postcondition commit and real campaign journal.

## Identity and resource coherence

All writes occur inside Narrative's existing inventory, fast-array replication and GUID map. Full removal detaches the actual object and retains it strongly in the journal; it does not destroy and later recreate class defaults. Rollback reinserts that same object/GUID, preserving clip ammunition, last-use/recharge timestamp, fragments, attachment references and other retained item fields. Existing-object restoration re-enables only base inventory ticking; it does not replay subclass initialization that could rewrite ammo references before other removed objects have been restored. Grant initialization uses the existing restore-mode add hook, suppressing auto-use, auto-equip, refill/reload and NPC activity grants. A cinematic weapon grant is not an implicit ammunition refill.

Normal and cinematic removal now retire membership and GUID lookup before removal callbacks. Foreign-inventory consume/remove attempts are rejected. A private native removal primitive prevents a second overridable permission check from changing an item between final journal validation and membership mutation. Inventory load clears the GUID map and advances a transient load epoch, invalidating all previous journals.

Quantity, general item state, item equipment state, equipment-slot and character wield writes have transient revision tokens. Tokens publish before callbacks; same-value quantity/equip/wield writes and busy/active ownership writes invalidate prior ownership. Native resource setters already mark items dirty, so later clip/recharge writes prevent deletion of a granted item. These tokens are not another saved inventory state.

## Failure, rollback and later owners

Rollback reverses only writes still owned by this journal. It clears the owned final wield, unequips the owned replacement, removes dedicated grants, restores removed original objects/quantities, restores the original equipment, and restores the original wield. Every callback boundary rechecks identity and native ownership. No full-inventory snapshot is assigned over current inventory.

| Interruption or conflict | Result |
| --- | --- |
| Cancel/timeout before application | No inventory mutation; existing scene input, tags and streaming leases are released. |
| Failure after some owned writes | Reverse only those owned writes; no beat receipt is committed. |
| Later same-value quantity/equipment/wield setter | Revision mismatch preserves the later owner. |
| Later clip, recharge or busy/active resource write on a grant | Grant is not deleted; the beat remains incomplete. |
| Later resource-only write on a retained partial-removal object | Restore owned quantity without resetting the later resource field. |
| Later inventory uses capacity freed by removal | Do not overfill or delete later items to restore the original; require checkpoint recovery. |
| Component unregistration, actor destruction, checkpoint load or newer playback | Stop mutation; restore only identities/epochs still safely owned, and release exact scene leases. |

If rollback cannot complete without overwriting a later owner, the failure explicitly requests the pre-scene checkpoint. Retry on that component is blocked until the affected inventories advance their load epochs (or cease to exist). This prevents repeated failed requests from duplicating preserved grants. The checkpoint remains the recovery boundary for arbitrary callback side effects or destruction. Direct field writes that bypass native setters are outside the revision contract.

Transform rollback now restores only a transform this request actually applied and that still equals its declared exit value. Wield restoration requires the exact setter revision and original owned weapon identities, avoiding a stale pointer being re-wielded after inventory replacement.

## Weapon presentation and initial setup

Previous/replacement weapon visual classes join the existing async preload request. Missing classes or missing current weapon visuals fail preparation. The existing character-visual owner can complete creation **only for an already-loaded class and currently equipped item**; it never synchronously loads a missing asset. Late/canceled callbacks cannot recreate a removed/replaced weapon, and destroying an old visual cannot remove a newer slot owner's map entry.

Final physical attachments must identify the current character, item, equipment slot and hand/holster. Verification reads the actual mesh parent, socket and relative offset after callbacks, including an available local player mesh, rather than trusting cached attachment flags. Sovereign transforming weapons complete the same finite Drawing/Deploying or Retracting/Stowing native handoffs used by their existing timer/notify implementation. This cinematic-only completion is fenced by request generation and transition serial, and checks the actual committed attachment. It does not change clip, recharge, abilities' resource costs, or the normal staged gameplay transition timings. Failed attachment or a callback's newer semantic request fails the scene.

An accepted opening scene remains Loading during real-viewport first-boot accessibility setup. The existing frontend helper presents settings; preloading may continue. Setup time is excluded from the monotonic preparation watchdog. Context loss, tick disabling and unregistration still cancel. Headless automation is unaffected, and no new mission/opening trigger is introduced.

## Validation and Monday acceptance

Executed the production inventory policy test under C++17 with `-Wall -Wextra -Werror -pedantic` and UBSan: **33,217 assertions passed**. It covers quantity extrema, dedicated-stack/weight boundaries, nonfinite input and exact revision ownership. The existing cinematic portable suite separately passed **41,878 assertions**. `git diff --check` passed; these checks do not imply UHT compilation.

Eleven new authored Unreal cases cover dedicated grants and duplicate application; quantity/capacity/approval boundaries; same-object GUID/clip/recharge restoration; later same-value quantity, busy and resource writes; mutating permission callbacks; partial quantities, foreign ownership and load-epoch invalidation; full-capacity equipment replacement/rollback; conflicting equip callbacks and retry blocking; actual full/skip campaign receipt commits; unregistration during a grant; and actual attachment parent/socket/offset validation after detach or reparent. These are **not executed** here.

Before accepting the feature in UE 5.7:

1. Build Editor/Game targets with UHT/UBT; run `ProjectVelkorran.Campaign.Cinematic`, campaign/save regressions and transforming-weapon regressions.
2. Author approved campaign item classes and actual scene manifests. Default-deny content approval, meshes, animation, sounds and scene authoring remain editor work.
3. Complete and skip equivalent scenes, including unarmed-to-granted weapon, existing weapon replacement at full capacity, equipped removal, stacked ammo removal, and no-op manifests. Compare inventory objects/GUIDs, resource fields, physical sockets, receipt and checkpoint.
4. Interrupt each application stage with component unregistration, missing/dead participant, save/load, full weight/capacity, a denied item predicate, same-value native rewrites and a newer playback. Ensure beat incompleteness, exact cleanup and checkpoint-required retry blocking.
5. Verify preloaded normal and transforming weapon presentations, failed sockets, removed ammo-source references, current/first-person attachment, 30/60 Hz handoff and post-save reconstruction in the real project. Physical asset readiness is not established by portable tests.
6. On a clean local profile, keep first-boot accessibility open longer than the loading timeout, then complete it. The accepted opening must start once, with no timeout or missed request; commandlet/headless automation must not wait for UI.
