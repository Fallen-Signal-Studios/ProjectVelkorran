# Replication readiness audit — 14 September 2026

Branch `engineering/replication-readiness` (from `engineering/t3-campaign-ui`, `b30eaa8c`).

## Authority and scope

The August 2026 TDD locks "Launch multiplayer: None" (§1.1) and reserves multiplayer replication scaffolding for a
separate Operations track (§19.5, §20). It also requires that "single-player authority is still explicit so
simulation, replay, and future reuse remain deterministic" (§15.1), that "prediction keys may remain available in
code" (§15.5), and that hit results are not frame-rate dependent (§6.8).

The creator's direction for this pass: *single-player authored, multiplayer-safe where reasonably possible.* The
goal is not multiplayer. It is to stop building foundational gameplay around assumptions that would make a later
2–4 player cooperative PvE combat prototype prohibitively expensive.

Nothing here networks the narrative layer, cinematics, dialogue, consequence/morality, campaign or galaxy state,
save ownership, protagonist handoffs, sessions, matchmaking, or campaign co-op. No online scaffolding was added: every
change is an authority, replication-flag or seam correction that leaves standalone behaviour identical.

Classification:

- **GREEN** — already replication-safe or naturally compatible.
- **YELLOW** — inexpensive foundational fix; made now.
- **ORANGE** — needs real multiplayer implementation; documented, not built.
- **RED** — architectural assumption that becomes expensive to unwind; resolved now where reasonably possible.

Evidence is native code in `Source/ProjectVelkorran` and the project's customized Narrative Pro copy
(`Plugins/Narrativeed3f9374a6eV6`). Authored Blueprints are largely absent from the repository (see
T3 in the backlog reconciliation), so any Blueprint that calls `GetPlayerCharacter(0)`, drives gameplay from an
anim notify, or reads local settings is unverified.

## 1. Assessment by system

### GAS ownership, attributes, effects and tags — GREEN, with fixes

- Player ASC lives on `ANarrativePlayerState` in Mixed mode; NPC ASCs on the character in Mixed mode.
- Health, Shield, Stamina, Poise, Echo and their maxima replicate `REPNOTIFY_Always`; the corruption attribute set likewise.
- Death is the replicated `bIsDead` with an OnRep that applies `State.IsDead`/`State.Fatal` on every role; no multicast.
- Shield, Poise, HealthRecharge and Echo writes are authority-gated; Shield-broken and Poise states use
  `TagAndCountToAll` loose tags, and Poise rebuilds its state on clients from them. Status and corruption components
  replicate with OnRep presentation and server-world-time durations.
- Fixed (YELLOW): `State.Exertion.Exhausted` was a non-replicated loose tag; clients and proxies never saw it.
- ORANGE (pre-existing, observed in automation logs since at least the T2 full run): `Narrative.State.Invulnerable`
  is granted both as a `TagAndCountToAll` loose tag (encounter-director and coordination protection suspensions) and
  through gameplay-effect granted tags (fatal-recovery and companion protection). UE 5.7 keeps one replication state
  per tag and logs "Trying to change existing tag ... replication state" when they meet, so a client's count can
  diverge. Converting the suspensions to a gameplay effect removes it; that touches encounter restore ownership, so it
  is recorded rather than changed here.

### Health, shields, stamina, poise, Echo, status, death/downed/revive

- GREEN: the rules above. Status and corruption are the most network-complete systems in the project.
- Fixed (YELLOW): Exertion movement speeds were applied only on authority, so an owning client would predict with
  Narrative defaults and be corrected constantly.
- Fixed (YELLOW): a client learned of a shield break from the replicated attribute but never presented it.
- Fixed (YELLOW): Field Recovery charges did not replicate; they now reach the owning client with a notification.
- ORANGE: fatal recovery and companion rescue are explicitly `NM_Standalone`; one player's failure reloads the world
  checkpoint; the failure menu is opened from the server-side controller. Co-op needs a downed/revive or party-wipe
  design first (TDD §6.13 has no solo downed state by design).
- ORANGE: stamina costs are direct attribute modifications and regen is server-ticked, so they are not predicted.

### Melee, ranged, weak points, criticals, deflection, sever, command links

- GREEN: every gameplay projectile is spawned and resolved on authority and replicates; damage credit follows the
  original instigator's avatar (including deflected rockets); there is no random critical — hit-zone multipliers come
  from physical materials and set-by-caller values; weak-point breaks and reveals, command-link state and severed
  regions replicate with OnRep rebuilds; the finisher reservation lease is safe for several attackers.
