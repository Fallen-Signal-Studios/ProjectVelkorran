# Settings, targeting, fatal recovery and native preflight

This pass extends the existing Narrative settings, Enhanced Input routing, camera manager, GAS damage result and save ownership. It does not add a competing input, attribute, camera or checkpoint store.

## Implemented native paths

| Concern | Native owner and behavior |
| --- | --- |
| Persistent settings | `USovGameUserSettings` is selected in `DefaultEngine.ini`. A complete settings transaction validates finite bounded values before persistence and one change event; recursive writes are rejected. Existing Narrative difficulty setters route into this owner. |
| Difficulty and assistance | Story/Standard/Veteran/Sovereign/Custom apply bounded incoming damage and enemy recovery factors. Separate timing, exertion, input buffer, melee/ranged aim and comfort settings survive preset changes. Sovereign unlock comes from validated completion of the terminal campaign mission. |
| Input | The existing Enhanced Input mapping supports semantic modifier routes, toggle aim/guard/sprint/modifier, physical release and cancellation. Several physical routes may hold one semantic action. Route revisions and ownership epochs prevent a release callback from releasing a fresh same-ASC activation. Suppression clears held input and native combat buffering. Existing remapping, mouse/gamepad sensitivity, inversion and deadzone settings persist immediately. |
| Camera | Existing Narrative camera behavior remains authoritative. Optional native target lock requires an authored `Sov.Target.HardLock` actor tag, a living hostile target, distance, line of sight and navigation relationship. Occlusion has a bounded grace period; death, possession loss and cinematic/input suppression clear the lock with a typed notification. Framing never translates the player. Manual look temporarily suppresses auto-camera assistance. Spring-arm collision remains enabled. |
| Comfort | Native consumers disable active/new camera shakes and reduce camera lens effects. The corruption comfort flag remains separate from gameplay corruption exposure and is available to authored presentation. |
| Fatal recovery | `USovFatalRecoveryComponent` immediately releases held actions and owns input suppression. A 0.05-second decision gives the committed damage result time to identify environmental/canonical fatalities. One verified reachable living companion can rescue once per encounter attempt when encounter permission and difficulty allow it. Rescue restores 35% Health, zero Shield, half Stamina and full Poise through the existing snapshot resource path, with one second of owned protection. |
| Retry | Non-rescuable player death and verified required-companion defeat use the existing encounter retry, then the existing checkpoint/most recent valid autosave if needed. Retry begins after 0.75 seconds, below the TDD's three-second initiation limit. A failed partial rescue restore also retries. The component reports a terminal failure if no valid recovery snapshot exists. |
| Recovery exclusions | `ASovRecoveryExclusionVolume` is a non-damaging authored exclusion region for lethal hazards, duels or canon failure. Saved active state restores in Narrative's World phase. Recovery rejects capsule overlap with its conservative world bounds, even before physics overlap updates. Disabled exclusions stop rejecting recovery. Checkpoint clearance also requires an unblocked capsule and a walkable floor. |
| Diagnostics | `USovDiagnosticsSubsystem` records bounded local semantic events, 512 records maximum, only with opt-in and outside Shipping builds. Data contains semantic IDs and numeric outcomes, no account IDs, dialogue text or arbitrary file paths. Explicit export writes beneath `Saved/Diagnostics`; there is no network delivery. Disabling recording clears retained records. |

All fatal decision, effect-removal/application, state-notification, input-release, ability-cancel, resource-restore and retry-fallback boundaries capture and recheck the recovery epoch, ASC, avatar and controller. Cleanup retires ownership before effect callbacks and only removes its exact effect/input contribution. A callback that replaces or rebinds the player cannot make the old recovery flow mutate the new owner.

## Native preflight

`SovValidateCampaign` still accepts the explicit shipping mission manifest:

```text
UnrealEditor-Cmd ProjectVelkorran.uproject -run=SovValidateCampaign -Missions=/Game/Missions/DA_M01.DA_M01,/Game/Missions/DA_M02.DA_M02 -ShippingValidation
```

Supply the complete mission list in real use. The example is abbreviated. The commandlet checks campaign asset identity and successor references, globally unique consequence and relationship-memory IDs, and existing pawn/profile configuration. It visits the dependency closure and runs native melee graph, corruption profile, evidence, narrative cue and Narrative dialogue graph validators. Evidence supporting IDs must resolve within that closure. Shipping validation additionally requires string-table text and rejects excluded legacy content roots.

Assets reached only through dynamic string loads must be explicitly included with `-AdditionalAssets=/Game/Path/Asset.Asset,...`. The commandlet cannot infer those loads. Cue dialogues use their owning mission for native campaign condition/event validation when one is authored. Unscoped dialogue assets receive graph and intrinsic validation; authored conditions/events still require correct mission context.

## Validation and remaining practical limits

Portable C++ policy tests cover settings boundaries, nonfinite data, difficulty gating, look behavior, bounded diagnostics and the complete fatal rescue admission truth table. Unreal automation sources cover atomic settings application and reentry, portable snapshot privacy, semantic toggle/chord/cancel routing, same-ASC release reentry, physics/hostility/navigation target admission, occlusion and unpossess loss, owned recovery protection, physical/exclusion clearance, canonical fatal rejection, effect-removal/application rebind and state-callback detachment.

