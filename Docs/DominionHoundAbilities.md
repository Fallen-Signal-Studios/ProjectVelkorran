# Dominion Hound Attack Abilities

The Dominion hound package provides three server-authoritative Narrative/GAS attacks:

- `USovGameplayAbility_DominionHoundBite`: a close-range Standard attack that selects among three authored bite variants on `Narrative.Input.Attack`.
- `USovGameplayAbility_DominionHoundHornCharge`: a Handler-authorized Heavy charge with continuous swept hit detection on `Narrative.Input.Ability1`.
- `USovGameplayAbility_DominionHoundPounce`: a snapshot-targeted Heavy leap with continuous swept hit detection on `Narrative.Input.Ability2`.

These are integral NPC attacks. Grant them from the hound's Narrative **Ability Configuration** rather than a weapon item, and do not add ammo requirements. Damage, Poise damage, movement, hit validation, recovery, and cancellation are native authority responsibilities; animation is presentation.

## One-time editor setup

Close the editor and perform a full `ProjectVelkorranEditor` build after pulling the implementation. The change adds reflected native classes and native Gameplay Tags, so Live Coding is not suitable for the first load.

Create Blueprint children of the native abilities:

| Suggested asset | Native parent | Purpose |
|---|---|---|
| `GA_DominionHound_Bite` | `Sov Gameplay Ability Dominion Hound Bite` | Bite variants, trace, timing, damage, Poise pressure, and cooldown |
| `GA_DominionHound_HornCharge` | `Sov Gameplay Ability Dominion Hound Horn Charge` | Charge montage, native movement, swept hit detection, damage, and recovery |
| `GA_DominionHound_Pounce` | `Sov Gameplay Ability Dominion Hound Pounce` | Pounce montage, native ballistic movement, swept hit detection, damage, and recovery |

Add all three Gameplay Ability Blueprints to the hound's Narrative **Ability Configuration -> Default Abilities**. The native defaults deliberately use distinct input tags:

| Ability | Input tag | Identity tag | Guard class |
|---|---|---|---|
| Bite | `Narrative.Input.Attack` | `Sov.Ability.NPC.DominionHound.Bite` | Standard |
| Horn Charge | `Narrative.Input.Ability1` | `Sov.Ability.NPC.DominionHound.HornCharge` | Heavy |
| Pounce | `Narrative.Input.Ability2` | `Sov.Ability.NPC.DominionHound.Pounce` | Heavy |

Do not remap all three abilities to `Narrative.Input.Attack`. Narrative activates every exact-matching ability spec for an input tag, and its general bot attack query can select the first matching spec. Keeping the tags unique lets a StateTree choose exactly one move.

Horn Charge is deliberately different from the other two moves: its native activation requires `Sov.State.CommandLink.Active`, the transient `Sov.State.CommandLink.HoundChargeAuthorized`, and a private native dispatch scope supplied only around the Handler's exact activation call. The tag alone cannot authorize the move. The Dominion Handler orders it by the exact `Sov.Ability.NPC.DominionHound.HornCharge` ability-spec identity. Do not configure the Hound's own periodic attack service to request Ability1. `Docs/DominionHandlerProfile.md` describes the command owner and encounter setup.

Because this exact dispatch bypasses the normal Hound Behavior Tree attack-token task, Horn Charge also reserves one of its selected target's Narrative attack tokens before it commits. At a full attacker budget, Narrative's normal token-steal rules may reassign an eligible token; otherwise activation is rejected. A token the Hound already held for the same target is reused. A Hound holding another target's token is not silently retargeted; its owning AI behavior must return that token first. The resulting lease is held through wind-up, movement, impact, and recovery unless Narrative reassigns it; losing the lease before payload completion cancels the attack. If this activation acquired the token, native `EndAbility` returns its unchanged lease on every success and cancellation path. Horn Charge does not return a same-target token that another Hound behavior already owned.

The native damage effect is also Blueprintable if a project-specific Gameplay Cue or metadata layer is needed. Preserve its instant `UNarrativeDamageExecCalc` execution and leave damage magnitudes to the ability's SetByCaller payload. Do not add a second Blueprint damage effect.

## Skeleton, sockets, and authoritative pose

Create the following sockets on the hound skeleton:

- `MouthSocket` at the center/front of the bite volume.
- `HornSocket` at the damaging tip of the horn.

