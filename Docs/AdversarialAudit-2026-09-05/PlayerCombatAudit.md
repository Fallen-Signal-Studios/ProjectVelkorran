# Adversarial player combat audit, 5 September 2026

Read-only source audit of ProjectVelkorran local HEAD `6225c68d50fbea605d7c7dfc8d61d5569805e28f`. Relevant authoritative TDD sections reviewed in the revised 14 August 2026 v2 document: 5.2–5.4, 6.5–6.10, 7.2–7.6. `Docs/CampaignV2ChangeLog.md` explicitly permits the revised weapon-specific ability rosters, finite Cinderline ammunition and transient combat sustain. I do not report the old prototype names/costs as missing functionality. No Unreal compilation or runtime execution was available or performed by this audit agent. Findings below are source traces; the proposed engine regressions are not represented as executed tests.

Paths below are relative to `/workspace/scratch/4e000d64b13d/ProjectVelkorran`. `Arsenal/` abbreviates `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/` only in this note.

## PC-01: Active melee ignores a newly applied poise break [P1, source-proven]

**Evidence:** `Source/ProjectVelkorran/Private/Melee/SovGameplayAbility_Melee.cpp:35–42,56–70,193–227`; `Private/Melee/SovAbilityTask_MeleeSweep.cpp:26–32,49–75,103–129`; `Private/Components/SovPoiseComponent.cpp:EnterBrokenState,SetPoiseState,UpdateOwnedStateTags` (approximately 604–638 and 764–817). `Arsenal/Private/GAS/NarrativeCombatAbility.cpp:80–86` is the shared dispatch gate.

**Exists / requirement:** Finite authored melee graphs, per-node attack GUIDs, socket sweeps, cover checks, buffered branches and exertion payments are real. TDD §6.6 requires broken poise to produce stagger; §5.2 says failed commitment is punishable, with specific superarmor windows rather than blanket protection.

**Trigger / failure:** Start a normal native melee node, then reduce the attacker's Poise to zero during startup or active time without killing it. PoiseComponent adds `Sov.State.Poise.Broken`. ActivationBlockedTags only governs new activations. No native listener cancels this active melee; ContextValid omits that tag. The task's gate only verifies the active attack identity/ASC, and OnContact still applies the damage effect. Existing generic ASC cancellation code handles death, not poise. The input buffer rejecting a new branch on poise break does not stop the already scheduled collision window. ContextValid also omits newly arriving interaction, ragdoll, Frozen and external Busy state, although some particular producers separately cancel attacks.

**Remedy:** Preserve the melee task/graph. Extend the native active-action gate and bind relevant interruption tags, using a shared policy with Evade/Echo where possible and an explicit documented superarmor exception. Add ScopeLock-aware teardown and remove the exact owned task/montage/input window. Do not depend on a Blueprint reaction montage to shut down gameplay damage.

**Dependencies / risk / order:** Shared Narrative ability lifecycle, PoiseComponent and GAS tag ordering; moderate change risk because interrupt ordering affects charged release and defensive exits. First combat remediation slice, together with PC-02.

**Validation / DoD:** Actual native activation, break attacker Poise through the damage transaction at startup, active sweep and recovery; assert no later contacts, no follow-up window, no lingering Busy, no delayed charged damage. Verify legitimate superarmor prevents the break rather than allowing a broken attack to continue. Run at 30/60/120Hz and with an active window crossed by a hitch. Existing `SovMeleeRuntimeTests.cpp` verifies geometric ledger/finite graph and cover only; it does not cover these interruption cases.

## PC-02: Echo payment can resume activation after synchronous cancellation [P1, source-proven control-flow defect; engine regression required]

**Evidence:** `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_Echo.cpp:190–218,283–350,353–382`; `Private/Components/SovEchoComponent.cpp:TrySpendEcho,HandleEchoAttributeChanged`; `Arsenal/Private/GAS/NarrativeCombatAbility.cpp:133–182`. Selene/Tarrik subclass epochs are in `Private/Abilities/SovGameplayAbility_SeleneEcho.cpp:ActivateAbility` and `SovGameplayAbility_TarrikLifecycle.cpp:ActivateAbility`.

**Exists / requirement:** Authority-only meter debit, per-activation attempted/succeeded flags, finite duration and interruption delegates exist; subclass payload epochs protect much later work. A canceled or replaced activation must never register new callbacks or execute its Blueprint payload hooks. TDD §6.5 calls for clean failure, and the native base explicitly promises payment before any generic Blueprint payload event.

