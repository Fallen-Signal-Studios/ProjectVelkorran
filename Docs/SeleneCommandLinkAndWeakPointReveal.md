# Selene command-link Sever and weak-point reveal

This slice implements the TDD's first concrete **Sever** contract: Axiom Null Pulse can break live, authored command links selected by its native pulse; the server interrupts that network's coordinated behavior, Selene receives `+12` Echo once for the validated hostile transition, and each participating enemy temporarily reveals its remaining weak points in red. `ASovDominionHandler` is the first native enemy profile that actively consumes this contract by ordering a linked Hound's Horn Charge. The Sever path does not treat Shield break, Device Disabled, damage, an animation event, or merely activating Axiom as proof that a link was severed.

The current Axiom implementation is the project-specific expression of the TDD's `GA_Selene_Disrupt` role. Its native Shield collapse, recharge suppression, and eligible-device shutdown remain independent of the command-link transaction. [AxiomNullPulse.md](AxiomNullPulse.md) documents the native charge/release and Blueprint migration.

## Runtime contract

`USovCommandLinkComponent` is a server-owned coordinator placed on the commander, handler, relay, or device that owns one encounter link. It has three states:

| State | Participant state | Meaning |
|---|---|---|
| `Inactive` | Neither command-link tag is contributed | The network is not operating; no Sever transaction exists |
| `Active` | `Sov.State.CommandLink.Active`; optional active-link Gameplay Effect | The authored network is operating and may be severed |
| `Severed` | `Sov.State.CommandLink.Severed`; active effect removed | This link instance completed its one authoritative Sever transition |

An active link has a generated `LinkInstanceId`. A successful `Active -> Severed` transition produces one `FSovCommandLinkSeverResult` with a fresh `TransactionId`, stable authored `LinkId`, owner/source/severer identity, Echo eligibility, and a snapshot of affected participants. Repeating the request returns `AlreadySevered` with the original transaction instead of minting another reward.

`ResetCommandLink` clears the previous state and, when **Starts Active** is enabled, creates a fresh link instance. Use it only for an authoritative encounter reset or respawn. Destroying or killing the link owner/command source deactivates the link without manufacturing a Sever, Echo award, or reveal.

### Sever outcomes

| Result | Typical cause | Echo / reveal |
|---|---|---|
| `Invalid` | Direct helper call outside native release authorization, missing identity/source weapon, invalid actor/world, inactive Axiom, or node outside the released cone/range/visibility | None |
| `Inactive` | The component exists but this link instance is not active | None |
| `NotHostile` | Selene is not hostile to any included/registered participant in the active link | None |
| `Immune` | **Severable** is disabled, or the owner/source carries `Sov.Status.Immunity.DeviceDisable` while that policy is respected | None |
| `AlreadySevered` | A second request targets the same severed instance | None; original transaction is returned for observation only |
| `NewlySevered` | The server validated the first request against an active, severable instance | Eligible Selene receives `+12`; affected actors begin their authored weak-point reveal |

These are native transaction outcomes. Blueprint should observe link/result presentation events, not call the Axiom helper to obtain them. Hostility and identity are checked on authority. The Echo generator consumes the transaction ID before testing resource availability, so a duplicate cannot become payable after Selene's meter changes. This is the only Selene award allowed while Axiom owns `Sov.State.EchoAbility.Active`; other Echo-ability damage still cannot generate precision Echo.

At native defaults, Axiom spends `30` and one successful hostile link Sever immediately returns `12`, for a net change of `-18`. Each distinct eligible link instance owns its own reward identity; the examples below use a single link. At exactly `30` Echo the expected result is `12`, and at a full meter the expected result is `82`.

## One-time Unreal Editor setup

Close the editor and perform a full `ProjectVelkorranEditor Win64 Development` build after pulling this source. The pass adds reflected component, enum, struct, zone, and replicated-state fields plus native Gameplay Tags; do not use Hot Reload or Live Coding for the first load.

### 1. Author the command node

