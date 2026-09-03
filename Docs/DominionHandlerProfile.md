# Dominion Handler Enemy Profile

The Dominion Handler is the first concrete **Commander** profile from the TDD. It is not a second damage dealer with a commander costume: while its command link is active, it authorizes a linked Dominion Hound's coordinated Horn Charge. Selene can Sever that relationship, interrupt an active charge, expose the remaining weak points, and earn the command-link Echo reward. The hound keeps Bite and Pounce after Sever, so the counterplay removes coordination rather than switching the enemy off.

This source pass supplies the native Handler character, its server-authoritative order ability, exact linked-Hound selection, and the command-link gate on Horn Charge. Blueprint assets, NPC definitions, StateTree/Behavior Tree assets, montages, sidearm attacks, VO, Niagara, audio, and encounter placement remain content work.

## Player-facing role

| Profile element | Combat meaning | Player answer |
|---|---|---|
| Visible Handler-to-Hound link | The Handler is enabling a specialist command | Prioritize, isolate, or Sever the Handler link |
| Order anticipation | A linked Hound is about to receive Horn Charge authorization | Break line of sight, interrupt, defend, or reposition |
| Horn Charge | Heavy, committed lane pressure | Perfect defense, Deflection, or evade the committed line |
| Successful Sever | Coordinated charge is interrupted and locked out for this link instance | Exploit the weaker formation; Bite and Pounce still threaten |
| Handler death | Command source is gone | Pending Handler orders stop and future charges lose authorization; an already committed Horn Charge completes, and no synthetic Sever reward is minted |

The link and order need a non-color cue as well as Dominion color language: use a readable beam/pulse, a Handler gesture or callout, and a Hound response. Do not communicate the command only with a red or colored flash.

## Native runtime contract

`ASovDominionHandler` derives from the project NPC base and owns exactly one native `USovCommandLinkComponent`. Its command ability is `USovGameplayAbility_DominionHandlerCommandHound`.

| Contract | Native behavior |
|---|---|
| Ability identity | `Sov.Ability.NPC.DominionHandler.CommandHound` |
| Narrative input | `Narrative.Input.Ability1` |
| Execution | Server initiated and server owned |
| Command membership | Candidates come only from the Handler's active command-link participants |
| Specialist lookup | The Hound spec is found by the exact `Sov.Ability.NPC.DominionHound.HornCharge` asset tag |
| Activation | Authority briefly supplies `Sov.State.CommandLink.HoundChargeAuthorized`, opens a private native dispatch scope on the exact Horn Charge instance, calls `TryActivateAbility` for that exact spec handle, then closes both authorizers |
| Range | Handler-to-Hound command distance, not Hound-to-player attack distance |
| Line of sight | Optional Handler-to-Hound Visibility trace, enabled by default |
| Failure | The anticipated Hound is revalidated at issue; rejection consumes no command cooldown and never substitutes an untelegraphed Hound |
| Presentation | Optional montage plus a reliable anticipation multicast and successful-order multicast expose cosmetic hooks |

The Handler never drives a linked actor through `AbilityInputTagPressed`. Narrative activates every spec sharing that input and leaves input state behind until a matching release. It also never uses a broad parent-tag activation request. Exact spec-handle activation ensures an order for Horn Charge cannot accidentally trigger another Hound ability assigned to `Ability1` later. Horn Charge additionally requires both the transient Handler-order tag and a private native dispatch scope. An active link, a generic Hound Ability1 request, or manually adding the authorization tag cannot start a charge.

### Structurally eligible Hound

A command candidate must pass all of these checks when candidates are collected and again immediately before dispatch:

- it is a valid, living linked actor in the Handler's world;
- Narrative team attitude reports it as friendly to the Handler;
- it has an initialized Narrative Ability System Component;
- the Handler's command link is still active;
- the candidate has `Sov.State.CommandLink.Active` and does not have `Sov.State.CommandLink.Severed`;
- it has no stale `Sov.State.CommandLink.HoundChargeAuthorized` contribution before dispatch;
- it is inside the authored Handler-to-Hound command range;
- it has clear Handler-to-Hound line of sight when that option is enabled;
- it owns an inactive ability spec with the exact Horn Charge asset identity.

