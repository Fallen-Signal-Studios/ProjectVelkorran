# Status and Eclipse Corruption Prototype

This slice closes the TDD's next Phase 1 systems dependency after Selene's network encounter. It adds one authoritative status owner for players and NPCs, the first playable Eclipse-corruption loop, and Selene's missing Deflection-to-Exposed payoff. It deliberately provides checkpoint capture/restore contracts without inventing the campaign checkpoint manager before consequence ownership is defined.

## Runtime ownership

| State | Runtime owner | Replication / persistence rule |
|---|---|---|
| Burn, Chill, Freeze, Device Disabled, Exposed | `USovStatusComponent` on every project player and NPC base | GAS effects/tags carry gameplay; a compact status view replicates UI/VFX identity, stacks, and expiry while authority retains exact handles and provenance |
| Eclipse exposure | `USovCorruptionAttributeSet` on `ASovPlayerState` | The persistent player ASC is the numeric source of truth |
| Corruption sources, bands, remedy, presentation | `USovCorruptionComponent` on `ASovPlayerCharacterBase` | Server owns transitions; compact band and presentation state replicate |
| Checkpoint representation | Versioned semantic structs | No actor pointers, timers, GUID handles, or active GE handles are serialized |

`ASovNPCCharacterBase` owns status but not player corruption. `ASovDroneNPCBase` explicitly opts into Device Disabled; ordinary characters do not. Both protagonists inherit exactly one status and one corruption component.

## Status request path

For a status coupled to damage, put the exact `Sov.Status.Apply.*` tag on the same instant damage spec that uses `UNarrativeDamageExecCalc`, then supply:

- `Sov.SetByCaller.Status.Magnitude`
- `Sov.SetByCaller.Status.Duration`

The damage transaction emits a typed `FSovStatusApplicationRequest` only when it actually reduces Health, Shield, or Poise and the target survives. The target's `USovStatusComponent` then performs authority, avatar, replay, immunity, eligibility, stacking, and definition validation. The aggregate `Sov.Event.Status.ApplicationRequested` gameplay event remains available for compatibility and presentation, but it is not the source of gameplay truth.

For an authored non-damage ability, hazard, or interaction, run on authority and call `Apply Status By Tag` on the target's `USovStatusComponent`. That helper creates the immutable request identity and exact target. Use the full `Apply Status` request only when the caller needs to preserve explicit source-ability tags or a transaction identity.

Do not apply a second duration GE in Blueprint. That would bypass centralized immunity, exact-handle cleanup, reapplication policy, and checkpoint semantics.

## Built-in launch definitions

The native definitions make the system usable before data assets exist. A `USovStatusDefinition` in `Status Definition Overrides` replaces the built-in definition with the same exact request tag.

| Status | Default | Reapplication | Defensive rule | Cleanse |
|---|---|---|---|---|
| Burn | 5 s, 1 s period | stronger replaces; equal refreshes; weaker is rejected | Burn or global immunity | Burn or global cleanse |
| Chill | 4 s, up to 3 stacks | add one clamped stack and refresh | Chill or global immunity | Chill or global cleanse |
| Freeze | 1.25 s hard control | duplicate rejected | Freeze/global immunity; 1.5 s recovery immunity after removal | Freeze or global cleanse |
| Device Disabled | 4 s | refresh | explicit device eligibility plus Device/global immunity | Device Disabled or global cleanse |
| Exposed | 1.5 s | refresh | Exposed or global immunity | Exposed or global cleanse |

Freeze grants Narrative's Busy, Movement Lock, and Weapon Block Firing constraints and stops current controller movement on successful application. This is the prototype anti-chain-CC contract; elites and bosses should author immunity rather than silently shortening the pose.

Every override definition must provide its request/state identity, duration and stack policy, immunity/resistance behavior, at least one cleanse rule, UI priority, normal presentation tag, accessibility presentation tag, checkpoint behavior, and any AI/animation constraints. Invalid definitions fail closed and do not replace the built-in.

### Current damage migrations

Cinder Sticky Grenade and Velkorran's Hunger now publish Burn magnitude/duration on their resolved damage transaction. They no longer perform an independent post-hit attribute comparison or directly apply a Burn GE. Consequently:

- Guard, Deflection, immunity, fatal hits, and zero-damage hits cannot receive a detached Burn;
- Hunger and Grenade resolve to one canonical Burn family;
- equal Burn strength refreshes, stronger Burn replaces, and weaker Burn cannot overwrite it;
- Burn ticks carry Thermal plus Bypass Guard and Bypass Deflection and cannot recursively request Burn.

## Selene: Deflection to Exposed

A successful perfect Deflection now applies the built-in 1.5-second Exposed status to the logical hostile attacker. It never applies Exposed to the projectile actor, Selene, a friendly, a dead actor, a cross-world actor, or an attacker whose project status component is absent.

If Selene defeats that hostile while her Exposed instance is active, her precision generator grants `+6 Echo` once for the fatal damage transaction. This is independent of weak-point components, excludes Echo-ability damage, and validates that Selene authored the active Exposed instance. The existing perfect-Deflection `+10` remains a separate one-shot transaction reward.

Presentation should bind to `On Status Changed` or the lifecycle gameplay events. Gameplay must not infer exposure from a VFX or montage.

## Eclipse corruption prototype

The prototype uses the TDD's exact vocabulary and avoids a poison-bar model:

| Exposure | Band | Prototype gameplay contract |
|---:|---|---|
| `0` | None | internal clean state |
| `> 0` to `< 25%` | Trace | information only; no control change |
| `25%` to `< 55%` | Intrusion | semantic band/state hook; no invented global penalty |
| `55%` to `< 85%` | Contest | remedy must be explicitly communicated |
| `>= 85%` | Overwrite Risk | reachable only when both the source and current mission authorize it; emits a mission hook, never kills or hijacks control |