If the ability Blueprint exposes a different socket name, either restore the native name above or set the property to the authored socket. A missing socket uses the native fallback offset, which is useful while integrating art but should not be the shipped setup.

Sockets must exist on the character's animation-driving skeletal mesh, not only on a cosmetic attachment or a separate Narrative appearance actor. On that authoritative mesh, set **Visibility Based Anim Tick Option** to **Always Tick Pose and Refresh Bones**. This is required so server-side traces follow the current mouth and horn pose when the mesh is hidden, offscreen, or running on a dedicated server.

Verify the mouth and horn transforms in a dedicated-server session as well as PIE. A client-only AnimBP can look correct while server collision remains in reference pose if this setting is wrong.

## Animation Blueprint and montages

Assign hound-skeleton montages in the ability Blueprint defaults:

- Add the three standard bite animations to the Bite ability's variant array. Give each variant its own montage, optional start section, play rate, impact delay, and post-impact recovery.
- Assign the horn charge animation to the Horn Charge montage slot.
- Assign the pounce animation to the Pounce montage slot.

The Bite ability selects among configured variants and avoids immediately repeating the last choice when more than one valid variant is available. It does not require all three animations to function; leave an unwanted entry unconfigured instead of duplicating an animation merely to fill the array.

In the hound AnimBP's AnimGraph, insert a **Slot** node between the locomotion/state-machine pose and the final output pose. Its slot name must match the montages, for example `DefaultGroup.DefaultSlot`. A full-body slot is the safest initial setup. Use a layered blend only after confirming the hound rig has a useful upper/lower-body division and that the attack pose still drives the mouth and horn bones.

Montage playback does not release gameplay payloads. Native authority timers own impact timing, traces, movement, recovery, and cleanup, so the abilities remain deterministic without an AnimBP or montage. Animation notifies may play cosmetic audio, dust, saliva, camera shake, or Niagara, but must not:

- apply damage or Poise damage;
- start or stop a gameplay trace;
- launch or translate the hound;
- finish, cancel, or reactivate an ability.

Tune each native impact/wind-up value to the matching contact frame. If a montage changes later, update its native timing in the ability Blueprint as part of the same content change.

## Attack behavior

### Bite

Bite is the hound's dependable close-range Standard pressure tool. At its native impact time, authority sweeps from the mouth socket through the configured bite reach and applies one unified Narrative damage result to the first valid hostile target. The per-activation hit ledger prevents that target from taking duplicate damage when a sweep begins or ends inside the same body.

The three bite animations are presentation variants of the same gameplay role. Individual timing can differ, but keep their reach, classification, and damage close enough that the random selection does not secretly change player-facing rules. If materially different bite behaviors are desired later, make them separate abilities with explicit StateTree selection.

### Horn Charge

Horn Charge is a readable Heavy commitment. It snapshots a charge direction after its wind-up, advances through native authoritative movement, and continuously sweeps the horn path. It does not home around corners after commitment. World collision stops the charge, and the hit ledger prevents repeated damage while overlapping a target.

The ability can begin only while the Hound is a participant in an active command link and the Handler's tag-plus-native-scope handshake is in progress. A successful command-link Sever cancels an active Horn Charge and blocks another one for that link instance. An inactive/unlinked Hound cannot self-authorize a charge, and manually adding the transient tag is still insufficient. Handler death deactivates the link and therefore removes authorization for a **new** charge, but it does not apply the explicit Severed interruption to a charge already committed, nor fabricate a Sever or Selene Echo reward.

When native movement is enabled, the Hound must be grounded when activation is attempted. Remove root-motion translation from the montage. The montage may animate legs, head, horn, anticipation, and recoil, but it must not also move the capsule. If an encounter intentionally uses authored montage root motion, disable the ability's native movement option and validate that the server owns and replicates the resulting motion. Never run both translation systems together. Keep Horn Charge's impact delay at or above `0.05 s`; this guarantees a real server-owned wind-up and lets Handler dispatch verify acceptance before any charge payload can complete.

### Pounce

Pounce is a Heavy gap closer. It snapshots a valid destination after its wind-up, computes an authoritative ballistic launch, and performs continuous swept hit detection over the leap. It is not a homing attack: player movement after commitment can cause it to miss. The ability ends through its native travel/recovery rules and restores any movement settings it temporarily changed.

As with Horn Charge, keep montage root-motion translation disabled while native movement is enabled. A montage should pose the jump and landing around the capsule motion, not move the character a second time.

## Prototype tuning