- GREEN: Guard and Deflection are LocalPredicted with authority-decided timing windows, replicated owned tags and
  cosmetic multicasts; Echo costs predict correctly; Echo-generation client RPCs are cosmetic.
- Fixed (YELLOW): the sticky grenade kept collision on proxies, unlike Hunger and the drone rocket.
- Fixed (YELLOW): sever cosmetics used a Reliable multicast although the severed state already replicates; a burst of
  kills could saturate the reliable channel.
- **RED, not fixed — melee net model.** Melee and Finisher are ServerOnly abilities that play their montage through
  the ASC from the server, which the engine does not replay to the locally controlled owner; combo input is buffered
  only where the controller is and never forwarded; charged-release events are not replicated; and `CanActivateAbility`
  refuses on clients. A remote player cannot melee at all today. The fix is a design decision (predicted swing with
  server-validated sweep, or server-run swing with input forwarding and owner montage), and every melee montage,
  notify and combo authored before it is decided raises the cost. See §6.
- ORANGE: melee sweeps sample blade sockets once per tick and blend linearly, so hit shape depends on tick rate
  (this also conflicts with TDD §6.8 in single player; fixing it changes standalone hit shapes, so it is recorded
  rather than changed here). Perfect-guard and deflection windows have no latency allowance. Narrative hitscan trusts
  client target data. Lock-on is local only. Selene's projectile has no client simulation.

### Enemy and boss state; AI targeting and threat

- GREEN: target selection is per perceived actor and filtered by `IsPlayerControlled`, which holds for every
  player's pawn on the server; threat memory is keyed per target; attack tokens live on each target's ASC; enemy
  abilities are ServerOnly, play montages through `PlayMontageAndWait`, and time impacts with timers rather than
  notifies; meshes tick pose on a dedicated server; no native boss or phase state depends on a player.
- **RED, resolved in part — fairness gate on a local viewport.** Offscreen ranged-attack admission projected the
  attacker into the *local* viewport. For any controller that is not local (every client's controller on a server)
  the attacker always counted as offscreen, and warnings are acknowledged only by a local HUD, so ranged enemies would
  never fire at that player. Fixed: a non-local controller is judged from the view its client reports to the server
  with its camera field of view, at a 16:9 frame (a wider real view can only require a warning, never skip one).
  Remaining ORANGE: per-player warning delivery and acknowledgement.
- ORANGE: coordination melee slots are shared encounter-wide and low-resource relief reads only the encounter player.
- ORANGE: the Aurelion sweep scanner and Thermal Fracture are standalone-only and bound to the Tarrik-with-companion-
  Selene pairing.

### Encounters, spawns, waves, boss completion

- GREEN: the encounter director replicates, is always relevant, and decides begin/resolve/fail/retry on authority;
  Mass promotion spawns on authority.
- **RED, resolved in part — one encounter player.** The director records a single `EncounterPlayer` (falling back to
  the first player controller), and every system asked "is this the encounter player?" to mean "is this pawn in the
  fight". Added `IsEncounterCombatant` and `GetEncounterCombatants`; fatal recovery, corruption overwrite, the sweep
  scanner and narrative-cue combat detection now ask the combatant question. `HasEncounterPlayer` keeps its checkpoint
  owner meaning for objectives, the Crucible and retry. The campaign has exactly one combatant, so behaviour is
  unchanged. Remaining ORANGE: the director's own internals (player death fails the attempt, attempt-actor
  attribution, relief and warnings) and a wipe rule.

### Interactions and initiating player; loot and sustain drops

- GREEN: interaction goes through Narrative's `ServerBeginInteract`; the server derives the target and passes the
  interacting pawn to `Interact(APawn*)`, and project terminals record that pawn and its controller.
- GREEN: sustain drops credit any player-controlled killer, claim once on authority, grant to the overlapping pawn,
  and replicate `bClaimed`. Client pickup physics is correct as is: the engine synchronizes a replicated actor's
  simulate-physics flag and applies physics replication targets.
- ORANGE: drops, medical caches, corruption countermeasures and the Echo bypass gate are first-come shared resources
  (a design question); level actors with gameplay-visible state do not replicate; interaction prompts evaluate on
  authority only.

### Player controller and pawn assumptions

- GREEN: per-player progression and protagonist snapshots live on the PlayerState with authority gates; combat
  components are per pawn; protagonist identity is a tag on each ASC; there is no mutable static state and no
  `TActorIterator` over player characters.
