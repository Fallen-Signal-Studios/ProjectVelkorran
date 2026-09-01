# Transforming Weapon Visuals

`ASovTransformingWeaponVisual` adds staged mechanical draw and stow presentation to Narrative weapons without replacing Narrative's replicated wield state, equipment rules, ability grants, or save format. Ordinary `AWeaponVisual` classes continue attaching immediately.

The transforming visual retains one replicated weapon actor and one skeletal weapon asset across every phase:

| Phase | Physical state | Presentation |
|---|---|---|
| `Holstered` | Holster socket | Collapsed pose |
| `Drawing` | Holster socket | Selene reaches for the weapon |
| `Deploying` | Wield socket | Blades extend |
| `Ready` | Wield socket | Extended pose; combat allowed |
| `Retracting` | Wield socket | Blades collapse in hand |
| `Stowing` | Wield socket | Collapsed hold before transfer |

The server owns phase changes, timers, and socket commits. Clients reconstruct their animation and material position from the replicated phase, phase start time, duration, starting extension, and transition serial. Animation notifies may request an earlier handoff on authority, but native timers always finish the transition if a notify is missing. Reversing during deployment or retraction continues from the current blade extension instead of snapping to an endpoint. Moving the same transforming weapon between two valid wield sockets is serialized through retract, holster, draw, and deploy rather than jumping directly between sockets.

## Verity asset requirements

Verity should use one skeletal mesh with rigidly weighted hilt and blade assemblies. Its deploy animation ends in the extended pose and its retract animation ends in the collapsed pose. Do not create separate collapsed and extended Narrative weapon items or swap the primary mesh in `BPHandleWield` / `BPHandleHolster`.

| Suggested asset | Purpose |
|---|---|
| `SK_Verity` | One weapon skeleton with blade hinge/slide bones |
| `AS_Verity_Holstered` | Optional collapsed idle or pose sequence |
| `AS_Verity_Deploy` | Collapsed-to-extended sequence |
| `AS_Verity_Ready` | Optional extended idle or pose sequence |
| `AS_Verity_Retract` | Extended-to-collapsed sequence |
| `AM_Selene_Verity_Draw` | Character draw montage |
| `AM_Selene_Verity_Stow` | Character stow montage |
| `AM_Verity_Deflection` | Optional weapon-skeleton spin montage for Deflection |

The weapon sequences must use Verity's weapon skeleton. Stable animation endpoints should match the transition endpoints exactly. The character AnimBP must expose the montage slot used by the draw and stow montages.

## Editor setup

1. Close Unreal and perform a full `ProjectVelkorranEditor` build after pulling the reflected native classes. Do not use Live Coding for the first build.
2. Create `BP_VerityWeaponVisual` from **Sovereign Transforming Weapon Visual**.
3. Assign `SK_Verity` to `Weapon Mesh`. Assign the matching local mesh only if Selene uses a separate first-person weapon presentation.
4. Assign the four weapon animation slots. `Weapon Holstered Animation` and `Weapon Ready Animation` are optional. If they are empty, the system holds the appropriate transition endpoint.
5. Assign `Character Draw Montage` and `Character Stow Montage`. If Selene's local mesh uses incompatible montage assets, also assign the optional `Local Character Draw Montage` and `Local Character Stow Montage` overrides.
6. Tune `Draw Attachment Delay`, deploy/retract play rates, fallback durations, and `Holster Attachment Delay After Retract` to the authored animation.
7. Populate the `Draw`, `Deploy`, `Retract`, and `Stow` cue slots with optional Niagara, sound, socket, relative transform, volume, pitch, and late-playback limits.
8. Optionally assign `Transformation Material Parameter`. The scalar is driven from `Collapsed Material Value` to `Extended Material Value` during deployment and reversed during retraction.
9. Assign `BP_VerityWeaponVisual` to Verity's Narrative `Weapon Visual Class`.
10. Keep using Verity's normal Narrative `Holster Attachment Configs` and `Wield Attachment Configs`. The transition system stages when those existing configurations are physically applied.

`bUseNativeSingleNodeWeaponAnimation` should remain enabled when Verity's weapon mesh has no competing weapon AnimBP. Disable it if a child Blueprint owns weapon animation through a custom AnimBP, then use `Weapon Transition Phase Changed` to drive that graph.

