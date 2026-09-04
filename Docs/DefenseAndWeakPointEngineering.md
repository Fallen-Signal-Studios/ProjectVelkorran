# Defense, weak-point consequences, and permanent sever state

The existing Narrative damage execution and AttributeSet remain the single damage resolver. A zero-body attack can now deliver explicit Poise damage or an authored status request without inventing Shield/Health loss. The same transaction gate checks friendly fire, channel immunity, and invulnerability for execution-based attacks and direct meta effects. Fatal policy retains its existing exceptions.

## Damage authoring contract

Use an instant effect with `UNarrativeDamageExecCalc`, `SetByCaller.Damage = 0`, and positive `Sov.SetByCaller.Damage.PoiseDamage` for pure Poise. The execution emits one `PoiseDamage` meta packet. Mixed attacks still emit one ordinary `Damage` packet and carry their Poise payload on that spec. A zero-body, zero-Poise effect with a `Sov.Status.Apply.*` asset tag emits the new `ControlRequest` meta marker. No second combat resolver was added.

All three packet types run Guard/Deflection, Stamina spending, routing, typed telemetry and break ordering. Standard Guard mitigates pure Poise and blocks status-only control; perfect Guard and Deflection reject both. Failed Guard and authored bypasses permit control. Poise recovery/superarmor retains its existing nonzero floor. Perfect Guard at five remaining Stamina still succeeds and breaks afterward.

`FSovDamageResult.bStatusApplicationRequested` records whether the request survived defense. A status consumer must still enforce the specific status immunity and source action validity; this boolean does not bypass those rules. For mixed hits, the previous rule requiring surviving applied Shield/Health/Poise damage is preserved. `RequestedPoiseDamage` records the payload before defense. `TargetTagsBeforeDamage` records target state before damage, death and delegate callbacks. Existing source/target typed result callbacks observe control-only attacks, while legacy positive-body-damage notifications remain unchanged.

`AttackId` is separate from the per-victim `TransactionId`. The resolver accepts it only from a native `ISovAttackReceiptSource` on the effect source object/causer, with the actual authoritative instigator validated. `UNarrativeCombatAbility::GetSovAttackIdentity` mints one identity per instanced authoritative activation and invalidates it before ending. A receipt must capture that ID and reject reuse after the ability ends or begins another activation. The mutable ability object itself is deliberately not a receipt. Missing native receipt leaves AttackId invalid; a damage context or per-hit transaction is not guessed to be a whole swing. Resource-loop content must use the native receipt wrapper for grouped heavy-hit rewards. No native incoming-attack interception/protection producer existed to wire; such an event is not fabricated from target proximity.


## Weak-point authoring

Each `FSovWeakPointZone.Consequence` provides:

- `BlockedAbilityTags`: active GAS ability-tag block counts owned by this zone.
- `CancelAbilityTags`: active abilities to cancel on a new break.
- `GrantedStateTags`: equipment/system state supplied by a native GameplayEffect. Choose tags that the existing relevant equipment or enemy system consumes, such as device-disabled; authoring an unused tag alone cannot disable arbitrary equipment.
- `Duration`: zero lasts until reset, positive values expire while the zone remains broken.

Use exact stable `ZoneId` identities and authored bone/material matchers. No bone-name heuristic chooses capabilities. A severable region can name `ConsequenceWeakPointIds` in its existing profile; those zones break without synthetic damage or Echo awards. An empty list preserves all current sever behavior.

Every zone owns one independent native effect handle and one block-count contribution. Effect expiry/removal releases only that zone's counts. Reset, ASC replacement and EndPlay remove owned handles and preserve external contributions. Explicit configured cancellation is not replayed during snapshot load. Granted state tags still invoke the ordinary consumers of those tags.

`FSovWeakPointStateSnapshot` stores broken IDs and each still-active consequence's remaining seconds (`-1` for permanent). `CaptureWeakPointState`, `CanRestoreWeakPointState` and `RestoreWeakPointState` support encounter snapshots. Restore validates the complete snapshot before mutation, clears pending break/reward and reveal state, reapplies only recorded sustained consequences and emits state refresh. It does not fabricate hit events, detailed break notifications or rewards. Narrative's component save interface captures the same struct; restoration can defer until ASC initialization. Expired finite consequences are not restarted by save/load or a repeated hit.

## Permanent anatomical state

`GetSeveredRegionMask`, `CanRestoreSeveredRegionMask` and `RestoreSeveredRegionMask` expose checkpoint-safe access. Existing terminated limb physics cannot be reversed by changing a bit mask. Restoring a smaller mask to a live participant returns false without mutation; the encounter system must respawn that participant and restore the saved record on the new actor. Narrative's direct component Load also preserves already observed sever bits and warns when callers attempt an impossible live rollback. Loading a superset reconstructs state without replaying detached-limb cosmetics.

## Validation

Performed here: the production-used portable packet/acceptance policy compiled with C++17, `-Wall -Wextra -Werror`, and passed 14 assertions. Scoped whitespace and source/header consistency checks passed. No Unreal Engine installation is available in this workspace, so UHT, Unreal compilation, runtime automation, replication and content playthroughs have **not** run.

Added runtime automation under:

- `ProjectVelkorran.Campaign.Defense.PurePoise`: actual transient-world GAS attacks, Guard/Deflection, friendly/channel immunity, one typed result, unchanged body resources.
- `ProjectVelkorran.Campaign.Defense.ControlOnly`: accepted no-damage control, Guard/Deflection/invulnerability rejection.
- `ProjectVelkorran.Campaign.WeakPoint.ConsequenceOwnership`: real hit and native Echo reward, ability cancellation and blocking, unrelated melee, overlapping zone ownership, effect removal, reset, external tag preservation, snapshot restore and invalid snapshot rejection.
- `ProjectVelkorran.Campaign.WeakPoint.ReentrantResetAndSeverRestore`: callback-driven reset clears pending effect/reward work; smaller sever-mask restore fails atomically.

The consequence test exercises the removal delegate used by expiry and dispels; actual wall-clock expiration and save/load through a packaged map remain engine validation requirements. Guard's five additional lifecycle tests and validation details are documented in `GuardLifecycleValidation.md`.

Suggested engine gate: run `Scripts/Validate-Unreal.ps1` for the campaign namespace, then test a configured weak point on a live NPC, two independent overlapping capability losses, expiry, a checkpoint before/after a nonlethal sever, and appearance reload. Content owners must supply valid zone mappings, ability tags and region profile links; no Blueprint or map assets were authored here.
