# Native Selene payloads

Stillpoint Grenade, Staccato Zero, Verity's Wake, and Dispatch now execute gameplay in their existing native ability classes. Axiom remains implemented by its existing separate native release path. This completes the paid action implementations; granting the Blueprint children and supplying weapon identities, presentation assets, encounter placement, and final tuning remain content work.

## Integration and migration

1. Keep the approved input/cost roster in [SeleneEchoAbilities.md](SeleneEchoAbilities.md). Use a Selene character with its explicit identity tag. Stillpoint requires an allowed wielded weapon, Zero/Wake require their allowed granting weapon, and Dispatch remains a universal signature.
2. Remove Blueprint gameplay from `Event ActivateAbility`, `Echo Ability Authority Committed`, animation notifies, and old projectile hit callbacks. Native code spends Echo and applies each action. Do not add another cost GE, input recall task, damage/status application, or ordinary-fire buff for Zero.
3. Keep start/local/end hooks for presentation. `Receive Native Selene Payload Released` runs on authority after the native action has been released; its actor is the grenade/wave/returning weapon or Zero's confirmed target. It does not authorize another damage application.
4. Optionally derive projectile presentation Blueprints from `ASovSeleneCombatProjectile`. Set the existing grenade/wave/returning class slots to those children and supply `PresentationMesh`, audio and cosmetic phase/hit events. Empty or incompatible old projectile classes safely use the native actor. The native base has no mesh asset, so gameplay can be functional but invisible until presentation is authored.
5. Reset old GE override slots to native defaults. The properties remain serialized for migration; runtime uses known native GE shells and ignores custom effect classes. This prevents accidental extra health damage, stacking, grants, or periodic execution from legacy effects.
6. Set Dispatch's `VerityWeaponClasses` to the actual Verity item classes. It finds only that owner's attached weapon visuals and snapshots their prior hidden state. It never removes an inventory item, unequips a firearm, hides unrelated weapons, or grants a new weapon on return.

Native releases occur immediately after the committed start hooks. Stillpoint/Wake/Zero close their action after release; their montage presentation must tolerate the end event without canceling the already released projectile. Dispatch remains active until return/expiry and supplies Busy through the existing Echo lifecycle. Axiom retains its own charge and recovery timing.

## Payload behavior and prototype tuning

| Action | Native behavior | Defaults |
|---|---|---|
| Stillpoint | Server ballistic grenade; fuse opens persistent sphere; first eligible overlap per actor receives remaining field control and DOT; late entrants supported | Fuse 0.8 s, radius 450 cm, duration 3.5 s, lockout 3 s, speed 1500 cm/s, optional detonation damage 0, Frozen DOT 12/s, resistant DOT 6/s |
| Zero | One server eye/control-rotation precision trace preserving HitResult; one damage transaction; accepted control freezes or chills resistant target | Range 15000 cm, base damage 60, AbilityScalar 1.75 exactly once, Poise 30, Freeze 2.5 s, lockout 3 s |
| Wake | Advancing flat swept lane; one target hit; world obstruction stops propagation; centerline or preexisting Chill allows Freeze, outer lane applies Chill | Range 2200 cm, width 500 cm, full centerline width 120 cm, speed 2200 cm/s, damage 50, Poise 20, control 3 s, Frost DOT 10/s |
| Dispatch | Server-steered outbound weapon; one target hit on each leg; queued/manual/timed/range recall; return restores presence | Echo 90, outbound 2.5 s / 2400 cm, speeds 1800/2600 cm/s, steering 180 degrees/s, lifetime 7 s, damage 70 and Poise 35 per leg, return-only chilled/frozen bonus Poise 30 |

The new damage/DOT/Poise numbers are exposed prototype defaults, not a claim of approved final balance. The preexisting named roster, Echo spends, range, width, fuse, field duration, steering, and maximum lifetime remain unchanged.