- **RED, resolved — per-machine settings deciding server gameplay.** Incoming-damage scale, exertion cost, evade
  invulnerability window, melee aim assist, projectile lead, combo input-buffer assist, interaction hold scale, enemy
  recovery cadence and companion rescue permission were read from `GetSovSettings()` — this process's user — inside
  authority-side gameplay. Added two seams on `UNarrativeGameUserSettings`: `GetSovPlayerSettings(PlayerContext)` for
  a player's assist and accessibility preferences, and `GetSovSessionSettings(WorldContext)` for shared difficulty.
  All ten call sites route through them; both resolve to the local settings today. Remaining ORANGE: attack-token
  difficulty is still read through `UArsenalStatics::GetGameplayDifficultyLevel()`; incoming-damage scaling is still
  applied only for a local controller until a per-player preference source exists.
- Fixed (YELLOW): projectile lead required a *local* controller, so it silently did nothing for a remote player's
  shooter even though the payload resolves on authority.
- Fixed (YELLOW): `StageCampaignLoad` refused networked sessions without a reason, so login accepted a player whose
  campaign pawn could never initialize; it now fails login with an explicit message. Campaign initialization polling
  is authority-only.
- **RED, not fixed — campaign state on the controller gates combat.** Corruption permission, Resonance, companion
  leadership and rescue, reward proofs and several world actors read mission and protagonist state from the
  pawn's `ASovPlayerController` campaign component, which is per controller and not replicated. Moving campaign state
  is out of scope; the cheap next step is a single combat mission-context seam (§6).

### Resonance

- **RED, resolved in part — world-singleton lookup.** `USovResonanceComponent::FindActive` returned the first player
  controller's pawn component. Added `FindForActor(World, Context)`, which resolves from the pawn, its companion
  leader, and only then the single standalone player; all five callers now pass the actor involved. Resonance itself
  remains standalone-only and pair-owned (ORANGE).

### RPC and authority boundaries; client-only decisions

- GREEN: the project declares no Server RPCs; its NetMulticast and Client RPCs are cosmetic except where noted.
- Fixed: the offscreen fairness gate (above) was the only native server decision waiting on client-only presentation.
- ORANGE: local presentation bound to server-only damage delegates — controller rumble
  (`SovHapticFeedbackComponent`), damage captions (`SovFrontendComponent`) and Narrative damage numbers
  (`NotifyDealtDamage`) — would stay silent for remote players. Rule going forward: do not add new local-player
  presentation to `OnDamageResolvedAsTarget`/`AsSource`; route it through the owning controller.

### Root motion and montages

- ORANGE: Evade applies a constant-force root-motion source on authority only and Sprint sets the CMC sprint flag
  on authority only; Narrative's movement component overwrites that flag from each client move, so remote sprint
  cannot work and remote evades will correct. Carry, traversal and transit move player pawns server-side.
- GREEN: enemy movement abilities drive velocity and launches on the server; the transforming weapon visual's direct
  `Montage_Play` calls are cosmetic with owner-prediction tokens.

### Determinism and synchronization

- GREEN: no gameplay randomness outside server-only AI selection; dismemberment cosmetics use a server seed;
  remaining times use server world time; projectile flight uses fixed sub-steps or swept movement.
- ORANGE: melee sweep tick-rate dependence (above); blood-decal roll seeded from local death time (cosmetic).

## 2. Changes implemented

| Id | Class | Change | Files |
|---|---|---|---|
| R1 | RED | Encounter combatant API; membership consumers moved off checkpoint ownership | `SovEncounterDirector`, `SovFatalRecoveryComponent`, `SovCorruptionComponent`, `SovAurelionSweepScanner`, `SovNarrativeCueComponent` |
| R2 | RED | Offscreen fairness gate judges a non-local controller from its reported view | `SovEncounterCoordinationComponent`, `SovEncounterCoordinationPolicy` |
| R3 | RED | Resonance coordinator resolved from the involved actor | `SovResonanceComponent`, `SovResonanceTargetComponent`, `SovProtectionInterceptReceipt`, `SovCompanionComponent` |
| R4 | RED | Player- and session-scoped gameplay settings seams, ten call sites | `NarrativeGameUserSettings`, `NarrativeAttributeSetBase`, `NarrativeCombatInputBuffer`, `NarrativeBotAttackSelection`, `PlayerInteractionComponent`, `SovExertionComponent`, `SovGameplayAbility_Exertion`, `SovGameplayAbility_Melee`, `SovAimAssist`, `SovFatalRecoveryComponent`, `SovCompanionCommands` |
| Y1 | YELLOW | Exertion movement speeds on every role | `SovExertionComponent` |
| Y2 | YELLOW | Exhausted tag replicates | `SovExertionComponent` |
| Y3 | YELLOW | Shield break presents on clients | `SovShieldComponent` |
| Y4 | YELLOW | Field Recovery charges replicate to the owner | `SovFieldRecoveryComponent` |
| Y5 | YELLOW | Sticky grenade proxies leave collision to authority | `SovCinderStickyGrenadeProjectile` |
| Y6 | YELLOW | Sever cosmetics unreliable | `SovDismembermentComponent` |
| Y7 | YELLOW | Projectile lead for any possessed player shooter | `SovAimAssist` |
| Y8 | YELLOW | Networked sessions refused with a reason; authority-only campaign init poll | `SovPlayerController` |