Native values are safe prototype starting points, not balance locks. Override them on the three ability Blueprints after testing against the actual hound mesh and encounter spaces.

| Setting | Bite | Horn Charge | Pounce |
|---|---:|---:|---:|
| Gameplay role | Close pressure | Committed lane attack | Gap closer / punish |
| Guard classification | Standard | Heavy | Heavy |
| Suggested damage band | 14-18 | 28-32 | 24-28 |
| Suggested Poise pressure | 10-14 | 35-40 | 28-35 |
| Useful target range | Up to about 400 cm | About 375-1,800 cm | About 500-1,600 cm |
| Suggested cooldown | About 0.8 s | About 3.0 s | About 4.0-5.0 s |
| Commitment | Short impact/recovery | About 0.45 s wind-up plus charge/recovery | About 0.4 s wind-up plus leap/recovery |

Keep Bite Standard and both gap closers Heavy. This preserves the shared defense language: ordinary Guard handles Bite normally, Heavy attacks exert greater Guard/Poise pressure, and a correctly timed perfect defense or Selene Deflection can still answer all three. Do not mark Horn Charge or Pounce Unblockable merely to make them threatening; tune telegraph, tracking commitment, damage, and Poise first.

## StateTree selection

Split selection ownership between the Hound and the Handler. The Hound's StateTree or Behavior Tree may request Bite and Pounce from target distance and cooldown availability. Horn Charge is requested only by the linked Handler's command ability: `Sov.State.CommandLink.Active` establishes the relationship, while the transient Handler-order tag and private native dispatch scope jointly authorize one exact activation attempt.

A practical first-pass policy is:

| Decision owner | Situation | Requested input/spec | Expected move |
|---|---|---|---|
| Hound | Target inside Bite range | `Narrative.Input.Attack` | Bite |
| Hound | Target beyond Bite range, valid landing route, pounce ready | `Narrative.Input.Ability2` | Pounce |
| Handler | Linked Hound is structurally eligible for a coordinated attack | Exact `Sov.Ability.NPC.DominionHound.HornCharge` spec | Horn Charge if its own target/range/cooldown/state checks pass at issue |
| Either | No owned move is valid or ability is cooling down | None | Reposition, flank, or wait |

Do not activate the generic attack input and hope Narrative resolves competing specs, and do not add Ability1 to the Hound's periodic attack service. Narrative can activate every exact-matching input spec; the Handler instead selects the exact Horn Charge ability handle on authority.

Before requesting a locally owned move, check that the ability is granted, off cooldown, within its authored minimum/maximum range, and not blocked by death, fatal damage, Poise break, Freeze, ragdoll, interaction, sequencing, or an existing attack. Face/focus the target during wind-up. Once Horn Charge or Pounce commits, let the native ability own movement until it ends; competing AI move requests should remain postponed.

Treat distance thresholds as overlapping decision bands rather than exact rings. The overlap lets encounter logic prefer a move based on line of approach, ally spacing, player state, and recent attack history without creating dead zones. Add decision cooldown and move-history weighting instead of randomly activating unrelated abilities.

## Authority and damage rules

All target acquisition, target validation, movement, traces, hit-ledger updates, damage application, recovery, and cooldown commitment run on authority. Clients receive replicated character movement, montage playback, and GAS state. The Blueprint presentation hooks execute on the server-owned ability; use replicated Gameplay Cues or presentation actors when remote clients need an additional effect. Dedicated servers must complete every gameplay path without constructing Niagara, audio, decals, camera effects, or a local AnimInstance.

Each hit must travel through the single Narrative damage execution so Health, Shield, Guard, Poise, Deflection, attribution, death, loot, and combat-sustain rewards see one consistent result. Valid victims are unique, living hostile ASCs. Self, allies, neutral actors, dead actors, and repeated components belonging to an already-hit ASC are ignored.

World geometry is gameplay-relevant. Bite cannot reach through a wall. Horn Charge stops at a blocking surface rather than translating through it. Pounce must respect the configured collision and landing path. The continuous charge and pounce sweeps must remain reliable at low frame rates; do not replace them with a single overlap at an animation notify.

Ending or canceling an ability must clear native timers, stop only movement owned by that ability, restore changed CharacterMovement values, end montage tasks, clear temporary gameplay tags, and prevent any delayed payload from firing. Death, Poise break, Freeze, device disable, and other configured blockers must not leave the hound sliding or damage-active.