Damage routes through `UNarrativeDamageExecCalc` and the shared attribute resolver, carrying Echo/Thermal channels and the spending ability's source tag. Zero/Wake consume the context-matched typed result before applying control, so perfect defense cannot cause a follow-up Freeze. Multi-channel immunity rejects the payload only when all declared channels are immune. Stillpoint's area control is explicitly independent of guard/deflection. Friendly, neutral, dead, self, invulnerable, and fully damage-immune targets fail closed.

Frozen grants the existing Narrative movement lock and Busy; Chill grants slow-walking. Freeze-immune targets and bosses fall back to Chill. Separate finite Freeze-immunity grants last for Freeze duration plus lockout. Frozen-only DOT checks the target tag on every execution. Removing one effect never resets other effects' counts. Return shatter augments Poise in the same damage transaction and does not consume another system's Frozen/Chilled effect.

## Ownership and failure handling

- Native ability epochs are established before spending, superclass callbacks and Blueprint events. After every release-side gameplay mutation the source, weapon, epoch, identity, and active state are rechecked. Stale equipment, holster, cinematic, death, interrupted lifecycle, or replaced ASC prevents continuation.
- Projectiles copy source identity and tuning, retaining no instanced ability. Their server-only sweeps use bounded substeps, canonical target resolution, per-target ledgers reserved before callbacks, and same-endpoint visibility. World obstructions stop outgoing lanes; return obstruction expires Dispatch and restores its presentation instead of damaging through a wall.
- Dispatch's second press is replicated by GAS and latched if it arrives before the actor is assigned. Recall reuses the paid actor. Its 50 ms watchdog detects ownership, source, active weapon, and equipment changes; hard interruption/death also uses the existing Echo cancellation lifecycle.
- Verity absence is a finite GE owned by one handle. Every completion path removes that handle, restores captured visuals, removes delegates/tasks/timers, and destroys the actor. A missing presentation allowlist leaves the original visual visible but does not corrupt inventory.
- Projectile phase and movement replicate. Phase hooks run on replication for remote presentation; hit hooks are authority notifications and require the project's replicated cue/presentation layer if observers need hit cosmetics. No client supplies damage, target lists, projectile position, or charge.

## Files

The existing `Abilities/SovGameplayAbility_SeleneEcho.h/.cpp` now supplies the strict native release adapter. Separate ability CPPs implement `SeleneStillpointGrenade`, `SeleneStaccatoZero`, `SeleneVeritysWake`, and `SeleneDispatch`. Shared pieces are `Combat/SovSelenePayload.*`, `Combat/SovNativeDamageReceipt.h`, `Effects/SovGameplayEffect_SeleneControl.*`, and `Projectiles/SovSeleneCombatProjectile.*`. Axiom's native payload files are unchanged.

## Verification

`Tests/Portable/SovSelenePayloadMathTests.cpp` compiles the exact production centerline, recall, and range-clamping helper with C++17. It covers inclusive boundaries, direction normalization, malformed values, rotated lanes, independent recall limits, and overshoot prevention. It passed with `-Wall -Wextra -Werror` in this workspace.

Eight `ProjectVelkorran.Campaign.SelenePayload.*` Unreal automation tests use transient physics worlds, real Narrative ASC/attributes, real weapon item fixtures, paid GAS activations, and the production actors/effects. They cover Zero's single scaled hit, control defense receipt and multi-channel immunity, Stillpoint late entrants, Wake centerline/flank/wall behavior, Dispatch separate leg hits and cleanup, queued GAS recall before actor assignment and late input rejection, asset-free native launch defaults, and Frozen-only DOT after thaw. These tests are authored but **not executed**: the workspace has no Unreal Engine 5.7 build/editor installation. UHT, editor compilation, GAS duration timers, collision integration, and network PIE remain required validation gates.

Run in the UE 5.7 editor command line with `Automation RunTests ProjectVelkorran.Campaign.SelenePayload`, and rerun `ProjectVelkorran.Campaign.AxiomNullPulse` because all Selene classes share the adapter. Before encounter acceptance, test two-client prediction/recall, immediate release input, source death/equipment change during flight, temporary status removal/refreeze overlap, freeze-immune bosses, close walls, late join presentation, and the exact Verity visual classes.