## 3. Verification

Mac, UE 5.7, `ProjectVelkorranEditor` Development, 14 September 2026:

- Editor target builds.
- Portable policy suites: 45 of 45 pass (`Scripts/Test-NativePolicies.py`).
- `ProjectVelkorran.Replication` and `ProjectVelkorran.Campaign.Encounter.Coordination`: 19 of 19 pass.
- Full `ProjectVelkorran` automation: 643 of 645 pass. Both failures are outside this closure:
  - `Validation.GameplayCuesStillResolve` is X1 (`/Game/Cues` absent), as before.
  - `Campaign.PlacedNPC.DefinitionFallbackAndExistingOwnership` fails only from the missing `SciFi_Drone_1` skeleton
    asset log (X2) and passes run alone; the T2 full run showed the same spillover on a different PlacedNPC test.
- Negative controls: each of the ten fixes was reverted and rebuilt. Every targeted test failed on its own
  assertion: exertion speeds, exhaustion tag state, proxy shield break, owner-only charges, proxy grenade collision,
  sever channel, remote-shooter assistance, remote-view admission, combatant checkpoint rule, and Resonance
  possession. The source was then restored and matched the pre-control diff.

Tests added:

- `ProjectVelkorran.Replication.*` (9): proxy exertion speeds, replicated exhaustion tag, proxy shield-break
  presentation, owner-only charges and notification, proxy grenade collision, unreliable sever channel with
  replicated state, settings seams and remote-shooter aim assistance, encounter combatants, Resonance resolution.
- `ProjectVelkorran.Campaign.Encounter.Coordination.RemotePlayerViewGatesRangedAdmission`.
- Portable `SovEncounterCoordinationPolicyTests`: view-frame boundaries.

These simulate the remote side with non-authority roles and non-local controllers in one standalone world. No
listen-server or client world was run; that harness is the first item in §6.

## 4. Remaining blockers for 2–4 player co-op PvE

1. Melee and Finisher net model (RED).
2. Sprint and Evade prediction; stamina prediction.
3. Campaign state on the controller gating combat systems (RED; seam recommended).
4. Encounter director internals: combatant deaths, wipe rule, attribution, relief, per-player warnings.
5. Fatal recovery: downed/revive or party-wipe design; client presentation routing.
6. Local presentation bound to server-only damage delegates.
7. Lock-on, aim and hit-data validation; perfect-defense latency allowance.
8. Level actors and interaction prompts without replicated state; standalone gates on combat-side world actors.
9. Narrative plugin: player-0 helpers (Tales, HUD, NPC tethering, level sequences), single-player world save,
   unsynchronized ragdoll pose, possible owner-side projectile double spawn.

## 5. Deliberately deferred

Narrative, cinematics, dialogue, consequence records, campaign and galaxy state, save ownership and identity,
protagonist handoffs and protagonist-as-companion, Resonance and Aurelion pairing mechanics, sessions, matchmaking,
dedicated-server builds, campaign co-op, and any Operations mode (TDD §20).

## 6. Recommended next replication slice (after the Aurelion vertical slice)

1. **Two-client combat harness.** A test-only, non-campaign GameMode and arena (per TDD §20.4 isolation) run as a
   listen server plus one client in automation, so replication claims are proven by real replication.
2. **Player action net model.** Decide and implement predicted melee and finisher (owner montage, forwarded combo and
   release input, server-validated sweep at fixed sample times), client-set sprint with server stamina, and predicted
   evade root motion.
3. **Combat mission-context seam** replacing direct campaign-state reads in corruption, Resonance, companions and
   rewards.

Before then, the one decision worth making early is the melee net model, because melee content authored against the
current server-only model is the most expensive thing to rework.
