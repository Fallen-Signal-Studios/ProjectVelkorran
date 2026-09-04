# Selene Phase 1 core loop

This slice gives Selene her first native, character-specific combat loop: tap a short Deflection window, intercept one eligible attack through the authoritative damage pipeline, break a deliberately authored enemy weak point, or use Axiom to sever a live command network, then receive Echo from the validated result. It does not infer precision from critical chance, raw damage, a bone-name convention, Shield break, Device Disabled, or cosmetic Blueprint events.

## Implemented scope

- `ASovSeleneCharacter` owns one `USovDeflectionComponent` and one `USovSeleneEchoGenerationComponent` in addition to the shared Echo, Shield, Health recharge, and Poise components.
- `USovGameplayAbility_SeleneDeflection` is a local-predicted tap ability on `Narrative.Input.AltAttack` with an authority-validated result.
- Deflection is a separate `ESovDefenseKind::Deflection` path. It is not Selene-flavored Guard and never enters sustained mitigation, chip damage, Guard break, or Guard-counter logic.
- `USovWeakPointComponent` gives an eligible target explicitly authored, independently breakable zones identified by skeletal bones and/or physical materials.
- `USovCommandLinkComponent` owns one authored encounter link's authoritative `Inactive`, `Active`, and `Severed` lifecycle, participant tags/effect, and unique Sever transaction.
- `USovSeleneEchoGenerationComponent` awards `+10` for an authoritative perfect Deflection, `+8` for the first authoritative break of an authored hostile weak point, and `+12` for Selene's first valid Sever of an active hostile link instance.
- A successful Sever temporarily reveals each participant's remaining weak points through localized red, bone-attached decals.
- Axiom now owns native authoritative charge/release, Shield-only collapse, bounded recharge/device suppression, and command-node selection. Its Blueprint supplies presentation and its actual weapon grant/allowlist; see [AxiomNullPulse.md](AxiomNullPulse.md). The other four Selene Echo abilities still need their authored payload implementations.
- Body hits, critical chance alone, repeated hits on a broken zone, friendly targets, Tarrik, and ordinary damage from an Echo ability do not receive precision rewards. The validated command-link Sever is the narrow exception allowed during Axiom's active Echo-ability state.

## One-time editor setup

Close the editor and perform a full `ProjectVelkorranEditor Win64 Development` build after pulling the implementation. The slice adds reflected native classes, component templates, enum/result fields, developer settings, and native Gameplay Tags; do not use Hot Reload or Live Coding for the first load.

### Selene player Blueprint

1. Reparent the Selene player Blueprint to `ASovSeleneCharacter` if it is not already using that parent.
2. In Components, verify exactly one inherited `SovDeflectionComponent` and one inherited `SovSeleneEchoGenerationComponent` are present.
3. Remove Blueprint-added Deflection, parry, Selene Echo generator, or duplicate shared resource components. Preserve old graphs only long enough to migrate presentation assets; they must not apply damage, spend Stamina, mutate Echo, or decide success.
4. Keep `Sov.Character.Player.Selene` on the Player Definition. Verify the runtime ASC has Selene's identity and does not retain `Sov.Character.Player.Tarrik`.
5. Compile and save the Blueprint after the native inherited templates appear.

### Deflection ability

1. Create `GA_Selene_Deflection` with native parent `USovGameplayAbility_SeleneDeflection`.
2. Add it once to Selene's default Narrative Ability Configuration. Do not also grant it from every weapon.
3. The native input is `Narrative.Input.AltAttack`. Narrative may activate every granted spec that claims the same input, so verify that no simultaneously active weapon or placeholder ability also owns Alt Attack. Resolve any conflict in the Ability Configuration before testing.
4. Leave the ability Cost Gameplay Effect empty for the prototype. The authoritative intercepted hit pays Deflection Stamina through the damage pipeline; adding an ability Stamina cost would charge twice. The native recovery is the current repeat-rate limiter.
5. Use `Deflection Ability Started` and `Deflection Ability Ended` for character montage, pose, audio, Niagara, and UI setup/cleanup only. They execute in the predicted/authority ability lifecycle and are not permission to apply gameplay. Verity's weapon-skeleton montage has a dedicated native path described below.
6. Use the cosmetic `Perfect Deflection` event for locally controlled hit confirmation. Do not grant Echo, damage the attacker, or change target state from that event.

### Verity Deflection spin