The optional Deflection spin is the custom-AnimBP path. Assign `Weapon Deflection Montage` (and an optional first-person override), put its Slot in the Verity weapon AnimBP, assign that AnimBP to both weapon meshes, and disable native single-node animation. The custom graph must then own the holstered/deploy/ready/retract presentation as well. Deflection starts the montage only while the visual is `Ready`; owner prediction is immediate, authority multicasts it to observers, cancellation blends it out, and normal recovery lets it finish.

For direct weapon swaps, have Narrative pass through its empty/holstered wield state before requesting the next weapon. This gives the outgoing transforming weapon ownership of its stow montage and overlay until its holster handoff, then lets the incoming weapon install its own overlay and draw cleanly.

## Timing and optional montage handoffs

The native settings are the authoritative watchdog:

- Drawing commits the wield socket after `Draw Attachment Delay`.
- Deployment lasts for the deploy sequence length divided by its play rate, or `Fallback Deploy Duration` when no usable sequence exists.
- Retraction uses the same rule with the retract settings.
- Stowing commits the holster after `Holster Attachment Delay After Retract`.

For a precisely authored montage, an authority-side notify can call the matching commit function with the serial emitted by `Weapon Transition Phase Changed` when that montage phase began:

- `Commit Wield Attachment(Expected Transition Serial)` at the grip frame.
- `Commit Holster Attachment(Expected Transition Serial)` during `Stowing`, when the collapsed weapon reaches the back.

The serial token rejects a late notify left over from a superseded montage. Simulated-client notifies are cosmetic and are rejected by these functions. Do not grant abilities, change Narrative wield state, or deal damage from a cosmetic AnimBP notify.

## Gameplay and collision contract

Narrative still grants weapon abilities when its replicated wield state changes. The transforming visual applies both existing transition gates while a transient phase is active:

- `Narrative.State.Weapon.Equipping`
- `Narrative.State.Weapon.BlockFiring`

All `UNarrativeCombatAbility` classes now block activation while `Equipping` is present. The gate remains through deployment and is removed only in `Ready` or `Holstered`. Transforming weapon collision primitives are also unavailable outside `Ready`, and their melee collision cache is refreshed after deployment completes.

## Cosmetic Blueprint hooks

- `Weapon Transition Phase Changed` (phase, normalized progress, transition serial)
- `Wield Attachment Committed`
- `Holster Attachment Committed`

Use these for additional blade-energy curves, mechanical movement, local camera treatment, or persistent presentation. The built-in cue slots already suppress historical one-shots when a client becomes relevant too late in a phase.

## Verification

Run these checks in Standalone, listen-server PIE with two clients, and dedicated-server PIE:

1. Verity begins collapsed on Selene's back.
2. Draw keeps it on the back until the grip frame, transfers once, deploys once, and enables attacks only at `Ready`.
3. Stow retracts in Selene's hand, transfers only after collapse, and finishes holstered.
4. Rapid draw/stow reversal settles on the latest requested state without a second socket jump.
5. Repeated Narrative replication callbacks do not replay audio, Niagara, or inherited wield/holster events.
6. Owner first-person and remote third-person presentation agree.
7. Joining or becoming relevant during every phase reconstructs the correct socket, animation time, and material value without replaying stale one-shots.
8. Death during transition clears the gameplay gate and settles around the socket that was already physically committed, without teleporting the weapon toward a pending target. If that character revives, the visual resumes toward Narrative's latest semantic wield target. Nonlethal ragdoll keeps the authoritative timer running so recovery cannot desynchronize Narrative's semantic wield state from the physical socket.
9. Saving/loading restores Narrative's stable wield endpoint rather than saving a transient cosmetic timer.
10. Deflection plays one Verity spin immediately for the owning player and once for observers; cancellation blends it out without affecting gameplay, while a normal recovery lets it finish.
11. An ordinary non-transforming weapon still attaches immediately and behaves exactly as before.

Useful runtime check:

```text
showdebug abilitysystem
```

If Verity uses the native transition path and moves sockets but does not animate, verify the weapon sequences use Verity's skeleton and native single-node animation is enabled. If a custom weapon AnimBP owns transitions or Deflection, disable native single-node animation and verify the appropriate Slot exists in both weapon-mesh graph paths. If Selene's character montage does not play, verify its Slot exists in both the third-person and local character AnimBP paths.