The container has no Unreal Engine toolchain. These engine automation tests and the commandlet are authored, not executed here. A UE 5.7 build, UHT, those automation suites and actual campaign map playthroughs remain required before shipping.

Aim assistance is deliberately bounded: melee corrects eligible attack orientation; ranged assistance applies view friction around a valid hostile target. These consumers do not provide universal target tracking, guaranteed hits or geometry traversal. Designer-authored animations, weapon sockets, target eligibility, camera composition, input assets, HUD/settings widgets, string tables, subtitles, VO and corruption visuals still need their editor integration and perceptual validation. Reduced corruption effects must be consumed by that authored presentation.

Native save loading preserves the current device/account settings. The portable gameplay snapshot is a separately requested import, and cannot silently replace accessibility choices or diagnostic consent. A locked Sovereign preset is rejected on explicit import until campaign completion validates the local unlock. Platform account/cloud conflict handling, certified platform storage behavior, hardware input testing and shipping performance/accessibility certification require the actual platform and engine environment.

## Additional native accessibility consumers

Three opt-in settings now have concrete consumers; all default to off and remain local accessibility preferences during portable gameplay imports.

- **Automatic sprint:** the existing Narrative Move action submits its actual magnitude. At or above 0.85, while grounded and gameplay input is available, the controller acquires the existing semantic Sprint hold. It never synthesizes movement or bypasses the granted sprint ability's Stamina/interrupt admission. Stopping movement, falling, aiming, modal suppression or a cinematic releases automatic ownership; a separate held manual sprint source is preserved. An exhausted/cancelled sprint is not repeatedly restarted while the same movement request stays held.
- **Aim snap:** on the rising edge of the real GAS aiming state, Targeting may rotate the camera toward one visible living hostile within eight degrees and 50 metres. The rotation is capped at eight degrees and happens once per aim activation. Allies, dead actors, opaque cover, unavailable possession and suppressed camera input are rejected. It does not require the restricted elite/duel hard-lock permission.
- **Projectile lead:** Velkorran's Hunger's native launch uses a solved constant-velocity intercept when its configured gravity scale is zero. The lead horizon is at most 0.6 seconds; aim correction is at most eight degrees. Both current and predicted points must pass line-of-sight checks. No valid bounded intercept means the original launch direction is preserved. The existing projectile collision/damage path remains authoritative. Ballistic sticky-grenade lead and arbitrary Blueprint-owned projectile launches are **not implemented** by this consumer and require a gravity-aware launch integration before claiming universal projectile-lead support.

The added portable policy suite checks the automatic-sprint admission matrix and 6,333 combined finite/intercept boundary cases. Engine regression sources check manual/automatic hold ownership, no automatic toggle latch, suppression, preservation of accessibility preferences, and real hostile/cover aim-target admission. Engine execution remains outstanding.

## TDD 13.9 native/platform work still missing

These requirements are not complete and must not be relabelled as editor-only work:

| Requirement | Native work still required | Widget/content and platform validation |
| --- | --- | --- |
| Vibration intensity by channel | Add a single owned haptic request path over Unreal's supported force-feedback APIs, bounded per-channel intensity settings, cancellation/priority behavior, and actual combat/interaction/cinematic producers. No project haptic channel router or native producers exist in this pass. | Author device-appropriate feedback assets, expose channel controls, verify accessibility-off behavior and test each supported controller/platform. |
| HDR calibration | Integrate supported display capability/output queries and a transactional HDR calibration apply/revert path over the real UE settings/rendering interfaces. Existing gamma/monitor settings are not an HDR calibration implementation. | Supply calibration imagery and readable controls; verify actual HDR displays, output transitions, luminance/tone mapping and fallback behavior. |
| Screen-reader support | Integrate platform-supported speech/accessibility services with menu, evidence and dialogue semantics, focus events and interruption/cancellation. Choice timing must wait for reader completion. No native screen-reader adapter, narration queue or completion contract exists in this pass. | Mark up real widgets with labels, roles and focus order; author evidence summaries and speaker/choice/timer announcements; test supported OS readers and platform APIs. |
| Menu focus memory/navigation wrap | The semantic gameplay input work does not implement menu navigation persistence. Extend the existing NarrativeCommonUI `UNarrativeActivatableWidget`/CommonUI focus behavior and `UNarrativeMenu`; validate restoration and wrap against the actual menu assets instead of adding another UI framework. | Wire each real menu, test controller/keyboard navigation and reader focus together. |
| High-contrast/color/outline/navigation options | Add renderer/widget consumers and persisted choices tied to the actual shipping HUD, outline materials and navigation presentation. No unused setting placeholders were introduced. | Author and validate non-color cues, color presets, outline materials, contrast and readable layout at required scales. |

Platform capability and certification cannot be proven in this source-only environment. These are concrete remaining engineering/integration tasks, in addition to the editor asset wiring listed above.