Candidate discovery is intentionally optimistic. It does not reach into another ASC's actor info to pre-run `CanActivateAbility`, and it does not treat a broad input query as readiness. At issue time, exact `TryActivateAbility` is the definitive Hound target, range, cooldown, state, and attack-configuration check. The Handler does not bypass those rules. If the anticipated Hound fails or loses validity during anticipation, the order fails cleanly without consuming command cooldown. A later AI request may select and visibly anticipate a different Hound; the release frame never substitutes an untelegraphed pack member.

Selection is least-recently-anticipated within the current link instance, then nearest to the Handler, with a stable actor-name tie break. The attempt is recorded when the reliable anticipation cue is sent, whether the Hound later accepts or rejects at issue time. This prevents one repeatedly rejecting Hound from starving the pack while preserving the rule that no untelegraphed replacement may charge. Every continuously eligible Hound receives an anticipated attempt before the first is reused, history resets with the link instance, and a previously attempted Hound remains available when it is the only valid candidate. `LastCommandedHound` remains success-only.

### Command lifecycle

1. AI requests the Handler's `Narrative.Input.Ability1` ability.
2. `CanActivateAbility` verifies authority, the active link, and at least one structurally commandable Hound. This is permission to begin anticipation, not proof that Horn Charge will still pass its own activation checks at issue time.
3. The ability commits, owns the Handler's busy/path-postpone state, starts its optional montage, multicasts the selected Hound's one-shot anticipation cue, and opens native anticipation timing.
4. At the native issue time, the Handler revalidates the exact Hound named by anticipation, adds the transient order tag while the private native dispatch scope is still closed, then opens that native scope only around one exact Horn Charge activation call. Synchronous tag listeners therefore cannot steal the authorization window. The captured link-instance identity is revalidated after those callbacks and after activation. Horn Charge must also acquire or already hold one of the selected player's Narrative attack tokens. At a full budget, Narrative's existing age/proximity rules may reassign an eligible token; otherwise the attempt is rejected.
5. A successful activation starts command recovery and command cooldown, and publishes presentation data for the selected Hound and its resolved attack target.
6. Total dispatch failure cancels cleanly without consuming the command cooldown.
7. Death, fatal state, Poise break, Freeze, device disable, ragdoll, interaction, sequence control, link Sever, or loss of the active-link state cancels outstanding native timers and authority state. The already-sent one-shot anticipation cue is not recalled.

Animation notifies never issue the gameplay order, activate Horn Charge, apply damage, or end the ability. Native timers are the authority source of truth, including on a dedicated server with no ticking AnimInstance.

## Hound behavior with and without a Handler

| Hound move | Unlinked / inactive / Severed | Active Handler link | Who requests it |
|---|---|---|---|
| Bite | Available under its normal rules | Available | Hound AI, `Narrative.Input.Attack` |
| Horn Charge | New activation blocked; an already committed charge is not retroactively canceled by ordinary link deactivation | Available when ordered | Handler command ability only, exact Horn Charge spec |
| Pounce | Available under its normal rules | Available | Hound AI, `Narrative.Input.Ability2` |

Do not let the Hound's periodic attack service request `Narrative.Input.Ability1`. The native Horn Charge requires an active link, the Handler's transient order tag, and the native-only dispatch scope, so a stray Hound-side request now fails closed; removing that request also avoids useless activation attempts. The Handler command ability is the only native owner of the coordinated charge decision.

For this first slice, assign each specialist Hound to one Handler-owned link. The component can represent more complex membership, but the Hound's active/severed GAS gates are participant state rather than a per-ability link-instance token. Shared Hounds across multiple live command links require an explicit multi-link authorization design before production use.

## Suggested high-level AI profile