1. Open the Verity weapon visual Blueprint derived from `ASovTransformingWeaponVisual` (for example, `BP_VerityWeaponVisual`). Under **Sovereign → Weapon Transition → Deflection**, assign the Verity-skeleton montage to **Weapon Deflection Montage**. Assign **Local Weapon Deflection Montage** only when first person needs a different asset; otherwise it falls back to the main montage.
2. Create or assign a weapon Anim Blueprint that uses Verity's weapon skeleton on both `WeaponMesh` and `LocalWeaponMesh`. Add a Slot node whose name matches the montage's Slot track between the base pose and the output pose.
3. Disable **Use Native Single Node Weapon Animation** on the weapon visual. Single-node playback does not run the weapon AnimBP Slot graph required by a montage. Once disabled, the weapon AnimBP/Blueprint must also present Verity's holstered, deploy, ready, and retract states instead of relying on the native `PlayAnimation` path.
4. Optionally set **Weapon Deflection Montage Play Rate**, **Weapon Deflection Montage Start Section**, and the cancellation blend-out time. Empty montage/section values are safe no-ops.
5. Do not call the montage from `Deflection Ability Started` as well. Native code starts it once after the predicted Deflection commits, multicasts it to observers, lets it finish through a normal recovery end, and stops it when death, Fatal, Poise break, ragdoll, Sequencer control, or prediction rejection cancels the ability.

Deflection is blocked while `Narrative.State.Weapon.Equipping`, and the montage plays only while the transforming visual is fully Ready. This prevents the spin from overwriting draw/deploy/retract presentation.

### Combat project settings

Under **Project Settings → Narrative - Combat Settings**, verify:

| Setting | Prototype value | Ownership |
|---|---:|---|
| Deflection half-angle | 65 degrees | Project combat settings |
| Standard Deflection Stamina damage | 8 | Project combat settings |
| Heavy Deflection Stamina damage | 18 | Project combat settings |

An eligible hit needs a valid source/damage causer so the server can test Selene's facing plane. Standard and Heavy instant attacks may be Deflected inside the window. `Sov.Damage.BypassDeflection`, Environmental, periodic/non-instant, Unblockable, and Fatal damage may not. Author explicit bypass tags on exceptional instant damage specs, not in a cosmetic impact graph.

### Weak-point targets

1. Add exactly one `USovWeakPointComponent` to each enemy Blueprint that exposes precision targets. Do not add it to every NPC merely to make body shots reward Echo.
2. Add a stable, unique `ZoneId` for each authored zone, such as `SensorCore` or `RocketPodLeft`.
3. Give every zone at least one matcher:
   - one or more exact skeletal `HitBones`, optionally including descendant bones; and/or
   - one or more `PhysicalMaterials` for a non-skeletal or layered target.
4. Keep `Minimum Applied Damage` at `0.01` for the prototype unless the target genuinely requires a stronger hit. Only applied Shield plus Health damage counts; Poise-only contact does not break a zone.
5. Ensure the authoritative trace places the bone and, when used, physical material in the damage effect's `FHitResult`. A cosmetic trace or reticle result is insufficient.
6. Set the target's Narrative team so Selene regards it as hostile. A valid break on a friendly or neutral actor changes the target's authored zone state but does not reward Selene.
7. Bind `On Weak Point Broken` and `On Weak Point State Changed` only for target presentation and encounter reactions. The replicated `BrokenWeakPointIds` array is the gameplay truth.
8. For actors that participate in an Axiom-severable network, author each zone's reveal attach bone/socket, relative transform, decal size, and optional material override. Assign the component's default Deferred Decal material and keep its vector parameter named `WeakPointRevealColor` unless the native parameter-name setting is intentionally changed.

An empty Weak Point component is a valid no-op, but invalid IDs, duplicate IDs, and zones with no bone/material matcher are authoring errors. A zone currently resets to its authored starting state when the target returns from death. It is replicated for active play but is not yet a campaign checkpoint/save record.

Command-node and reveal-material setup is documented in [SeleneCommandLinkAndWeakPointReveal.md](SeleneCommandLinkAndWeakPointReveal.md). Migrate Axiom's Blueprint using [AxiomNullPulse.md](AxiomNullPulse.md): remove old charge/release timers, pulse damage/status/Sever calls and normal-end logic, retain presentation, and grant it only from the actual wielded Axiom weapon.

## Prototype tuning