**Trigger / failure:** Bind an `OnEchoChanged` or `OnEchoSpent` listener that calls FinishEchoAbility/CancelAbility during TrySpendEcho. EndAbility resets flags and cleans Narrative target-data registration. After it returns, the *outer assignment* at Echo.cpp204 sets `bAuthorityEchoSpendSucceeded=true` again. ActivateAbility calls Narrative Super at306 before checking `IsActive` at307. Narrative Super generates a new attack ID, registers target-data callback and invokes its generic Blueprint activation path. A canceled activation therefore reenters setup; its newly installed binding has no corresponding normal EndAbility cleanup. If a callback reactivates the same instance, outer base state can also overwrite the newer activation before subclass epoch checks run. The exact UGameplayAbility commit/scope-lock behavior needs the regression on UE5.7, but the missing pre-Super epoch/state fence is explicit in this source.

**Counter-review qualification:** Narrative ActivateAbility resets `bCombatEndPending` before it binds target-data callbacks. A deferred End therefore does not make this setup safe. However, actual UE5.7 CommitAbility return behavior and scope-lock timing determine whether stale setup executes, whether the generic K2 hook runs, and whether a later queued End removes a new binding. An orphaned delegate is conditional on ending having completed before continuation; no engine observation proves that every payment cancellation leaks or executes a native payload.

**Remedy:** Preserve meter and native ability hierarchy. Put the epoch/spec/avatar/ASC ownership contract in EchoBase itself, before the cost call, and revalidate after every synchronous payment/Blueprint/GAS boundary. Do not assign outer results into shared per-instance state after the captured epoch changes. Only perform teardown after IsEndAbilityValid/ScopeLock handling, and do not let an old Ended callback clear a new activation's timers/handles.

**Dependencies / risk / order:** Shared Echo superclass and NarrativeCombatAbility delegate registration. High change risk because every signature ability inherits this path; fix before tuning or further payload work. First combat remediation slice.

**Validation / DoD:** Actual ASC activation with cancellation from both meter attribute and spend events; reactivation from an ended listener; source/avatar replacement during debit; inherited cost GE failure/reentrancy; scope-lock deferred cancellation. Assert at most one debit per successful activation, no generic/native presentation or payload hook after its canceled epoch, no leftover target-data delegate, and one owner for every duration timer. Existing Echo tests exercise nested arithmetic writes, not activation cancellation during payment.

## PC-03: Ammunition consumption is neither validated nor atomic [P1, source-proven]

**Evidence:** `Arsenal/Private/Items/WeaponItem.cpp:474–505,635–692`; `Arsenal/Private/Items/InventoryComponent.cpp:309–342`; `Arsenal/Private/Items/NarrativeItem.cpp:298–314`.

**Exists / requirement:** One Narrative inventory quantity includes magazine plus reserve; loaded clip is separately tracked by WeaponClipState. This is the suitable existing architecture and should remain. Approved TDD change requires finite magazine/reserve pacing and truthful HUD/save state.

**Trigger / failure A:** `ConsumeAmmo(-1)` passes `GetAmmoInClip() >= Amount`; inventory correctly rejects the negative removal, but WeaponItem increases AmmoInClip and returns true. INT_MIN also risks signed overflow. Zero succeeds without a shot resource payment. Public Blueprint consumers are not required to pass a positive integer by source contract.

**Trigger / failure B:** Start with inventory quantity20 and loaded clip1. A one-shot OnItemModified listener calls ConsumeAmmo(1) once more while outer ConsumeAmmo(1) is in progress. The first inventory removal publishes quantity19 before outer clip is reduced. The nested call still observes clip1, consumes another reserve round and reduces clip to0; outer call then reduces clip to-1. Both report success although only one round was loaded. OnItemRemoved can cause the same reentry. Moreover, WeaponItem ignores the actual ConsumeItem return quantity, so an ammo item that refuses removal still decrements its clip and authorizes the attack while retaining inventory quantity for reload.

**Remedy:** Refactor existing ConsumeAmmo into a validated, source-owned transaction. Require strictly positive bounded amount, reserve the clip before delegate publication, verify ammo source/inventory ownership and exact quantity removal, handle rollback without overwriting a newer owner/write, and fence reentrant consumption. Preserve the intended no-ammo bot exception separately. Share the admission/payment contract with primary-fire Blueprint events; no parallel ammo component needed.