The TDD prefers StateTree for high-level AI. The same policy can be implemented in the current Behavior Tree/task-service setup while StateTree assets are being integrated.

| State | Entry | Actions | Exit |
|---|---|---|---|
| Acquire | No valid hostile target | Perception, orient, move to support lane | Valid hostile target |
| Support | Target known, no command opportunity | Maintain protected mid-range spacing; use authored sidearm later | Command candidate ready, threatened, or link lost |
| Command | Active link and commandable Hound | Face/focus the fight and request Handler `Ability1` once | Ability completes/cancels |
| Reposition | Poor line, crowding, or close threat | Move to regain sight of a Hound and avoid front-line exposure | Useful support lane recovered |
| Disrupted | Link inactive/Severed or Handler device-disabled | Stop command requests; use ordinary survival/sidearm behavior | Recovery, reset, or death |

The decision layer should query whether the Handler command ability is granted and whether the Handler has a structurally eligible candidate before requesting it. Treat this as an optimistic command opportunity: exact Hound activation at the native issue time is the final readiness decision. Do not loop over arbitrary input tags every service tick. Use an authored move table or explicit task selection so the AI knows which combat intent it is asking for.

Recommended first-pass weighting:

- prefer a command when a linked Hound is alive, has a plausible charge target, and is not already committed to another attack;
- avoid issuing into blocked geometry or immediately after the player has moved behind hard cover;
- use a decision cadence longer than the ability's native anticipation/recovery and respect the native cooldown;
- reduce repeated orders through recent-move weighting instead of random activation of unrelated abilities;
- let melee-slot/formation logic veto a charge if another committed attacker already owns the same lane.

## One-time Unreal Editor setup

Close the editor and perform a full `ProjectVelkorranEditor Win64 Development` build after pulling this source. This pass adds reflected native classes and a native Gameplay Tag; do not use Hot Reload or Live Coding for the first load.

### 1. Create the Handler character Blueprint

1. Create `BP_DominionHandler` with native parent **Sov Dominion Handler** (`ASovDominionHandler`). Reparent an existing Handler prototype if one already owns the final mesh and Narrative setup.
2. Assign the Handler skeletal mesh, AnimBP, Narrative NPC definition/character definition, team/faction, perception configuration, movement, and collision using the same project conventions as other Narrative NPCs.
3. Confirm the inherited **Sov Command Link Component** is present. Do not add a second component in Blueprint.
4. Ensure the actor replicates and its Narrative ASC initializes on authority.
5. Give the Handler an authored weak-point component/zones only if the Handler itself should reveal weak spots on Sever. The command component must include its owner as a participant for this profile, even when the Handler has no authored weak points.

The native class does not create an `NPCDefinition`, `Activity`, StateTree, Behavior Tree, Blackboard, sidearm, or inventory asset. Those are references to project content and must be assigned in the Blueprint/Narrative data.

### 2. Create and grant the command ability

1. Create `GA_DominionHandler_CommandHound` with native parent **Dominion Handler: Command Hound** (`USovGameplayAbility_DominionHandlerCommandHound`).
2. Add it to the Handler's Narrative **Ability Configuration -> Default Abilities**.
3. Preserve `Narrative.Input.Ability1` and the native `Sov.Ability.NPC.DominionHandler.CommandHound` asset identity.
4. Preserve the native active-link requirement; do not replace the command-link component's state ownership with Blueprint loose tags.
5. Assign the optional command montage, section, and play rate.
6. Tune the native issue delay to the visible gesture/callout release, then tune recovery and watchdog around the full animation. Do not add a notify that activates the Hound.
7. Leave the native bot range/frequency and command cooldown as the initial decision values until the real arena, Handler mesh, and Hound speed are tested.

Leave the Handler command Blueprint's inherited GAS **Cost Gameplay Effect** and **Cooldown Gameplay Effect** unset in this slice. The ability still calls `CommitAbility` for the standard GAS lifecycle, while the native success-only timer owns its 5.5-second command cooldown. An authored cost or normal GAS cooldown would be committed during anticipation and would incorrectly charge the Handler when every Hound rejects the delayed order at issue time.

