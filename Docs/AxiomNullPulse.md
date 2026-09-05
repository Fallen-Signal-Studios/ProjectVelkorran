# Native Axiom Null Pulse

This implements the first slice selected by [the September gameplay audit](GameplayAudit-2026-09-04.md): Selene can spend Echo to break a shielded command formation using existing Narrative damage, command-link and reward systems. The baseline was `main` at `aef0371e10c11b55b61cb68d0f47d9fb814f2597`.

## Gameplay contract

| Property | Native behavior |
|---|---|
| Activation | Requires Selene identity, an allowed currently wielded granting weapon, valid tuning and at least 30 Echo. The existing Echo base owns payment and cancellation. |
| Charge | Authority measures elapsed time. Input release or full-charge fallback releases once; callers cannot supply charge, origin or targets. |
| Charge tuning | 0.65 seconds to full; 1,500–4,000 cm range; 8–22.5 degrees cone half-angle; 1.5–4 seconds suppression. Values interpolate with charge. |
| Target selection | Collision overlap followed by charge-derived cone/range and visibility checks, canonical character ownership and deduplication. Character center and bounded eye samples each require their own matching visibility check, so a nearby horizontal shot can hit. No full-world actor scan. |
| Shield | Current Shield is collapsed through Narrative's typed damage execution. Disruption is already resolved, bypasses Guard and Deflection, has Shield coefficient 1, Health coefficient 0, Poise 0 and Shield bypass 0. |
| Suppression | Independent duration Gameplay Effects own recharge-blocked tag counts. A shorter subsequent pulse cannot remove or shorten another active suppression. |
| Device shutdown | Native drones qualify; additional archetypes require an explicit allowlist. Biological hounds and handlers do not qualify by default. Device immunity and boss policy are separate checks. |
| Command links | Only a node authorized by the current native release can use the existing Sever transaction. A fresh valid sever consumes the existing one-time +12 Selene reward. |
| Recovery | Short post-release recovery, with cancellation/task/timer cleanup and activation-epoch checks against stale callbacks. |
| Feedback | Existing charge start/end presentation events, charge-alpha query, and a release event with origin, direction, charge, range, cone and affected-target counts. |

An uninterrupted fresh-link cast at 30 Echo ends at 12. At 100 it ends at 82. A miss still spends 30, consistent with the existing paid activation lifecycle. Cancellation does not introduce a new refund policy. Link Sever retains independent Hound Bite/Pounce while removing coordinated Horn Charge through the existing link consumers.

No new resource store, damage resolver, enemy-status manager or command-link ledger is introduced. The shared portable helper only computes charge and geometry; actor eligibility and all gameplay mutation stay in Unreal/GAS.

## Existing asset integration

The source repository does not include game or Narrative binary content. These are concrete acceptance steps for the existing working project, not claims that local assets are absent or incorrectly authored:

1. Use the existing Axiom weapon and ability grant. Its concrete ability Blueprint must derive from `USovGameplayAbility_SeleneAxiomNullPulse`, be granted with the actual weapon item as source object and allow that weapon class. Retain the approved contextual ability input slot. Confirm the ASC receives input-release events.
2. Remove old Blueprint damage/status/Sever payload graphs from this ability. Native release now owns gameplay. Retain montage, sound, Niagara and HUD work in the presentation events. `ReleaseAxiomNullPulseFromAim` is an authority-only optional release hook; it must not be followed by a second authored damage application. Full-charge release also works without that hook.
3. Use native effect defaults. Legacy serialized effect fields are retained for compatibility, but unsupported effect classes fall back to the native shells. This deliberately prevents an old custom effect from causing Health damage, permanent tags or a second periodic payload.
4. Bind charge feedback to `GetAxiomChargeAlpha`; bind `ReceiveAxiomPulseReleased` to the pulse and impact presentation. The release result is authority-side. For a remote-client presentation experiment, route cosmetics through the project's existing presentation/replication conventions before calling it network-validated. The approved game remains authored single-player.
5. Give intended command nodes query collision, a `SovCommandLinkComponent`, stable link identity and hostile participants. Visibility collision must represent walls and the correct character/visual ownership. Place one shielded hostile, a command node with Handler/Hound members, an eligible drone, a friendly and an occluding wall in the combat greybox.
6. Keep additional device-disable classes narrow. An explicit broad `AActor` allowlist would intentionally opt biological enemies into shutdown and defeat the default archetype distinction.

## Acceptance route in Unreal

Build and run the focused native suite using [UnrealValidation.md](UnrealValidation.md). Then run these content-dependent checks in PIE at 30, 60 and 120 fps:

- Earn Echo by a first weak-point break and a perfect deflection. With exactly 30, tap and fully charge Axiom against separate fresh formations. Verify the approved cost and one-time Sever reward, visible Shield break and predictable cone growth.
- Verify zero, partial and full Shields never lose Health or Poise. Guard and Deflection must not absorb the pulse. Verify suppression ends, and an overlapping shorter suppression does not end a longer grant.
- Verify a friendly, neutral, dead, invulnerable, Disruption-immune, outside-cone, out-of-range and occluded target is unaffected. Device immunity must not silently become general Shield immunity.
- Start drone gunfire/rocket/pursuit and interrupt with a successful disable. Sever a handler/hound command link and confirm independent Bite/Pounce continue. Recast on the same link and verify no extra reward.
- Release early, let the fallback timer fire, invoke the optional release hook twice, cancel, change weapons, enter a cinematic and die during charge. Verify no late or duplicate payload. Repeat immediately after cancellation to detect stale callbacks affecting the next cast.
- Observe draw/holster, aim origin at cover, charge sound/VFX cleanup, pulse result feedback, Echo HUD and weak-point reveal. Source tests cannot establish these asset behaviors.

## Validation record

- **Passed in the audit environment:** compiled the actual production `Combat/SovAxiomPulseMath.h` using g++ with C++17, `-Wall -Wextra -Werror -pedantic` and undefined-behavior sanitizer; 34 charge/interpolation/cone/range/invalid-input checks passed. Run `python3 Scripts/Test-AxiomPulseMath.py` to reproduce.
- **Provided for the real project:** seven Unreal world/GAS tests under `ProjectVelkorran.Campaign.AxiomNullPulse`, plus a Windows build/report-validation entry point. The tests are `ReleaseAndDamage`, `TargetingAndImmunity`, `DeviceEligibilityAndEffectOwnership`, `CancellationWeaponAndLinkReward`, `ReentrantCancellation`, `CloseHorizontalAim` and `ActivationGates`. They exercise real activation/payment, damage, collision, immunity, independent effect ownership and link rewards using content-free test fixtures. None has been executed here.
- **Reviewed:** existing Narrative API compatibility, ability lifecycle/reentrancy, damage routing, target ownership, test fixtures and integration documentation. Review corrected unrelated immunity blocking device shutdown, source changes between synchronous callbacks, reward loss after committed Sever cancellation, close-range targeting, and explicit drone test attribute registration. `git diff --cached --check` passed for all 22 changed files.
- **Not executed here:** UnrealHeaderTool, UnrealBuildTool, editor automation, Windows runner execution, PIE/content acceptance, packaged builds or Level 1/2 playthroughs. This environment has no UE5.7, Windows toolchain, PowerShell, ZenDyn or binary game/plugin content. There is no configured remote Unreal runner.

The native implementation is available for review. A passing portable geometry test does not establish a passing Unreal build. The slice's playable acceptance gate remains open until the real editor build, automation and content route above succeed.

The native suite does not yet exercise elapsed-time effect expiry, the full-charge timer firing, live Hound/Drone attack interruption, actual inventory/equip transitions, or network processes. Independent effect-handle removal verifies ownership, not timer expiry. These remain explicit editor acceptance cases rather than claimed automated coverage.

## Files changed

All paths are relative to the repository root.

| Area | Exact files |
|---|---|
| Existing ability | `Source/ProjectVelkorran/Public/Abilities/SovGameplayAbility_SeleneEcho.h`; `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_SeleneEcho.cpp` |
| Native pulse and math | `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_SeleneAxiomNullPulse.cpp`; `Source/ProjectVelkorran/Private/Combat/SovAxiomPulseMath.h` |
| Effect shells | `Source/ProjectVelkorran/Public/Effects/SovGameplayEffect_AxiomNullPulse.h`; `Source/ProjectVelkorran/Private/Effects/SovGameplayEffect_AxiomNullPulse.cpp` |
| Unreal tests | `Source/ProjectVelkorran/Private/Tests/SovAxiomRuntimeTests.cpp`; `Source/ProjectVelkorran/Private/Tests/SovAxiomRuntimeTestFixtures.h`; `Source/ProjectVelkorran/Private/Tests/SovAxiomRuntimeTestFixtures.cpp` |
| Validation tools | `Scripts/Validate-Unreal.ps1`; `Scripts/Test-AxiomPulseMath.py`; `Scripts/Tests/AxiomPulseMathTests.cpp` |
| Audit and implementation records | `Docs/GameplayAudit-2026-09-04.md`; `Docs/AuditEvidence-PlayerCombat.md`; `Docs/AuditEvidence-EnemySystems.md`; `Docs/AuditEvidence-Campaign.md`; `Docs/AxiomNullPulse.md`; `Docs/UnrealValidation.md` |
| Updated integration guides | `Docs/CampaignFoundation.md`; `Docs/SeleneEchoAbilities.md`; `Docs/SeleneCoreLoop.md`; `Docs/SeleneCommandLinkAndWeakPointReveal.md` |

## Follow-on work

The next three slices are the complete enemy ability chooser, repeatable encounter save/retry, and authored Level 1-to-Level 2 protagonist handoff. Remaining Selene spenders, two Tarrik spenders, 90-point signature readiness, weak-point gameplay consequences and other audit gaps remain explicitly outside this change.