**Dependencies / risk / order:** Inventory event semantics, reload/HUD, weapon save snapshots, source destruction during item callbacks. Medium/high change risk. Second combat slice after shared activation cancellation policy; ship no finite-ammo claim until fixed.

**Validation / DoD:** Negative/zero/INT_MIN/over-magazine requests cannot mutate or succeed; one-round magazine with plentiful reserve cannot authorize a second synchronous shot; denied inventory removal cannot authorize firing; item removal/replacement/owner switching during callbacks cannot corrupt another weapon. Assert `0 <= clip <= clip size`, total ammunition conservation, and identical save/load values. Existing portable save-ammo tests do not exercise this runtime mutation boundary.

## PC-04: Cinder Judgement can detonate behind cover when the muzzle clips through it [P1/P2, source-proven geometry]

**Evidence:** `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_TarrikEcho.cpp:1013–1053,1056–1082,1120–1163,1196–1204`. Compare the already safer code in `Private/Abilities/SovGameplayAbility_TarrikCinderlineRequiem.cpp:92–100`.

**Exists / requirement:** Judgement uses a physical weapon socket/fallback muzzle, eye aim trace, world impact and LOS-limited radial blast. TDD §6.9 requires cover response and collision/presentation consistency.

**Trigger / failure:** A valid socket within MaximumMuzzleDistance lies on the far side of a thin wall (a normal animated gun-clipping case). The eye aim trace hits the near face *behind* that muzzle. ShotDirection becomes negative relative to view; the muzzle trace hits the far face and the explosion occurs on the enemy side of cover. There is no eye-to-muzzle bridge check, nor prevention of backward shots. Requiem already checks the bridge and rejects reverse convergence, proving a suitable local policy exists.

**Remedy:** Refactor Judgement to reuse the existing bridge/forward-convergence behavior, with a shared helper if appropriate; conservatively resolve blocked muzzle on the player's side. Do not remove the radial LOS check.

**Dependencies / risk / order:** WeaponTraceChannel versus Visibility policy, character/attachment ignores and presentation endpoints. Moderate/contained risk. Second combat slice with ammunition and weapon-state correctness.

**Validation / DoD:** Thin-wall socket clipping, fallback muzzle clipping, eye hit behind socket, near enemy and offset camera. Assert no backward ray and no damage/physics impulse across cover; tracer/impact endpoints match collision. Compare ordinary Judgement and Requiem with identical geometry.

## PC-05: Judgement re-reads mutable ability ownership inside an already released multi-target payload [P2, source-proven boundary inconsistency]

**Evidence:** `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_TarrikEcho.cpp:1171–1215,1228–1233,1276–1297,1306–1312,1361–1419`. Compare captured contexts/epochs and `USovNativeDamageReceipt` used in `Private/Combat/SovSelenePayload.cpp` and `Private/Combat/SovTarrikPayloadSupport.h`.

**Exists / requirement:** Judgement's direct shot and radial continuation apply through the canonical damage execution. Released projectiles are allowed to outlive the GA where designed; this is not a demand that every canceled GA erase a fired projectile. Ownership and attribution must remain tied to the release that paid for it.

**Trigger / failure:** A direct-hit or first-blast target callback ends/reactivates the ability or changes the shared ASC avatar. ApplyJudgementExplosion and ApplyJudgementDamage subsequently obtain SourceActor and SourceASC from *CurrentActorInfo*, not the release snapshot. The remaining blast may acquire a different source/attitude/spec context. BeginJudgementRecovery likewise affects whichever activation is now active. `ApplyJudgementDamage` also computes success from four before/after aggregate attributes after synchronous delegates; a damage reaction that heals/refills can incorrectly report no hit. Conversely a nested unrelated damage can make the original packet appear successful.

**Remedy:** Preserve damage execution. Capture source ASC/avatar/source item/spec/activation and one release context before applying any packet. Either finish the committed release against that immutable context or abort stale continuation, according to an explicit payload policy. Recovery/presentation belong only to the captured epoch. Use the existing resolved transaction receipt to measure this packet, never aggregate attribute deltas.

**Dependencies / risk / order:** Shared Echo epoch fix PC-02, damage callback ordering and handoff semantics. Moderate risk. Third combat slice; reuse existing Selene/Tarrik receipt helpers rather than creating a new damage channel.