The Handler AnimBP needs a Slot node between its locomotion/state-machine pose and Output Pose. Use the exact slot authored into the command montage, initially a full-body slot such as `DefaultGroup.DefaultSlot`. If the Handler must keep locomotion while signaling later, move to a deliberate layered blend after validating the skeleton mask.

### 3. Author the command link

On each placed Handler instance:

1. Set a stable encounter-unique **Link Id**, for example `KennelA_HandlerLink`.
2. Leave **Starts Active**, **Severable**, **Respect Device Disable Immunity**, and **Can Grant Selene Echo** enabled for the standard profile.
3. Leave **Include Owner As Participant** enabled. This is required for `ASovDominionHandler`: its Ability System Component must receive `Sov.State.CommandLink.Active` before the native command ability can activate. Weak-point reveal on the Handler remains optional and is controlled by whether it has authored weak-point zones.
4. Add the controlled Hound actors to **Linked Actors**. For a spawned pack, call `RegisterLinkedActor` for every Hound after the Handler and Hounds exist. Then, on authority, check `IsCommandLinkActive`; if it is false, call `ActivateCommandLink(Handler)` and handle a failed activation. Pass the Handler itself as command source—using another actor deliberately prevents this Handler from issuing orders and avoids split source/death/Sever attribution. A valid Link Id with **Include Owner As Participant** may already have activated the link at Begin Play, while other runtime authoring can correctly leave it inactive. Call `UnregisterLinkedActor` before a planned removal from the network; a destroyed Hound is unregistered automatically by the native component.
5. Optionally assign an Infinite-duration **Active Link Effect Class** for a small, legible coordination modifier. Do not duplicate the active/severed tags in that effect, and avoid a generic raw damage multiplier that obscures the actual Commander behavior.
6. Keep **Reveal Weak Points On Sever** enabled and author reveal material/zones according to `Docs/SeleneCommandLinkAndWeakPointReveal.md`.

### 4. Configure linked Hounds

1. Grant Bite, Horn Charge, and Pounce from the Hound's Narrative Ability Configuration as described in `Docs/DominionHoundAbilities.md`.
2. Preserve Horn Charge on `Narrative.Input.Ability1` with exact identity `Sov.Ability.NPC.DominionHound.HornCharge`.
3. Preserve Horn Charge's native Active and HoundChargeAuthorized activation requirements. Never grant `Sov.State.CommandLink.HoundChargeAuthorized` from a Gameplay Effect, Character Definition, Behavior Tree, or Blueprint; the tag alone is deliberately insufficient, and the Handler's exact native dispatch owns its complete lifetime.
4. Register the Hound on exactly one Handler link for this profile.
5. Remove Ability1/Horn Charge requests from the Hound's own attack service. Keep Bite and Pounce selection local to the Hound.
6. Confirm an inactive, active-but-unordered, or Severed Hound cannot activate Horn Charge when Ability1 is requested manually. Also confirm manually adding `Sov.State.CommandLink.HoundChargeAuthorized` still cannot start it. Only the Handler command's tag-plus-native-scope handshake may open the exact activation attempt.

### 5. Connect AI and presentation

In the current Behavior Tree implementation, give the Handler a dedicated task/service branch that requests its own `Narrative.Input.Ability1` only when the Handler's command ability and structural candidate query report an opportunity. The issue attempt can still fail because Horn Charge's target, range, cooldown, or state changes during anticipation; that path is expected and consumes no command cooldown. Do not alter the generic Hound attack service to search every granted ability blindly.