1. For a Dominion Handler, use `ASovDominionHandler` as the Blueprint parent and keep its one inherited `Sov Command Link Component`; do not add a duplicate. A different commander, relay, or device can own exactly one explicitly added `USovCommandLinkComponent`. Do not add one to every linked Hound.
2. Ensure the owning actor replicates. The component is replicated by default, but a non-replicated owner cannot deliver its state to clients.
3. Give the command node and linked enemies the correct Narrative team identity. The command-link component fails closed unless Selene is hostile to at least one actual included/registered participant. A relay without an ASC can be selected by the native pulse and validate hostility through its participants. An ASC-bearing command node must itself be hostile to Selene under the pulse filter; do not assume neutral ASC-bearing actors bypass that filter. An empty relay cannot create a rewardable link.
4. Set **Link Id** to a stable, encounter-unique name such as `KennelA_HandlerLink`. Never derive reward identity from a display name or actor label.
5. Leave **Starts Active** enabled when the network should operate at Begin Play. If activation depends on encounter scripting, disable it and call `Activate Command Link` on the server when the encounter enters the linked phase.
6. Leave **Severable** enabled for an ordinary Axiom target. Disable it for a deliberately immune phase.
7. Leave **Respect Device Disable Immunity** enabled unless this encounter explicitly allows Sever to bypass `Sov.Status.Immunity.DeviceDisable`.
8. Leave **Can Grant Selene Echo** enabled for a genuine hostile tactical link. Disable it for tutorial, decorative, or repeatable utility networks that must not generate Echo.
9. Enable **Include Owner As Participant** when the component owner should receive link tags/effects and have its own weak points revealed. It is required for `ASovDominionHandler`, because the Handler's command ability requires `Sov.State.CommandLink.Active` on the Handler's Ability System Component. Disable it only for a different, pure coordinator whose gameplay never consumes participant state. At least one included/registered participant must be hostile to Selene for the resulting transaction to be Echo-eligible.
10. For a placed encounter, assign every controlled enemy in **Linked Actors** on the placed command-node instance. The property is instance-authored. For a dynamically spawned pack, call `RegisterLinkedActor` on authority for every member after the node and members exist. Then check `IsCommandLinkActive`; only when false, call `ActivateCommandLink(CommandNode)` on authority and handle a failed activation. A valid Link Id with an included owner may already have activated at Begin Play, while other runtime authoring can remain inactive. Call `UnregisterLinkedActor` before a planned removal from the network; actor destruction unregisters that member automatically.
11. Optionally assign **Active Link Effect Class** to an Infinite-duration Gameplay Effect containing the actual coordination bonus. The component owns the applied handles and removes its contribution when the link is severed, deactivated, unregistered, or torn down. Do not put the permanent Severed state in this effect; the component owns both link-state tags.
12. Leave **Reveal Weak Points On Sever** enabled and start with **Weak Point Reveal Duration** at `5.0` seconds.

If the command source is a different actor from the component owner, call `Activate Command Link(CommandSource)` from authority. Passing no source uses the owner. Assigning actors or calling activation from a client is invalid.

### 2. Connect Axiom Null Pulse

Use the native `USovGameplayAbility_SeleneAxiomNullPulse` parent. Follow [AxiomNullPulse.md](AxiomNullPulse.md) when migrating an existing Blueprint:

1. Grant the ability from Axiom's actual `UWeaponItem` and set its exact `AllowedWeaponClasses`. The player must have Selene's explicit identity; the granting Axiom must remain wielded through release.
2. Remove Blueprint gameplay from `Echo Ability Authority Committed`: charge timers, pulse traces/overlaps, Shield/status applications, direct Sever and Echo calls, and normal ability-ending logic. Native code now performs that work once.
3. Keep Blueprint montage, camera, sound, VFX and UI presentation. `Receive Axiom Pulse Released` reports the authoritative release geometry and result counts after gameplay; `OnCommandLinkSevered` still supplies the individual link transaction for presentation/encounter reactions. Neither hook may apply the pulse again.
4. Keep the native Gameplay Effect defaults. Native code uses safe damage and bounded-duration suppression/device-disable shells even if legacy serialized overrides remain. Device Disabled defaults to eligible drones, so losing a Handler link does not stop unrelated organic Hound attacks.
5. Input release or the native full-charge timer releases the pulse using the authority clock. Range, cone and line of sight select actual command-node owners; author collision/query participation and link membership so the command node is discoverable. The node must be in the pulse itself; merely hitting a linked Hound does not discover and sever its Handler.
6. Let native recovery finish the ability. Do not call `Finish Echo Ability` at charge start or immediately on authority commit.

`Try Sever Axiom Command Link` is an internal step authorized for the current node by native release. Direct calls from Blueprint, notifies, Gameplay Cues, or presentation delegates return `Invalid`. Its component remains idempotent: a later legitimate pulse reaching an already-severed instance cannot restart reveal or repay Echo. Shield/recharge/device results remain separate from the successful link transaction.

### 3. Author weak-point reveal zones

Every linked actor that should display a red target needs exactly one `USovWeakPointComponent`. The reveal uses the same authored zones that already validate precision damage; it does not invent a weak point from a socket name.

For each `Weak Point Zones` entry:

1. Keep a stable unique **Zone Id** and at least one gameplay matcher in **Hit Bones** and/or **Physical Materials**.
2. Set **Reveal Attach Point** to the bone or socket that should carry the decal. Leaving it empty falls back to the first non-empty **Hit Bones** entry. A physical-material-only/static-mesh zone may leave both empty and attach at the selected mesh component's origin; author its relative transform explicitly.
3. If multiple visible meshes are candidates, add a unique Component Tag to the intended mesh and copy it into **Reveal Mesh Component Tag**. Otherwise the first visible mesh that supports the resolved attachment is used.
4. Adjust **Reveal Relative Transform** in bone/socket space. Rotate the decal projection toward the surface and offset it slightly if clipping occurs.
5. Keep **Reveal Decal Size** tight around the weak spot. The axes are projection depth, width, and height in centimeters; start around the native `12 x 24 x 24` and tune against the final animation.
6. Use **Reveal Decal Material Override** only when a zone requires a different mask. Otherwise it inherits the component's **Weak Point Reveal Decal Material**.

For the Dominion Hound, start with a zone such as `Horn` whose hit matcher and reveal attachment use the real horn bone/socket. Ensure the Hound is a replicated link participant with its initialized Narrative ASC: actors without an ASC can still reveal weak points, but cannot receive the state tags that authorize or interrupt behavior. Horn Charge requires the durable `Sov.State.CommandLink.Active` relationship plus a transient Handler-order authorization during activation; a successful link Sever removes the durable authorization, applies `Sov.State.CommandLink.Severed`, and cancels a charge already in progress. Bite and Pounce neither cancel nor become blocked, so the Hound becomes less coordinated rather than inert.

The Handler's command ability is the sole decision owner for Horn Charge. Do not request Ability1 from the Hound's generic attack service just because the active-link tag is present. See `Docs/DominionHandlerProfile.md` for exact-spec activation, Handler/Hound Blueprint setup, and the complete enemy-profile test matrix.

### 4. Create the red reveal material

Create a material such as `M_WeakPointReveal_Decal`:

1. Set **Material Domain** to `Deferred Decal` and choose a decal blend mode that writes the channels used by the material in the project's renderer.
2. Add a Vector Parameter named exactly `WeakPointRevealColor`. Feed it to Base Color and, if desired, a controlled Emissive contribution. The component's native default is a saturated red `(1.0, 0.015, 0.01, 1.0)`.
3. Build a soft local mask for the weak-spot shape and feed it to Opacity. Multiply that mask by the `Decal Lifetime Opacity` material expression so the component's timed fade works.
4. Keep the projection opaque/readable enough to survive combat lighting, but do not replace the enemy silhouette with a full-body red wash.
5. Assign the material to **Weak Point Reveal Decal Material** on the enemy's component. The component temporarily enables decal reception on the character's relevant mesh components and restores their prior setting when presentation ends.

The implementation intentionally uses small bone-attached decals rather than `SetOverlayMaterial`. Unreal's mesh overlay is whole-mesh, cannot isolate an authored bone, and is already used by Shield/interaction presentation. A second overlay would overwrite that channel. Tight decal volumes also prevent most projection onto nearby geometry; if one animated region still slides or bleeds, use a dedicated socket, smaller volume, or a zone-specific proxy/Niagara presentation rather than taking over the Shield overlay.

## Authority, replication, and presentation

The server owns link registration, activation, reset, Sever, state tags, active-link Gameplay Effect handles, Echo, and reveal timing. `FSovCommandLinkReplicationState` replicates the state/revision/link identity and last Sever identity together, while `LinkedActors` replicates the membership snapshot. Clients may observe `OnCommandLinkStateChanged` and `OnCommandLinkSevered`; neither delegate grants gameplay.

On the first successful Sever, the server calls `RevealWeakPoints` on every affected participant that owns a weak-point component. `FSovWeakPointRevealState` replicates a serial, synchronized server end time, and reveal instigator. Each non-dedicated client reconstructs one attached decal for each currently unbroken zone and calculates the remaining duration from GameState server time. This gives late-relevant clients the correct remaining window without replaying Echo or Sever.

The reveal:

- shows only unbroken authored zones;
- extends, rather than shortens, an already active reveal;
- refreshes after a zone breaks or the Narrative character appearance changes;
- clears when the timer expires, the target dies, or weak points reset;
- remains valid gameplay state even if a presentation material/socket is missing, although no decal can render in that authoring-error case.

`OnWeakPointRevealStateChanged` is a local presentation notification. UI, audio, or Niagara may observe it, but must not decide whether a zone exists, break it, or add Echo.

## Gameplay Tags and events