| Rule | Native default | Location |
|---|---:|---|
| Perfect Deflection window | 0.11 s | Selene's inherited Deflection component |
| Minimum Stamina to start | 8 | Selene's inherited Deflection component |
| Ability recovery | 0.35 s | `GA_Selene_Deflection` |
| Perfect Deflection Echo | +10 | Selene's inherited Echo generator |
| First weak-point break Echo | +8 | Selene's inherited Echo generator |
| First active hostile command-link Sever Echo | +12 | Selene's inherited Echo generator |
| Weak-point reveal after Sever | 5.0 s | Command-link component |
| Standard/Heavy hit Stamina | 8 / 18 | Narrative Combat Settings |
| Facing half-angle | 65 degrees | Narrative Combat Settings |

The start threshold is not a prepaid cost. The resolved hit spends up to the remaining Stamina and may reduce Selene to zero. A successful Deflection removes that hit's Shield, Health, and Poise damage, consumes the one-hit window before reward/presentation callbacks, and leaves the ability in its authored recovery. There is no held or late chip-defense state after the 0.11-second window closes.

## Authority and event flow

1. The owning client predicts `GA_Selene_Deflection`, calls `BeginDeflection`, and opens `Sov.State.Deflecting`; the authority runs the same activation.
2. `UNarrativeAttributeSetBase` resolves the incoming damage on the server. It checks the active window, facing, bypass/unblockable policy, and current Stamina before Shield/Health routing.
3. A success produces one typed `FSovDamageResult`: `DefenseKind = Deflection`, `bDeflected = true`, `bPerfectDefense = true`, zero routed damage/Poise, and the authoritative Stamina payment. `bGuarded` remains false.
4. `USovDeflectionComponent` consumes the server window before callbacks, emits `Sov.Event.Deflection.Perfect`, and multicasts the immutable result for presentation. The local timer is a fallback if the unreliable presentation multicast is lost.
5. `USovSeleneEchoGenerationComponent` observes the validated Deflection on authority, writes `+10` through `USovEchoComponent`, and sends an owning-client presentation notification through `OnSeleneEchoAwarded`.
6. For a weak point, the target component resolves and replicates the new break during the target-side damage callback. Selene's source-side callback consumes that exact `FSovDamageResult.TransactionId` once, then writes `+8` on authority. The unique transaction ID prevents a reused Gameplay Effect context from claiming an earlier break.
7. Axiom spends once, charges on authority, and releases on input release or its full-charge timer. Its native range/cone/visibility checks authorize the actual command node before the internal `TrySeverAxiomCommandLink` call; direct external helper calls return `Invalid`. Its `USovCommandLinkComponent` alone may perform `Active -> Severed`, remove its participant contributions, mint the unique transaction, and begin the replicated weak-point reveal.
8. Axiom routes only `NewlySevered` to Selene's generator. The generator consumes the transaction ID once and writes eligible `+12` through `USovEchoComponent`; `AlreadySevered`, Shield break, Device Disabled, and link deactivation never award it.

`USovEchoComponent` remains the only shared Echo writer/storage policy. Its normal `0–100` clamping, encounter activity, decay, threshold, and checkpoint-value APIs still apply. UI may observe `OnSeleneEchoAwarded` and the shared Echo delegates; it must not predict or reapply an award.

## Multiplayer and PIE matrix

Run the functional checks first in Standalone, then repeat the network-sensitive rows in two-player listen-server PIE with Selene controlled by the remote client. When available, repeat them on a dedicated server with two clients. Use packet lag/loss simulation for the prediction rows after the zero-lag pass.