The server-owned ability exposes `ReceiveHoundOrderStarted`, `ReceiveHoundOrderIssued`, `ReceiveHoundOrderFailed`, and `ReceiveHoundOrderEnded`. Use those only for authority-local diagnostics; they do not replicate cosmetic work to remote clients. The Handler character exposes `ReceiveHoundHornChargeAnticipation`, a reliable one-shot multicast at wind-up start, and `ReceiveHoundHornChargeOrdered`, the successful-order multicast after the Hound actually accepts its charge. Use them for a short link pulse, Handler callout, Hound acknowledgement, UI marker, or other remote cosmetics. Anticipation cosmetics must self-expire because cancellation does not send a second client multicast to recall them. Do not create persistent state that depends on receiving the successful-order multicast. Treat the selected Hound and charge target as presentation context only. None of these hooks may activate an ability, assign link tags, grant Echo, or decide that a Sever occurred.

When StateTree becomes the encounter standard, preserve the same separation:

- evaluators expose sensed target, active link, and command availability;
- a selection task requests the Handler command ability;
- GAS owns commit/cooldown/cancellation;
- the Handler owns exact Hound dispatch;
- the Hound ability owns target validation, motion, contact, and damage.

## Prototype tuning

Native values are starting points, not balance locks:

| Setting | Initial intent |
|---|---:|
| Maximum Handler-to-Hound command distance | About 2,500 cm |
| Handler-to-hostile bot engagement range | About 2,200 cm; this is not the communication radius |
| Require Handler-to-Hound line of sight | Enabled |
| Order issue delay | About 0.35 s |
| Recovery after order | About 0.45 s |
| Command cooldown / bot decision frequency | About 5.5 s |
| Watchdog | Long enough to include issue plus recovery; about 1.5 s |

The Hound's own Horn Charge minimum/maximum target range remains separate. Increasing Handler range does not increase charge reach, and shrinking charge reach does not change whether a Handler can communicate with a Hound.

## Authority, Sever, and failure rules

Authority owns candidate collection, membership and tag validation, link-instance identity, the native dispatch scope, line-of-sight checks, exact spec activation, cooldown, and command presentation dispatch. Clients may render the replicated result but cannot nominate or activate a Hound.

A first valid Selene Sever transitions the component from Active to Severed. It removes the active-link contribution, applies the severed contribution, cancels an in-progress Horn Charge through the Hound's watched tag, prevents another Horn Charge from activating, reveals unbroken weak points when configured, and can pay Selene's `+12` Echo exactly once for that link instance.

Handler death or destruction deactivates the link and clears its contributions. Loss of the active state cancels a Handler command still in wind-up, preventing a late dispatch. It does **not** retroactively cancel a Horn Charge the Hound already committed; only the explicit Severed transition carries that specialist interruption. Handler death also does not fabricate a Sever transaction, weak-point reveal, or Echo payout. An encounter reset may create a fresh link instance; it must not reuse the old transaction identity.

Any failed command path must clear the Handler ability's timers, montage task, temporary owned tags, candidate references, and delegates. A failed Hound activation must not leave either actor busy and must not start the command cooldown when no order was delivered.

## Verification matrix

First test with empty cosmetic Blueprint children, then repeat after final animation/VFX/audio work. Gameplay must remain identical without presentation assets.