**Validation / DoD:** Damage callback cancels/reactivates same GA; callback changes avatar; first target immediately dies/destroys; callback heals the applied damage. Remaining target ownership must never migrate, new activation timers must remain untouched, and hit feedback must represent the original transaction.

## PC-06: Replay ledgers grow without a lifetime bound [P2, source-proven capacity issue]

**Evidence:** `Source/ProjectVelkorran/Public/Components/SovTarrikEchoGenerationComponent.h:171,184–187`; Tarrik cpp:219–221,860–866,895–920,946–947. Selene header:165–170; Selene cpp:267,335,345–352,392–400,413–414. Search confirms no removal/reset of the Consumed* sets during this component lifetime.

**Exists / requirement:** Transaction and attack GUID deduplication prevents repeated Echo rewards, including across ordinary chain resets. Preserve that invariant. AAA resource behavior also needs bounded long-session memory and predictable budgets.

**Trigger / failure:** Every sourced damage transaction, including rejected Echo/periodic/irrelevant packets, is retained in the hero's TSet until actor destruction. Encounter resets clear only cadence/precision/HeavyAttacks and leave Consumed* tombstones. The count grows linearly with total combat history; incomplete HeavyAttacks additionally remain for the encounter. A benchmark is still needed to quantify practical memory impact; do not describe an observed OOM.

**Remedy:** Refactor deduplication around the existing transaction/attack receipt lifetime and encounter/possession epoch. A stale epoch must fail validation after compaction; simply clearing a set or evicting oldest GUIDs would reopen replay awards. Publish capacity/count diagnostics and a finite pending-receipt policy.

**Dependencies / risk / order:** Damage authority, receipts, checkpoint/encounter resets. High correctness risk if naively capped. Third combat slice after lifecycle ownership. Test 100k+ transactions and many encounter transitions, retain old receipts and prove replay rejection after cleanup; verify stable allocation bounds and unchanged reward totals.

## PC-07: Public Selene effect-class fields are silently ignored [P2, source-proven authoring API defect]

**Evidence:** `Source/ProjectVelkorran/Public/Abilities/SovGameplayAbility_SeleneEcho.h:86–98,146–152,216–223,387–396`; associated constructors assign these properties, but payload calls in `Private/Abilities/SovGameplayAbility_SeleneStaccatoZero.cpp:60`, `SovGameplayAbility_SeleneDispatch.cpp`, `SovGameplayAbility_SeleneStillpointGrenade.cpp`, `SovGameplayAbility_SeleneVeritysWake.cpp` do not pass them into payload parameters. `Private/Combat/SovSelenePayload.cpp:Damage,GrantDuration,FrostDOT` hardcodes the native classes.

**Exists / requirement:** Native payloads deliberately constrain damage/status behavior. That is defensible. However editable Blueprint properties named EmpoweredShotDamageEffectClass, OutboundDamageEffectClass, ReturnDamageEffectClass, FreezeEffectClass and similar advertise an effect override that runtime never consults. The programmer/designer contract must be truthful.

**Failure:** A designer changes the documented class to tune a valid child, add a gameplay cue, or change duration policy; validation still succeeds and gameplay silently uses the original native CDO. This is a source API defect, not merely absent in-editor setup.

**Remedy:** Decide which extension points are supported. Safely validate and pass permitted native subclasses through existing context/parameters, or explicitly deprecate/remove/migrate unsupported properties with actionable asset validation. Do not open arbitrary damage effects just to satisfy the property label.

**Dependencies / risk / order:** Blueprint serialization/content migration and native GE invariants. Moderate risk; after lifecycle/weapon correctness, before editor content rollout. Validation: allowed child override has observable intended cue/behavior; unsafe override fails content validation; migrated assets have no silently dead configuration.

## PC-08: Recoil aim/hip presets are reversed [P3, source-proven]

**Evidence:** `Arsenal/Private/Items/RangedWeaponItem.cpp:201–215`. `GetRecoilImpulse()` uses `bIsAiming ? HipRecoilImpulseTranslationMin : RecoilImpulseTranslationMin`, likewise Max. `GetWeaponSpread()` at106 separately chooses the local aiming tag for local users; recoil only reads the authoritative aim tag at208.

**Exists / requirement:** Separate hip and aimed recoil presets exist to support Cinderline/Selene response. Their names and observed use disagree, undermining designer tuning. This does not require a new recoil system.