Without both authorizations, exposure is capped at Contest. The core system never reverses controls, delays input, selects dialogue, fakes menus/saves, or forces friendly fire.

The generic `Sov.Status.Apply.Corruption` and `Apply Instant Exposure` paths are intentionally capped at Intrusion because they do not carry a complete remedy and presentation contract. Contest and Overwrite Risk require a validated registered source.

### Author a field

1. Place a Blueprint child of `ASovCorruptionFieldVolume`.
2. Give `Source Spec > Source Id` a stable value unique among fields that can affect the same player, such as `M05.Eclipse.ChamberA.Field01`. Do not ship the prototype default unchanged on multiple volumes.
3. Set continuous rate and/or instant exposure.
4. Set falloff. Linear falloff requires `Outer Radius > Inner Radius`.
5. Enable line of sight and choose the occlusion channel when geometry should protect the player.
6. Define the allowed-target query and band cap.
7. Provide the exact remedy tag and clear player-facing instruction.
8. Provide a presentation profile and a non-distorting accessibility substitute.
9. Declare ordinary/canon checkpoint behavior.
10. Leave `Authorizes Overwrite Risk` off unless mission logic also calls `Set Mission Allows Overwrite Risk(true)` for a designed failure clock or boss phase.

Overlapping fields contribute concurrently from the same elapsed-time sample and clamp once to the highest eligible authored ceiling, so random source-handle order cannot change exposure. Same-frame instant payloads are likewise batched and resolved piecewise by their authored ceilings. A source ID cannot be registered twice on one player.

### Author a remedy

Place a Blueprint child of `ASovCorruptionRemedyVolume`, use the exact source-authored remedy tag, and choose a reduction or full clear. It applies once per entry. Failed early overlaps remain retryable after the persistent ASC becomes ready.

`Apply Remedy`, `Reduce Corruption`, and `Clear Corruption` are also available to authority-owned abilities and objectives. A mismatched required remedy fails closed. Cleansing gameplay exposure does not erase a narrative consequence record.

### Presentation and accessibility

Bind HUD/audio/VFX to:

- `On Corruption Changed`
- `On Corruption Band Changed`
- `On Corruption Presentation Changed`
- `On Overwrite Risk Entered`

The presentation snapshot includes normalized exposure, exact band, most relevant source direction, remedy instruction, profile, and accessibility substitute. `Set Reduced Effects Presentation Enabled` changes only the cues; thresholds, accumulation, and outcomes remain identical.

`On Corruption Remedied` is an authority-side gameplay hook in this prototype. Client presentation should react to replicated `On Corruption Changed` and presentation snapshots; a falling value is the network-safe remedy cue.

## Checkpoint adapter boundary

This slice exposes `Capture Checkpoint State` / `Restore Checkpoint State` for statuses and `Capture Corruption Checkpoint` / `Restore Corruption Checkpoint` for corruption. It does not automatically write these structs into Narrative save data yet.

The future encounter/checkpoint coordinator must:

1. capture semantic state before destroying or resetting the avatar;
2. restore only after the PlayerState ASC is bound to the new avatar (early calls queue safely);
3. use the authored ordinary-versus-canon policy;
4. reconstruct opted-in live corruption sources by stable `Active Source Ids`;
5. wait for streamed encounter volumes to reconcile, then call `Finalize Corruption Source Restore` so saved one-shot sources cannot replay during an arbitrarily slow load;
6. never separately serialize the same corruption attributes through Narrative's generic ASC save list, which would bypass source and canon policy.

Live source IDs are captured independently from aggregate exposure policy, with a separate canon-persistent subset retained in the version-three schema. A canon checkpoint may therefore reset the meter while still asking encounter logic to reconstruct only specifically opted-in persistent sources. Capturing again during a slow ordinary or canon restore carries those still-pending reconstruction IDs forward without weakening their canon policy. Replay guards never expire on a wall-clock guess: each is consumed by its matching source registration or cleared by the explicit finalization call.

All five built-in combat statuses currently clear on checkpoint/retry. Asset overrides may explicitly opt into remaining-duration or full-duration restoration. Death clears every exact live status handle first; an eligible captured semantic record may be reapplied by the later retry coordinator.

## UE 5.7 validation

Build with Live Coding disabled, then run:

```text
Automation RunTests ProjectVelkorran.Campaign.Foundation
Automation RunTests ProjectVelkorran.Campaign.Status
Automation RunTests ProjectVelkorran.Campaign.Corruption
Automation RunTests ProjectVelkorran.Campaign.Selene
```

Minimum PIE checks:

- damage/no-damage/fatal/Guard/Deflection status gating;
- equal, weaker, stronger, and replayed Burn requests;
- Chill at 1/2/3 stacks and checkpoint reconstruction of stack strength;
- Freeze rejection during its recovery-immunity window;
- Device Disabled on an opted-in drone and rejection on an ordinary character;
- exact cleanse, global cleanse, source-filtered cleanse, expiry, death, and ASC/pawn replacement;
- Deflection applies Exposed to the attacker and a Selene-authored exposure kill grants exactly `+6` once;
- two concurrent corruption fields with different caps/rates in both registration orders;
- source exit, occlusion, death, retry, ordinary checkpoint, canon checkpoint, and late ASC readiness;
- Overwrite Risk with neither, one, and both authorization gates;
- reduced-effects presentation with identical numeric exposure, bands, remedies, and mission events;
- listen-server, owning client, observer, and dedicated-server behavior.