| Area | Test | Expected result |
|---|---|---|
| Native composition | Inspect `BP_DominionHandler` | Exactly one inherited command-link component; Narrative ASC initializes |
| Ability grant | Use `showdebug abilitysystem` | Handler has one command spec on Ability1 with the exact Handler command asset tag |
| Membership | Link one Hound and leave a second nearby but unregistered | Only the linked Hound can be ordered |
| Exact dispatch | Give a Hound another ability using Ability1 | The Handler activates only the exact Horn Charge spec |
| Normal command | Active link, valid Hound/target, range and sight clear | Order anticipates once, chosen Hound starts one Horn Charge, then Handler recovers/cools down |
| Independent attacks | Inactive or Severed link | Bite and Pounce remain usable; Horn Charge cannot activate |
| No self-command | Request Hound Ability1 directly while linked | Native Handler authorization is absent, so Horn Charge does not start |
| Authorization spoof | Manually add `Sov.State.CommandLink.HoundChargeAuthorized`, then request Hound Ability1 | Horn Charge still fails because the private native dispatch scope is closed; no Hound cooldown or token is consumed |
| Anticipated-candidate invalidation | The named Hound becomes busy/invalid during anticipation while another Hound remains valid | The order fails without cooldown; no other Hound charges until a fresh command names and anticipates it |
| Fair selection | Three or more equivalent valid Hounds, including one that rejects at issue, across repeated commands | Least-recently-anticipated rotation gives every continuously eligible Hound a visible attempt before reuse and resets with a new link instance |
| Command range | Move Hound just inside/outside command range | Only the inside candidate is commandable; its player-target range is still checked separately |
| Command sight | Put hard Visibility-blocking cover between Handler and Hound | Command is rejected with sight required; clearing cover restores eligibility |
| Hound target failure | Linked Hound has no valid hostile target | No charge starts and a total dispatch failure consumes no command cooldown |
| Target attacker budget | Fill the target player's budget with fresh, non-stealable tokens | No charge starts, no token is overcommitted, and total dispatch failure consumes no Handler command cooldown |
| Eligible token steal | Make one full-budget token eligible under Narrative's normal age/proximity policy | The slot is reassigned rather than overcommitted; Horn Charge holds the new lease through its commitment |
| Different-target token | Give the selected Hound a token for another player before ordering it toward the current target | The order fails without abandoning the earlier behavior's token; returning it makes the Hound eligible again |
| Attack-token cleanup | Let Horn Charge finish, then repeat with cancellation and Sever | Any token the charge newly acquired is returned on every path; a pre-existing same-target token remains owned by its original behavior |
| First Sever | Sever during order anticipation and during active charge | Handler order cancels; active charge cancels; Horn Charge stays blocked; one Echo/reveal transaction resolves |
| Handler death | Kill Handler before issue, then repeat after the Hound commits | Wind-up order cancels and future charges are unauthorized; an already committed charge completes; no synthetic Sever Echo/reveal is created |
| Wrong command source | Activate a Handler-owned link with another actor as source | The Handler cannot issue orders until a fresh link instance uses that Handler as its command source |
| Hound destruction | Destroy a registered Hound, then reset/reactivate the encounter link | The Hound is removed from replicated membership automatically; surviving members reactivate and the stale reference cannot poison configuration validation |
| Other cancellation | Poise-break, Freeze, disable, ragdoll, or sequence the Handler | Pending order/timers clear with no late Hound activation |
| Missing montage | Remove Handler command montage | Native order timing and cleanup still work |
| Listen server | Observe from host and remote client | One authority order/charge; presentation agrees for both clients |
| Dedicated server | Run server plus two clients, including offscreen actors | Selection, order, cancellation, damage, and cooldown do not depend on local animation or effects |
| Encounter reset | Reset/activate a completed encounter link | Fresh instance can command and later pay one legitimate new Sever transaction |
| Reset during dispatch | Reset/reactivate the link from a synchronous authorization or activation callback | The old command is canceled/rejected and cannot cross into the new link instance |

After content setup, run:

1. `Automation RunTests ProjectVelkorran.Campaign.DominionHandler`
2. `Automation RunTests ProjectVelkorran.Campaign.DominionHound`
3. `Automation RunTests ProjectVelkorran.Campaign.Selene`
4. `CompileAllBlueprints`

## Explicitly deferred

This pass does not author binary Unreal assets, a Handler firearm/sidearm, formation-slot expansion, an arena-specific StateTree or EQS query, player-target handoff between Handler and Hound, Handler cover locomotion, bespoke commander UI, VO, link-beam Niagara/audio, boss immunities, viewmaker discovery, Cipher propagation, converted devices, persistence, or shared-Hound multi-link arbitration.

Those additions should consume the native command, link, Sever, and presentation seams. They must not replace them with Blueprint-only authority, animation-notify damage, broad input activation, or a second Echo/reveal transaction path.