## PIE verification matrix

Run the matrix with an empty/cosmetic-free ability Blueprint first, then repeat with final montages and effects. Damage should not depend on art.

| Area | Test | Expected result |
|---|---|---|
| Ability grant | Inspect the hound with `showdebug abilitysystem` | Three specs exist and exactly one spec owns each of Attack, Ability1, and Ability2 |
| Link gate | Request Horn Charge while unlinked, inactive, active-but-unordered, Handler-ordered, and Severed | Only the active link plus Handler tag-and-native-scope case may activate; Bite and Pounce remain independent |
| Command ownership | Run the Hound attack service with an active link but no Handler order | Horn Charge never self-activates |
| Authorization spoof | Add the transient order tag manually and request Ability1 | Horn Charge still does not start, and it consumes neither cooldown nor attack token |
| Charge preconditions | Attempt native-movement Horn Charge while grounded, falling, and swimming | Only the grounded Hound accepts the order; failed attempts consume no charge cooldown or attack token |
| Handler order | Issue the linked Handler command with a valid target | Authority selects the exact Horn Charge spec and starts one charge |
| Target attacker budget | Fill the player's budget with fresh, non-stealable tokens, then free one slot | Horn Charge is rejected while full and becomes eligible after a slot is available; total rejection consumes no Handler command cooldown |
| Token reassignment | Fill the budget with a token that Narrative's age/proximity policy permits this Hound to steal | Narrative reassigns rather than overcommits the slot, and loss of the old lease is visible to its former controller |
| Token cleanup | Complete, cancel, and Sever a Handler-issued charge | A token newly reserved by Horn Charge is returned exactly once; an older Hound token for the same target is left owned by its original behavior |
| Bite variants | Trigger at least 20 close attacks | Every configured montage can play; no immediate repeat occurs when another valid variant is available |
| Bite timing | Test all three montage timings | Contact aligns with the native impact delay and damage occurs without notifies |
| Hit ledger | Leave a large target overlapping a multi-frame sweep | One ability activation applies damage to that target's ASC exactly once |
| Multiple components | Sweep a skeletal target with several bodies | The target receives one hit, not one per component or bone |
| Team filtering | Place allies, neutral actors, the hound itself, and hostiles in the sweep | Only living hostile ASCs take damage |
| Walls and cover | Attack a player behind thin and thick world geometry | Bite does not reach through cover; charge stops; pounce respects blocking collision |
| Guard | Use ordinary Guard against all three attacks | Bite resolves as Standard; Horn Charge and Pounce resolve as Heavy with their authored Poise pressure |
| Deflection | Time Selene Deflection against each contact | The shared defense pipeline recognizes the classification, negates/reduces damage as configured, consumes defense resources, and grants the intended Echo once |
| Miss window | Sidestep after charge/pounce commitment | The hound does not home back onto the player and can miss cleanly |
| Low frame rate | Repeat charge/pounce at 15-20 FPS or with artificial hitching | Continuous sweeps still catch crossed targets once; no tunneling or duplicate hits |
| Cancellation | Interrupt wind-up, travel, impact, and recovery with death, Poise break, Freeze, or forced cancellation | No delayed damage; timers/tags clear; owned movement stops; changed movement values restore |
| Re-entry | Reactivate each ability after normal end and after cancellation | No stale hit ledger, last movement, timer, or montage task affects the next activation |
| Root motion | Compare native movement with root motion disabled, then intentionally test the alternative mode | Exactly one system translates the capsule; no doubled distance or client/server divergence |
| Listen server | Test with two players observing and defending | Authority applies one hit; both clients see the same movement/montage without duplicate gameplay |
| Dedicated server | Run server plus at least two clients, including an offscreen hound | Socket traces, movement, Guard/Deflection, damage, and cancellation match listen-server behavior |
| Late relevance | Become relevant during charge or pounce | Replicated movement/state reconstructs without replaying damage or cosmetic one-shots |

If Bite or Pounce never begins, first confirm the correct Gameplay Ability Blueprint is in the Hound's Ability Configuration, the NPC has completed Narrative ASC initialization, the Hound AI is requesting the matching exact input tag, the target is hostile and inside the authored range, the ability is off cooldown, and no blocking gameplay state is active. If Horn Charge never begins, additionally confirm that the Hound is registered on one active Handler link, has the active-link tag without the severed tag, and is being ordered through the Handler's exact-spec command path rather than the Hound's input service.