| Contract | Use |
|---|---|
| `Sov.State.CommandLink.Active` | Server-contributed state on active participants |
| `Sov.State.CommandLink.Severed` | Server-contributed state after this link is severed; cancels watched specialist abilities |
| `Sov.State.CommandLink.HoundChargeAuthorized` | Transient server-only proof around one exact Handler-issued Horn Charge activation; never author it into content |
| `Sov.Event.CommandLink.Severed` | Gameplay event sent to the severing actor for the first successful transition |
| `Sov.Echo.Source.CommandLinkSever` | Source tag written by `USovEchoComponent` for the validated `+12` award |

Use `OnCommandLinkSevered` or the gameplay event for presentation and explicit encounter reactions. Do not implement a second Blueprint state machine that independently removes effects, assigns the tags, or grants Echo.

## Validation checklist

After the full editor build, compile and save the command-node, hound, weak-point, material, and Axiom ability assets, then run `CompileAllBlueprints`.

Test in Standalone first, then repeat network-sensitive cases with a remote Selene in two-player listen-server PIE and, when available, dedicated-server PIE:

| Case | Expected result |
|---|---|
| Active link begins | Included owner and registered live participants have `Sov.State.CommandLink.Active` and one optional effect contribution |
| First hostile Axiom Sever | Returns `NewlySevered`; active tag/effect are removed; Severed tag appears; specialist attack cancels; Selene gains exactly `+12` |
| Later legitimate pulse on same instance | Native link transaction returns `AlreadySevered`; no second Echo, reveal restart, event, or effect removal |
| Direct helper / blocked geometry | External helper call returns `Invalid`; a node outside the released cone/range or behind blocking geometry never Severs |
| Inactive / immune / invalid target | Appropriate non-success enum; no Echo and no weak-point reveal |
| Ordinary Axiom target without a link | Shield/recharge/device payload may resolve; no Sever or `+12` |
| Link node without Shield | A valid authored link may still Sever; Shield amount is not the Sever predicate |
| Full and threshold Echo | `100 -> 70 -> 82` and `30 -> 0 -> 12`; transaction cannot pay again after later meter changes |
| Command source death/destruction | Link becomes Inactive, pending Handler wind-up cancels, contributions clear, and no synthetic Sever/Echo/reveal occurs; a Horn Charge already committed is not retroactively canceled without Sever |
| Encounter reset | A fresh link instance activates; a later legitimate Sever can award once for that new instance |
| Hound behavior | An active Horn Charge cancels on Sever and cannot restart; Bite/Pounce remain eligible subject to their normal rules |
| Handler command | Active Handler link orders exactly one eligible linked Hound Horn Charge; an unlinked nearby Hound is never selected |
| Sever during order | Pending Handler order cancels before issue, or an already active Horn Charge cancels; no late timer reactivates it |
| Red reveal | Every client sees decals only on each participant's unbroken authored zones for the synchronized duration |
| Reveal break/expiry | Breaking a revealed zone removes its decal; remaining zones persist; all decals fade and restore mesh decal settings at expiry |
| Relevancy / appearance | A late-relevant client sees only the remaining reveal time; mesh/appearance changes rebuild decals without duplicating gameplay |
| Friendly/neutral link attempt | Component returns `NotHostile`; no state transition, reveal, or Echo reward |
| Multiple links sharing a participant | Each coordinator removes only its own tag/effect contribution; one link's reset does not mint or repay another transaction. Do not share a specialist Hound across Handler links in this first command profile |

Also run:

1. `Automation RunTests ProjectVelkorran.Campaign.Foundation`
2. `Automation RunTests ProjectVelkorran.Campaign.Selene`
3. `Automation RunTests ProjectVelkorran.Campaign.DominionHandler`
4. `Automation RunTests ProjectVelkorran.Campaign.DominionHound`
5. `Automation RunTests ProjectVelkorran.Campaign.AxiomNullPulse`
6. `CompileAllBlueprints`

## Explicitly deferred

This slice does **not** reflect, redirect, retarget, or transfer ownership of physical plasma/projectile actors. Selene's current Deflection resolves an eligible incoming damage transaction by negating its routed damage/Poise, paying Stamina, and granting Deflection Echo. Adding physical projectile reflection requires a separate server-owned projectile contract for collision consumption, velocity/target reassignment, faction/instigator attribution, damage transaction ownership, and duplicate-hit prevention.

Also deferred are command-link discovery through the viewmaker, Cipher progression chains, converted turrets, cross-link propagation, mark/exposure rewards, precision chains, checkpoint persistence for broken weak points, and bespoke boss phase rules. This implementation supplies the authoritative seam those later features can use without fabricating their outcomes now.
