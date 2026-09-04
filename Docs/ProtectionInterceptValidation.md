# Protection intercept: producer and validation

`USovProtectionInterceptReceipt` is a native proof for Reformation Drone Gunfire. It does not infer protection from receiving damage. A living hostile AI must be focusing a different living character friendly to Tarrik. The actual shot must first hit Tarrik. Repeating the identical weapon-channel trace after excluding only Tarrik and known character/weapon visuals must first hit that exact intended ally farther along the ray. Normal shooter exclusions, trace radius, trace complexity and physical-material query settings are preserved. Arbitrary owned or attached props, projectiles, other characters and walls remain blockers.

`USovGameplayAbility_ReformationDroneGunfire::FireNextBurstShot` creates the proof before its existing damage application. The base weapon helper arms it against that exact outgoing effect context and subscribes only for the synchronous typed damage resolution. The existing SourceObject, SourceAbility, instigator, hit and damage execution are preserved. Exact context, source ASC/avatar, target ASC/avatar and committed transaction are required. Applied Health/Shield damage or an accepted Guard/Deflection defense qualifies; zero-damage perfect Guard is therefore valid. Rejected invulnerability/channel immunity produces no accepted receipt. Periodic and Echo damage cannot qualify. The receipt closes after the application, including when nothing is awarded.

`USovTarrikEchoGenerationComponent` consumes the proof once and awards 15 through the existing Echo component, at most once per enemy source per five seconds. Five seconds is exposed prototype tuning, not a new canonical TDD value. Full-meter interceptions still refresh activity and consume the source cooldown. Encounter boundaries and ASC replacement reset cooldown scope; consumed transactions remain replay fences. The Drone burst uses an epoch check after damage/reward callbacks so a cancelled or reactivated ability cannot inherit the old shot's timer.

## Automated checks

Run the engine build first using [UnrealValidation.md](UnrealValidation.md), then the automation prefix `ProjectVelkorran.Campaign.Echo.Protection`:

| Test | Real runtime behavior exercised |
| --- | --- |
| `Geometry` | World ray and sphere sweep; exact AI focus; intended ally/team/liveness; cover before/behind Tarrik; ally-owned crate; Tarrik-owned attached prop; intervening alternate ally. |
| `DroneProducer` | Real native Gunfire activation and trace, real GAS damage; nested same-source/same-target damage with a different context; original weapon/ability context preservation; replay rejection; same-source cooldown, independent enemy source and cooldown expiry. |
| `DefenseAndReceipt` | Real shared damage resolver perfect Guard with zero body damage; invulnerability rejection; wrong ASC, wrong context, rearm and disarm rejection; cancellation during reward delivery does not schedule later burst shots. |

Fixtures provide characters, attributes, teams, AI focus and primitive collision without gameplay assets. They change only authored Gunfire timing/spread/presentation defaults; the producer and damage routing under test are the production implementation. No runtime assertion result is claimed here: Unreal 5.7/UHT/UBT and required plugin/content dependencies are unavailable in this workspace, so these three suites have not been engine-compiled or executed.

The production cooldown header is engine independent. `python3 Scripts/Test-NativePolicies.py` compiled and passed all 10 available portable suites with C++17, warnings as errors and undefined-behavior sanitization, including 13 protection cooldown assertions. These checks cover exact expiry, repeated time, clock rewind, minimum cooldown and NaN/infinity. They do not substitute for the World/GAS suites.

## Content validation and scope

In PIE, place a living friendly companion/civilian behind Tarrik and let a native Drone Gunfire ability retain that ally as its AI focus. Verify one +15 protection notification, no reward for a wall or different character behind Tarrik, no body-hit-only reward when Tarrik is the intended target, and accepted perfect Guard with zero Health/Shield damage. Repeat at full Echo, spend before five seconds, retry the encounter, and cancel the Drone during its first damage callback. Check actual CharacterVisual and equipped/holstered weapon collision on the configured weapon trace channel.

Only this verified native Drone Gunfire producer currently generates the generic protection award. Rockets, self-destruct, melee, redirects and other enemy abilities require their own equally concrete protection geometry/receipt integration. Cinder Ward remains self-resistance and does not pretend to redirect ally damage. No Blueprint award boolean or parallel damage pipeline was introduced.