**Remedy / tests:** Swap the presets to match names and explicitly select the relevant local/authority aiming state consistently. Preserve existing authored values only through a documented migration if content compensated for the reversed behavior. Use deterministic distinct min=max presets and verify hip versus aim output, including predicted local aim start/stop. Low source risk, moderate content-retuning risk. Weapon correctness slice after PC-03/04.

## Coverage risks requiring validation or explicit design decisions, not proven missing gameplay

1. **Actual montage/skeletal sampling is untested by current melee regression.** `SovMeleeRuntimeTests.cpp` moves a test mesh via SetWorldLocation and manually invokes TickTask after `LEVELTICK_TimeOnly`. It verifies interpolation geometry, not actual montage evaluation order, socket animation, tick prerequisites, root motion, or motion-blurred fast arcs. An ordinary engine world at 30/60/120Hz with real rig/montage fixtures is needed before claiming frame-rate-independent melee. No claim that all current sweeps necessarily miss.
2. **Staccato Zero uses Visibility rather than the configured WeaponTraceChannel and does not request physical material.** `SovGameplayAbility_SeleneStaccatoZero.cpp:52–60`; Narrative primary traces and melee use the project WeaponTraceChannel and return physical material. If authored hurtboxes block weapon traces but ignore Visibility, Zero cannot hit the same target; if precision relies on physical material, its HitResult omits that metadata. Review exact collision policy and test modular hit proxies. Content determines whether this discrepancy is currently observable; source contract should use one deliberate semantic query policy.
3. **Combat tests do not close the player loop.** Existing tests largely construct native actors/abilities and invoke isolated payloads. They do not establish that real EnhancedInput -> Narrative semantic input -> weapon grants -> ready draw -> paid release -> effect -> sustain pickup -> reload -> checkpoint works end to end. Blueprint input/grants are the user's remit, but the engineering gate should include a real integration fixture instead of extrapolating from unit cases.
4. **Third-person precision convergence:** `SovSelenePayload::Aim` starts at actor eyes and copies controller rotation, while hard-lock/aim snap use PC GetPlayerViewPoint. A shoulder-offset camera can put the visible reticle and native aim line on different parallel rays. Need close-range/small-weakpoint comparisons before declaring a defect; the shipped camera composition is unavailable. Reuse one validated aim point plus separate obstruction check if confirmed.
5. **Finite combo graph expressiveness:** FSovMeleeAttackNode exposes one normal follow-up plus an evade exit. It cannot directly express heavy and ranged transition branches despite TDD §6.8. Existing Blueprint abilities might provide those transitions around this native graph. Audit those assets when available; do not label the whole game feature absent based solely on a limited native graph.
6. **Defensive cancel atomicity:** preflight verifies stamina/geometry, then payment and FinishMelee emit callbacks before TryActivateAbility(Evade). A callback may invalidate geometry/ownership. Decide whether the cost pays for cancellation itself or only successful evade; if the latter, the two-stage transaction needs rollback/commit semantics. This is a design-contract gap, not an unconditional double-charge claim.

## Systems worth preserving

The code already has substantial real engineering: canonical Narrative attributes/damage execution, resolved transaction receipts, native hero identity gates, finite payload lifetimes, wield-source checks for Selene, shared Echo storage, stamped melee hit ledgers, source-to-contact cover checks, explicit exertion admission, projectile target ledgers, protection and weak-point proof, ammo/Echo sustain restrictions, native targeting/nav/LOS loss policy, and a sizable transforming weapon state machine with replicated phase/serial and montage ownership. The audit recommends extending or refactoring those boundaries; none of the findings justify replacing them with a second combat architecture.

## Recommended combat remediation slices

1. **Interruptible paid actions:** PC-01/02, shared epoch ownership, native lifecycle regressions. Done when canceled/broken actions cannot dispatch or corrupt their replacement and every owned callback/timer is accounted for.
2. **Trustworthy weapon transactions and cover:** PC-03/04/08, query-policy decision from risk2, explicit reserve conservation and close-cover tests. Done when one loaded round authorizes at most one shot and all weapon endpoints respect cover.
3. **Stable payload attribution and bounded histories:** PC-05/06/07, validated designer extension points, stress/replay tests. Done when multi-target releases retain their original owner, receipt stores have measured bounds without replay holes, and exposed configuration affects runtime or is rejected visibly.
