# Work PC preserved-change reconciliation — 2026-09-06

The work PC's pre-pull changes remain recoverable. The unique Dominion AI classes, their narrow link-configuration dependency, and confirmed missing legacy melee helper APIs were restored. The old stash was not applied over newer gameplay, save, travel, or accessibility work.

## Preserved revisions

- Pre-pull base: `aef0371e10c11b55b61cb68d0f47d9fb814f2597`.
- Pulled main reviewed: `0bbd7c897224b8ea3a60a1067e37dc21e5f07153` (merge PR #38).
- Preserved tracked state: `c1aaa147082d7591cb8cc20bd6a2808d1195d2b1`, initially `stash@{0}`, also retained by branch `codex/preserve-workpc-20260906`. Use the immutable SHA if stash ordering changes.
- Preserved untracked tree: `667eea568e8df652f2f83814c04e63472e20b629` (`c1aaa147^3`).
- Independent backup: `C:\Users\msnod\Documents\Codex\2026-09-06\go\work\pre-pull` contains `tracked.patch`, the original status, stash SHA, and untracked files.
- The older GitHub Desktop stash was not applied or removed.

## Restored exactly or narrowly

These four previously untracked runtime files were copied from the preservation backup without replacing any upstream class:

- `Source/ProjectVelkorran/Private/AI/BTTask_SovDominionHandlerCommandHound.cpp`
- `Source/ProjectVelkorran/Public/AI/BTTask_SovDominionHandlerCommandHound.h`
- `Source/ProjectVelkorran/Private/AI/SovDominionPackCoordinator.cpp`
- `Source/ProjectVelkorran/Public/AI/SovDominionPackCoordinator.h`

The Behavior Tree task selects a unique exact Handler command grant, observes its authoritative GAS completion, and applies failure-only retry backoff. The pack coordinator resolves explicitly authored NPC spawners, waits a bounded number of attempts for initialized Hounds with one ready Horn Charge grant, registers the exact pack, and activates its command link.

The preserved `SovDominionHandlerBTTaskTests.cpp` and `SovDominionPackCoordinatorTests.cpp` were restored under `Source/ProjectVelkorranTests/Private/Tests/`, matching upstream's Editor-only test-module layout. They were not returned to the runtime module.

`SovCommandLinkComponent.h/.cpp` received only:

1. The preserved `ConfigureLinkId(FName)` declaration and implementation: authority-only; rejects mutations in progress, non-inactive state, an existing instance identity, and `NAME_None`; writes the authored/replicated identity, increments revision, and wakes replication.
2. The `BeginPlay` configuration-warning condition now checks `bStartsActive`. An intentionally inactive link can be configured when its spawned participants become ready. `ActivateCommandLink` still performs the full configuration check unconditionally.

For coordinator-authored packs, the Handler link must start inactive (`bStartsActive=false`), with exact Handler/Hound spawner references and an encounter-unique `CommandLinkId`. The coordinator deliberately rejects already active or terminal link state.

### Confirmed legacy melee compatibility

The fresh Unreal Blueprint inventory subsequently reported "BeginMeleeDamageWindow" and "EndMeleeDamageWindow" missing from the authored /NarrativePro/Pro/Core/Abilities/GameplayAbilities/Attacks/Melee/GA_Attack_Combo_Melee asset (ContentInventory.log, 13:36:16). The preserved additive implementation was therefore restored in SovTransformingWeaponVisual.cpp/.h: authority and notify/montage ownership checks, per-window hit deduplication, polling, and holster/death cleanup. Header additions were merged around newer cinematic and animation-instance declarations. The new ability-owned native Melee/ implementation was unchanged. The restoration is included in the successful UE 5.7.4 Editor 24 and Game 09 builds and the subsequent 414/414 native automation pass. Fresh rendered PIE mapped-input checks observed both first-person and third-person attack montages for Verity and Velkorran (`work/live-primary-acceptance-Selene.json` and `work/live-primary-acceptance-Tarrik.json` in the work PC evidence directory). These checks establish authored attack activation and animation; they do not independently qualify legacy melee contact damage against an enemy.

### Confirmed legacy weapon debug switch

Fresh live weapon use later produced actual missing-console-variable warnings for `n.weapon.DrawVFX`. Root therefore restored the exact preserved `Source/ProjectVelkorran/ProjectVelkorran.cpp` declaration: include `HAL/IConsoleManager.h`, `TAutoConsoleVariable<bool> CVarSovDrawWeaponVFX`, default `false`, description `Draw Narrative weapon VFX debug helpers.`, and `ECVF_Default`. The current restored file matches the saved declaration in immutable preserved commit `c1aaa147082d7591cb8cc20bd6a2808d1195d2b1`; the primary module declaration remains intact. This registers the optional debug query without enabling drawing or changing weapon gameplay. The declaration is included in the successful Editor 24 and Game 09 builds and the current 414/414 native automation pass. Fresh GUI 11 wield/fire execution produced no missing `n.weapon.DrawVFX` warning (`Saved/Logs/WorkPC-Setup-11-20260906.log`, zero occurrences). No retained direct console-value readback is claimed; the default-false value is established by the restored source.

## Changes already superseded upstream

- `NarrativeGameplayTags.cpp` early-initialization guard and constructor bootstraps in `SovGameplayAbility_DominionHandler.cpp`, `SovGameplayAbility_DominionHound.cpp`, and `SovGameplayAbility_SeleneDeflection.cpp`: upstream's Narrative and Sovereign tag `Get()` functions initialize through idempotent registrars. Reintroducing per-CDO bootstraps is unnecessary.
- `SovGameplayAbility_SeleneEcho.cpp/.h` Axiom prototype and `Docs/SeleneCommandLinkAndWeakPointReveal.md`: upstream now owns native charge/input release, source-weapon and identity validation, shield/device payloads, line of sight, release-authorized Sever, interruption protection, and recovery in `SovGameplayAbility_SeleneAxiomNullPulse.cpp`. The old collision-independent link scan and normal-EndAbility release were not restored.
- `ProjectVelkorran.uproject`: the preserved edit only added a final newline. Keep upstream's Editor test module and updated plugin configuration.
- The original status listed `Source/ProjectVelkorranEditor.Target.cs`, but its preserved tracked patch contains no hunk for that file. Keep the current UE 5.7/V6 target and `ProjectVelkorranTests` module entry.

## Unique hardening retained for deliberate later integration

These changes are genuinely absent from reviewed main. They remain in the preserved commit; they have not been silently discarded or applied to newer implementations.

| Preserved paths | Exact behavior to assess and port selectively |
|---|---|
| `Private/Characters/SovDominionHandler.cpp`, matching public header | `FindSingleInactiveExactHornChargeAbility`, duplicate eligible-grant rejection, removal-pending filtering, state/movement preflight in `IsCommandableHound`, and single-handle revalidation/dispatch in `TryActivateExactHornCharge`. Preserve current combat interruption behavior when porting. |
| `Private/Abilities/SovGameplayAbility_DominionHound.cpp`, matching public header | Explicit Busy blocker; `BlocksWeaponEquippingAtActivation`, `HasAnyActivationBlockingState`, and `HasRequiredMovementStateForActivation`. The explicit equipping blocker itself is now inherited safely from NarrativeCombatAbility. |
| Original `Private/Tests/SovDominionHandlerTests.cpp` and `SovDominionHoundAbilityTests.cpp` | Multiplicity, blocker-container, equipping, and grounded movement checks accompanying the above changes. Port into the current Editor test files only with the corresponding runtime behavior. |
| `Private/Components/SovCommandLinkComponent.cpp` | `CanServeAsCommandLinkEndpoint` and its uses in activation, participant snapshots, and reveal: reject dead/Fatal endpoints, including synchronous death/reset callbacks. Do not replace upstream checkpoint restoration. |
| `Private/Components/SovWeakPointComponent.cpp`, matching public header | Real damage-matcher validation (non-None bone or valid physical material), first-identity deduplication, Fatal/dead reveal guards, and consistent reveal/presentation/matching. Main now also has persisted consequences and revealed targeting anchors; those newer paths must remain intact and use the same policy if this is ported. |
| Untracked `SovWeakPointExposureTests.cpp` | Reflection/replication, malformed-zone, duplicate-zone, and terminal-state exposure tests. Not restored before adapting the above policy to current lifecycle and consequence code. |
| `Private/Weapons/SovTransformingWeaponVisual.cpp`, matching public header | Restored after fresh Unreal compilation confirmed the legacy combo caller. The independent ability-owned native melee under `Source/ProjectVelkorran/{Public,Private}/Melee` remains unchanged. |
| `ProjectVelkorran.cpp` | Restored exactly after actual live weapon queries warned that `n.weapon.DrawVFX` was missing; default remains false with `ECVF_Default`. See confirmed restoration above. |
| Untracked `SovSeleneAxiomNullPulseTests.cpp` | Tests the superseded Axiom helper names and geometry seam. Do not copy unchanged; use the current Axiom runtime suite and production release path. |

No legacy helper names or the two old Hound cooldown tags were found by the targeted ASCII package-name scan across `Content/Inventory`, `PlayerCharacters`, `Characters`, `WeaponMeshes`, `SCFP`, and `FantasyBeast04`. This limited scan did not include the shared combo parent and was not proof of absence. Fresh Unreal Blueprint compilation then confirmed the missing melee calls described above. No speculative Axiom or other compatibility wrapper was added.

## Other preserved files and configuration

- `Config/DefaultEditor.ini`: WIP removed the `[ConsoleVariables]` section and four repeated `NarrativePro.DisableProjectSettingsSetupNotification=1` entries. No functional project setup depends on that removal; current settings were retained.
- `Config/DefaultGameplayTags.ini`: WIP reordered several existing tags and added `Sov.Ability.NPC.DominionHound.ChargeCooldown` / `PounceCooldown`. Fresh Unreal GUI startup subsequently reported both exact tags as invalid in the authored Horn Charge and Pounce ActivationBlockedTags. Both rows were restored additively, preserving upstream ordering. A focused raw scan of the actual DominionHound ability directory confirmed those exact references and no additional cooldown sibling; no redirects or speculative tags were added.
- Untracked `Readme.txt`: imported asset-package instructions (three meshes, forty material instances, twenty-three textures), not the project TDD. Preserved in the backup and untracked tree.
- Untracked `SCFP.png`: a small reference image of two science-fiction pistols. Preserved in the backup and untracked tree; no runtime dependency was identified.

## Validation and separate build repairs

`git diff --check` passed. UE 5.7.4 Editor 24 and Game 09 builds succeeded with the restored AI classes, link dependency, legacy melee helpers, debug switch, and compatibility repairs included. Build evidence is `Saved/Validation/20260906-062604-3878313c/Build-compatibility-24-success.log` and `BuildGame-09.log`. The subsequent full native run passed 414/414 matching tests, with coverage matching all 414 source tests and source unchanged during automation (`Saved/Validation/20260906-102000-d144ee90/summary.json`, `coverage.json`, and before/after source manifests; 25 test warnings). The automation invocation did not rebuild binaries; the separate successful build logs provide the build evidence. A final independent hash comparison found all 1,313 native project/plugin source files unchanged from that run's `source-after.json` (`work/final-native-review-20260906.json`). Fresh actual attack montage acceptance and its contact-damage limitation are recorded above.

Separate from preservation, compiler-reported C4458 local-name collisions were corrected without behavior changes in Narrative plugin `Private/UnrealFramework/NarrativePlayerController.cpp` (input/sprint Character and Tags locals) and `Private/AI/NarrativeThreatMemory.cpp` (local Blackboard and suspension-owner lambda arguments). These are current UE 5.7 build repairs, not restored WIP.