| Area | Test | Expected result |
|---|---|---|
| Composition | Inspect Tarrik and Selene | Selene has one Deflection and one Selene generator; Tarrik has neither; shared components remain singletons |
| Identity | Try the Selene ability with missing/incorrect identity | Activation fails; a Tarrik-tagged ASC never earns Selene Echo |
| Input | Grant another active Alt Attack spec | Treat the duplicate activation as an authoring failure; ship with one active claimant |
| Timing | Hit before, inside, and after the 0.11 s window | Only the in-window authority result Deflects |
| Facing | Hit inside and outside the 65-degree half-angle | Only the server-facing hit Deflects |
| Classification | Standard, Heavy, BypassDeflection, Environmental, periodic Burn, Unblockable, and Fatal | Standard/Heavy instant hits can succeed; bypass/environmental/periodic/unblockable/fatal damage routes normally and awards no Deflection Echo |
| Stamina | Start below 8; then resolve standard/heavy hits at several valid Stamina values | Start below 8 fails; success spends the configured amount up to remaining Stamina, once |
| Consumption | Land two eligible hits in the same frame/window | The first authority success consumes the window; the second is not Deflected or rewarded |
| Cancellation | Apply death, Fatal, Poise Broken, ragdoll, or Sequencer control during recovery | Ability/window closes, tags clean up, and no late award occurs |
| Prediction | Activate as remote client at 150 ms simulated lag, then miss and hit | No stuck Deflecting/Busy tag, double Stamina payment, or duplicate `+10` |
| Deflection presentation | Observe owner, server, and second client | Gameplay resolves once; owner gets one confirmation; proxies do not grant gameplay or duplicate Echo |
| Verity spin montage | Deflect with Verity in third person, first person, and as a remote client | The predicted owner sees one immediate spin; observers see one spin; normal recovery lets it finish; cancellation blends it out |
| Weak point | Break one hostile authored zone | Broken ID replicates and Selene receives exactly `+8` |
| Weak-point replay | Hit the same broken zone repeatedly | No additional break or Echo until an authoritative reset |
| Multiple zones | Break two distinct zones | Each newly broken zone can award once |
| Invalid precision | Body hit, critical body hit, Poise-only hit, missing HitResult, or unmatched bone/material | No weak-point break and no `+8` |
| Filters | Hit friendly, neutral, dead, or invulnerable targets; deal damage from an Echo ability | No Selene precision award |
| Replication | Join/relevancy after a zone is broken | Client reconstructs the replicated broken state without generating a second break event/reward |
| Reset | Restore the enemy from death or restart the encounter | Zones return to authored start state; no award occurs merely from reset |
| Resource cap | Earn at 96/100 and at 100/100 Echo | First grant clamps to 100 and reports only the applied amount; full meter grants nothing |
| Command link | Sever one active hostile authored link with Axiom | State becomes Severed, active contributions clear, linked specialist action cancels, and Selene receives exactly `+12` |
| Sever replay | Release a later valid Axiom pulse against the same severed instance | Native link transaction returns `AlreadySevered`; no second event, reveal, or Echo award |
| Axiom release | Input release and full-charge timer coincide | One authoritative pulse and one Echo spend; no late payload after interruption or source-weapon replacement |
| Axiom helper | Call Sever directly from Blueprint | `Invalid`; no state change or Echo award |
| Sever filters | Try inactive, immune, non-hostile, missing, outside-charge-cone/range, or occluded command nodes | No successful transition, reveal, or `+12` |
| Axiom independence | Collapse Shield/apply Device Disabled on an actor without an active link | Existing Axiom payload resolves, but it produces no Sever reward |
| Weak-point reveal | Sever a link whose participants own authored unbroken zones | Owner and proxies see localized red decals for the synchronized duration; broken/unconfigured zones do not appear |
| Command reset | Reset the encounter and Sever the new active instance | Reset itself grants nothing; the new unique instance may award once when legitimately severed |

Also run:

1. `Automation RunTests ProjectVelkorran.Campaign.Foundation`
2. `Automation RunTests ProjectVelkorran.Campaign.Selene`
3. `Automation RunTests ProjectVelkorran.Campaign.AxiomNullPulse`
4. `CompileAllBlueprints`

## Explicitly deferred

### Physical projectile reflection

Deflection currently consumes an eligible authoritative damage transaction; it does not reflect, retarget, or transfer ownership of the underlying projectile actor. That requires a separate server-owned projectile contract for collision consumption, velocity/target reassignment, faction attribution, and duplicate-hit prevention. The command-link/Sever slice does not alter that boundary.

### Runtime protagonist handoff

Runtime Tarrik/Selene handoff remains deferred. Narrative's ASC lives on PlayerState, so ordinary re-possession can carry prior ability specs, loose tags, effects, input claims, and component bindings into the next pawn. Test Tarrik and Selene in separate play sessions or with fresh PlayerStates until an explicit migration and cleanup policy exists. The campaign design permits only authored controlled handoffs; it does not permit free player switching.

### Other deferred Selene rules

Precision chains, mark/exposure kills, undetected threat bypass, Deflection counter/exposure payloads, command-link discovery/chaining, weak-point checkpoint persistence, and full camera/movement differentiation are not fabricated by this slice. Add each only when its real authoritative gameplay event and acceptance test exist.
