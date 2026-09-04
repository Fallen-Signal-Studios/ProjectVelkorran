# Tarrik native engineering completion

The approved five-ability roster in `CampaignV2ChangeLog.md` remains authoritative. This change completes native Cinder Slam and Cinderline Requiem and makes Hunger and Sticky Grenade usable without Blueprint gameplay release timers. Values newly introduced below are engineering defaults for tuning, not final balance claims.

## Native behavior

| Ability | Authority fallback | Recovery | Payload |
|---|---:|---:|---|
| Sticky Grenade | 0.35 s | 0.4 s | Existing sticky/fuse/radial/Burn projectile |
| Velkorran's Hunger | 0.35 s | 0.4 s | Existing blade wave/direct/Burn projectile |
| Cinder Slam | 0.55 s | 0.65 s | Heavy Kinetic/Thermal 90 damage, 80 Poise, 450 cm radial, 50% edge falloff; 50 Damage Resistance ward for 2.5 s |
| Cinder Judgement | Existing 0.28 s | Existing 0.45 s | Existing direct hit and controlled blast |
| Cinderline Requiem | 0.45 s | 0.65 s | 100 damage/80 Poise penetrating hit; timed 60 damage/100 Poise lane; shared Cinder Burn |

Echo costs stay 35/50/90 by input slot. Weapon-specific abilities still require the exact granting weapon instance to be wielded and its class allowlisted. Grenade remains universal and character-granted. Native release revalidates identity, equipment and interrupt states. Invalid Tarrik identity or an ongoing weapon equip now fails before Echo is spent.

The shared Tarrik scheduler fences native release/recovery callbacks to the activation which scheduled them. Authority timers and optional early release notifies share one payload gate. Released projectiles and the Requiem lane outlive normal ability recovery. Actor ownership attributes the lane to the source pawn for encounter retry cleanup; it stops if the source ASC changes avatar.

Slam uses the existing Narrative damage execution. Its knockback requires the exact resolved transaction to have broken Poise, the target to remain alive, and no boss/super-armor exclusion. Its ward is a duration Gameplay Effect against the existing DamageResistance attribute, not a new shield system. Same-source wards refresh rather than stack.

Requiem traces from server-controlled eyes and muzzle, using Narrative's configured weapon trace channel. It resolves CharacterVisual ownership, ignores each penetrated character before retracing, and stops at world geometry. A fixed trace-work bound fails closed at the last confirmed point. A muzzle bridge prevents a forward socket from starting beyond nearby cover. No client hit or origin is accepted.

The paid line copies source context and settings into `ASovCinderRequiemLine`. It includes both confirmed endpoints, limits work to 64 nodes, and applies one line packet per ASC across all nodes, separately from the one direct hit. Each blast requires visibility and deduplicates modular bodies. Damage results use a context-scoped native receipt so reentrant damage/healing callbacks cannot attach Burn or knockback to the wrong transaction. Burn reuses Cinder Grenade's definition, retaining same-source refresh and immunity behavior.

## Blueprint migration

1. Keep native `ASovTarrikCharacter`, or ensure the player definition supplies only Tarrik's identity. Keep the established weapon allowlists and grants.
2. Remove Blueprint damage application, radial loops, ward application, projectile spawning and Requiem chain timers from these abilities. The native methods now own them. Never implement generic `ActivateAbility` in a child.
3. Use the existing Echo Started/Local Presentation hooks for montage, camera and audio. Tune native release/recovery delays to match the montage. Server notifies may invoke the release method earlier; duplicate calls are rejected.
4. For Slam, `Receive Cinder Slam Released` supplies authority origin/radius/result count. Use the existing predicted start event and replicated gameplay cues or your cosmetic routing for the orb/blast on remote views.
5. For Requiem, a child of `ASovCinderRequiemLine` can implement `Receive Line Detonation`. The actor replicates node progress and catches up coalesced cosmetic updates. Keep gameplay out of that event. `Receive Cinderline Requiem Released` is an authority ability hook for the initial line/shot.
6. Keep authored damage overrides as plain instant Narrative-damage shells and ward overrides as duration resistance effects. Empty or malformed new Slam/Requiem effect configurations resolve native fallbacks. Projectile art remains optional gameplay configuration.

## Files

- `Public/Abilities/SovGameplayAbility_TarrikEcho.h`: existing classes extended, no parallel ability family.
- `Private/Abilities/SovGameplayAbility_TarrikEcho.cpp`: preserved projectile/Judgement payloads with native timing and source revalidation.
- `Private/Abilities/SovGameplayAbility_TarrikLifecycle.cpp`: shared activation-fenced timers and identity checks.
- `Private/Abilities/SovGameplayAbility_TarrikCinderSlam.cpp`: native radial/ward payload.
- `Private/Abilities/SovGameplayAbility_TarrikCinderlineRequiem.cpp`: authority penetrating trace and copied lane payload.
- `Public/Private/Projectiles/SovCinderRequiemLine.*`: persistent timed lane and replicated cosmetic node progress.
- `Public/Private/Effects/SovGameplayEffect_CinderWard.*`: native duration resistance ward.
- `Private/Combat/SovTarrikPayloadSupport.h`: canonical actor resolution, eligibility, visibility and typed damage receipt integration.
- `Private/Combat/SovCinderLineMath.h`: bounded production line/timing math.
- `Private/Tests/SovTarrikPayloadTestFixtures.*`, `SovTarrikPayloadRuntimeTests.cpp`: real World/GAS fixture coverage.
- `Tests/Portable/SovCinderLineMathTests.cpp`: compiled engine-independent production helper checks.

Paths above are under `Source/ProjectVelkorran` except the portable test and this documentation. The generic `Combat/SovNativeDamageReceipt.h` is shared with Selene's completion work.

## Verification

Compiled and ran the production math test with GCC C++17, strict warnings enabled. It passed 2,102 checks covering endpoints, monotonic node placement, bounded work, invalid/NaN/infinite inputs and timing budgets. This compiles the exact helper used by the Requiem actor and signature validation.

Added three Unreal automation registrations under `ProjectVelkorran.Campaign.Tarrik`:

- Slam: real GAS activation/spending, duplicate/late release rejection, hostile/friendly/occluded/immune filtering, routed damage and resistance ward.
- Requiem: character penetration, friendly pass-through, world cover, one immutable lane, off-axis line damage, per-target dedup and continued chain after ability end/weapon removal.
- Projectile lifecycle: native timer release without a Blueprint payload, duplicate rejection, paid projectile persistence, and source-weapon removal before release.

UE5.7, UHT and runtime automation are unavailable in this workspace. These automation tests were authored and source-reviewed, not executed. Run `Scripts/Validate-Unreal.ps1 -EngineRoot <UE_5.7> -TestFilter ProjectVelkorran.Campaign.Tarrik` with the required plugins installed.

Editor acceptance additionally requires ward expiry/refresh with other resistance effects, boss and super-armor Poise immunity, defeat/death/retry cleanup of released actors, ability cancellation/reentry at each release stage, dedicated/listen-server presentation, close wall/muzzle cases, skeletal hit zones on actual weapon collision channels, late replicated cosmetic updates, and final tuning. The source implementation does not certify these content-dependent gates.
