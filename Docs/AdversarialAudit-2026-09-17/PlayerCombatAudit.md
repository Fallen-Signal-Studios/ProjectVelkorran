# Adversarial player combat audit, 17 September 2026

## Scope

Read-only engineering audit of ProjectVelkorran at HEAD `f07538c9` (branch `codex/aurelion-tdd-content-20260913`, with the uncommitted content edits listed in the session git status). No source, config or content was edited, no build or test was run, and Unreal Editor was not launched.

**Authority.** TDD `Docs/Design/Sovereign_Call_Origins_TDD_v2_2026-08-14.md`: §4 (input, camera, movement, traversal), §5 (protagonists), §6 (combat), §7 (Echo, Resonance, Corruption), Appendix A.1–A.3 and B.1, read against §0.3 labels and §0.7 non-goals. `Docs/CampaignV2ChangeLog.md` permits: the revised Tarrik and Selene Echo rosters, player Health recharge, Cinderline magazine and reserve ammo, transient ammo and Echo drops, and deterministic primary-fire variation. None of those is reported as missing. Aim-down-sights was removed deliberately on 2026-09-16 (commit `36e1dfb6`). It is noted, not flagged. Narrative's own `GA_Weapon_Aim` and the `State.Weapon.IsAiming` tag are still granted by the weapon items.

**Evidence method.**
- **Source.** Game module `Source/ProjectVelkorran`, tests `Source/ProjectVelkorranTests`, and the customised plugin `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal` (abbreviated `Arsenal/` below). `P/` abbreviates `Source/ProjectVelkorran/`.
- **Content.** `.uasset` and `.umap` files are binary. I did not interpret Blueprint graphs. I did scan serialized package paths, class paths and FName strings, such as `/Script/ProjectVelkorran.SovGameplayAbility_Evade` or `Sov.State.Poise.Broken`, to see which native classes, abilities and tags each asset references. The scan covered about 6,800 project-owned assets under `Content/{Abilities,Campaign,Characters,Framework,Input,Items,PlayerCharacters,Weapons,Cues,Inventory,Spawners,UI,Aurelion,Maps}` and `Plugins/.../Content/Pro/Core`, plus 242 level and external-actor files. A string match proves a reference exists. A missing match proves a class or tag is not referenced by name. Neither proves what a graph does at runtime. Every finding that relies on this scan is labelled as content evidence.
- **Tracking caveat.** The player kit wiring assets are **not git-tracked** under the repo content policy (`.gitignore:58`, `:70`). That covers `AC_Tarrik`, `AC_Selene`, `PD_*`, `IC_*`, `WI_Velkorran`, `IMC_Combat`, `DA_CombatInputs`, `GA_Evade`, `GA_Sprint`, `GA_Tarrik_Guard`, `GA_Selene_Deflection`, `BP_SovTarrik`, `BP_SovSelene` and `BP_SovPlayerController`. The content conclusions below describe this workstation. A clean clone cannot reproduce them (see PC2-15).

## TDD requirement coverage

Status key: **Impl** = implemented and wired; **Partial** = engineered but incomplete or not wired; **Missing**; **Changed** = deliberate change-log or creator decision.

| TDD | Requirement (abridged) | Status | Evidence |
|---|---|---|---|
| §4.1 | Shared control grammar, pad and KBM parity | Partial | Semantic input and routing in `Arsenal/Private/UnrealFramework/NarrativePlayerController.cpp:924-1000`. `Content/Input/IMC_Combat` and `DA_CombatInputs` map Attack, AltAttack, HeavyAttack, Ability2/3, Evade, Grenade, Sprint, Cover, Crouch and Reload. **No** lock-on, companion-command, evidence or ability-modifier mapping in content (PC2-07). |
| §4.2 | Enhanced Input, layered IMCs, semantic tags, timestamped buffer, remap, sensitivity, inversion, dead zone, hold/toggle | Partial | Buffer with timestamps and 0.22 s base in `Arsenal/Private/GAS/NarrativeCombatInputBuffer.cpp:80-95`. Toggles in `NarrativePlayerController.cpp:924-936` and `P/Public/Settings/SovGameUserSettings.h:41-47`. Sensitivity, inversion and dead zone in `Arsenal/Private/Settings/NarrativeInputSettings.cpp`. Remap via `bEnableUserSettings` in `Config/DefaultInput.ini:97-98`. Only one project IMC exists, not the nine state-layered contexts. The buffer is consumed only by the native melee, which protagonists do not use (PC2-01). No buffer visualisation found. |
| §4.3 | `USovCameraControlComponent`: mode arbitration, framing, shoulder swap, FOV, lag | Missing (native) | No such class. Camera is Narrative `NarrativeCameraComponent` / `NarrativeCameraMode` plus UE GameplayCameras assets referenced by `BP_SovTarrik` (PC2-08). |
| §4.4 | Per-protagonist camera profiles | Missing | No source value matches 390/350/430/380 cm or 78/82/58/52°. Content not interpretable. |
| §4.5 | Camera safety rules | Partial | Spring-arm collision forced on at `P/Private/Targeting/SovTargetingComponent.cpp:197`. Shake disable in `Arsenal/Private/Camera/NarrativePlayerCameraManager.cpp:29`. No off-screen warning, finisher return-control or corruption-camera rules found. |
| §4.6 | Movement speeds, combat-only sprint stamina, evade intent | Impl (defect) | Profiles in `P/Public/Exertion/SovExertionComponent.h:16-28` and `P/Private/Exertion/SovExertionComponent.cpp:52-62,97-110`. Combat sprint drain at `:238`. "Out of combat" can latch on (PC2-03). Mantle, ladder and squeeze are Narrative traversal content, unverified. No heavy-attack turn-rate limit found. |
| §4.7 | Evade invulnerability 0.18/0.27 s, perfect windows 0.16/0.11 s, difficulty widening | Impl (defect) | `SovExertionComponent.cpp:59`, `P/Public/Components/SovGuardComponent.h:81`, `P/Public/Components/SovDeflectionComponent.h:78`. Widening applies to evade only (PC2-04). |
| §4.8 | Traversal authoring by protagonist | Needs content | Not assessable from source. |
| §5.1 | Shared contract (light/heavy/charged/defence/evade/finisher/4 abilities; soft/hard lock; takedowns; respec) | Partial | Evade, Guard, Deflection and abilities are granted (content refs). No finisher grant, no working hard lock and no soft lock (PC2-07, PC2-09). |
| §5.2.2 / §5.3.2 | Signature equipment | Impl | `Content/Items/Weapons/WI_{Velkorran,Cinderline,Verity,Staccato,Axiom}`, loadouts `IC_Tarrik` and `IC_Selene`. |
| §5.2.3 / §5.3.3 | Combat cadence (3–4 committed links, heavy cleave, punishable commitment, Selene exits) | Partial / needs content | Player melee is Narrative `GA_Attack_Combo_Melee` Blueprint combos, not the native graph (PC2-01, PC2-02). |
| §5.2.4 / §5.3.4 | Core abilities | Changed (Impl) | Revised rosters in `P/Private/Abilities/SovGameplayAbility_{TarrikEcho,TarrikCinderSlam,TarrikCinderlineRequiem,Selene*}.cpp`, granted by the weapon items and AC assets. Costs are 30–50 for basic and 90 for signature, against the TDD's 20/25–35 and 100 (see PC2-05 note). |
| §5.2.5 / §5.3.5 | Three branches, free respec at safe points | Partial | `P/Public/Progression/SovTechniqueComponent.h:31` (`RespecAtSafePoint`) and `SovTechniqueTypes.h:22,40` (BranchId). The only content reference is `BP_SovPlayerState`. No branch or node assets found. |
| §5.4 | Convergence and Resonance interactions | Partial | `P/Private/Resonance/SovResonanceComponent.cpp:207-231` implements the four setups. No `SovResonanceTargetComponent` is placed in any scanned level, including `L_Aurelion_M12` and `M13`. `SupportSever` needs a mark nothing produces (PC2-06). |
| §6.2 | Combat state stack, single source of truth | Impl / Partial | One Narrative attribute set for Health, Shield, Stamina, Poise and Echo, plus `USovCorruptionAttributeSet`. A second, legacy corruption owner remains (PC2-12). |
| §6.3 | Shield 3 s / 5 %/s, routing order | Impl; Health recharge Changed | `P/Public/Components/SovShieldComponent.h:140,144`. Resolver `Arsenal/Private/GAS/NarrativeAttributeSetBase.cpp:711-900`, exec calc `Arsenal/Private/GAS/NarrativeDamageExecCalc.cpp:190-251,322-386`. |
| §6.4 | Damage formula, bounded mitigation, channels | Impl | `NarrativeDamageExecCalc.cpp:329-386`. Difficulty scalar is a SetByCaller with no producer. Player incoming scale is applied at `NarrativeAttributeSetBase.cpp:451-464`. |
| §6.5 | Stamina rules and values | Impl | `SovExertionComponent.cpp:159-182,228-266`. Evade admission at `P/Private/Exertion/SovGameplayAbility_Exertion.cpp:60-96`. |
| §6.6 | Poise states, recovery immunity, super-armor windows | Partial | States and recovery in `P/Public/Components/SovPoiseComponent.h:135-155`. No producer of `State_Poise_SuperArmor` (PC2-13). Protagonist attacks are not gated by break (PC2-02). |
| §6.7 | Status framework and launch families | Partial | Definition fields `P/Public/Status/SovStatusDefinition.h:61-161`. Built-ins cover Burn, Chill, Freeze, DeviceDisabled and Exposed (`P/Private/Components/SovStatusComponent.cpp:371-437`), plus corruption. Suppressed, cloaked/sensor-blurred and silenced/ability-locked are missing. |
| §6.8 | Melee framework: data asset, socket sweeps, ledger, substeps, 0.22 s buffer, cancels | Partial (not wired for players) | Native `P/Private/Melee/*` is complete and tested but used only by four Aurelion enemy abilities (PC2-01). Heavy and ranged branches are missing (PC2-14). |
| §6.9 | Ranged: recoil, spread, ammo, weak points, tracer/collision agreement, Cinderline burst from melee node | Partial; ammo Changed | Ammo transaction fixed (`Arsenal/Private/Items/WeaponItem.cpp:482-537`). Recoil presets reversed (PC-08). Staccato Zero uses a different collision policy (PC2-10). No melee-to-burst path. |
| §6.10 | Targeting: free, soft magnetism, hard lock, cycling, marks, command targets, separate assists | Partial | Hard lock and cycling in `SovTargetingComponent.cpp` are unbound in content, with no eligible targets (PC2-07). Ranged aim friction at `NarrativePlayerController.cpp:354-371`. Melee assist is native melee only. Marks and command targets have no producer (PC2-06). |
| §6.11 | Finishers | Partial (unwired) | `P/Private/Abilities/SovGameplayAbility_Finisher.cpp` is tested but granted to no player and has no targets in content (PC2-09). |
| §6.12 | Threat readability | Partial | Attack-class tags exist. Presentation belongs to the enemy/UI domain. |
| §6.13 | Fatal, rescue, reload, respawn invulnerability | Impl (P3 gap) | `P/Private/Recovery/SovFatalRecoveryComponent.cpp`, `P/Public/Recovery/SovRecoveryPolicy.h`. No early end on offensive action (PC2-11). |
| §6.14 | Difficulty presets and separate tuning | Partial | `P/Private/Settings/SovGameUserSettings.cpp:303-321`. Defence-window assist is partly ignored (PC2-04). |
| §6.15 | Telemetry | Partial | `P/Public/Diagnostics/SovDiagnosticsSubsystem.h:16-21`. No producer for `CameraFailure`, whiff/hit rate, cancel type, perfect-defence attempts or time-to-specialist-kill. |
| §7.2 | Echo 0–100, no passive regen, 6 s / 8 per s decay to 25, encounter normalisation, checkpoint value | Impl (defect) | `P/Private/Components/SovEchoComponent.cpp:52-90,222-340`, `P/Private/Campaign/SovEncounterDirector.cpp:475,500,522`. Latch bug (PC2-03). |
| §7.3 | Tarrik generation table | Partial | Perfect guard +12 and counter +10 at `P/Private/Components/SovGuardComponent.cpp:486,569`. Poise break +15 and intercept +15 at `P/Private/Components/SovTarrikEchoGenerationComponent.cpp:927,940-962`. Heavy 3+ is unreachable for the player, command target has no producer, and there is an extra ranged-cadence source (PC2-05). |
| §7.4 | Selene generation table | Partial | Values in `P/Public/Components/SovSeleneEchoGenerationComponent.h:105-128`. Mark-window kill has no producer (PC2-06). |
| §7.5 | Resonant state at 75, signature readiness | Impl / Changed | `SovEchoComponent.cpp:429-450`. Readiness is keyed to the Requiem and Dispatch 90 cost (`:177-206`). |
| §7.6 | Joint Resonance handshake | Partial | Engineering is present and tested (`P/Private/Resonance/SovResonanceComponent.cpp`, `SovResonanceRuntimeTests.cpp`). Not placed in content. |
| §7.7 | Eclipse corruption bands and agency protections | Partial | Bands and remedies engineered and tested (`SovCorruptionRuntimeTests.cpp:110-540`). **No** `SovCorruptionSourceVolume`, `SovCorruptionSourceComponent`, profile or interactable in any scanned Aurelion content. |
| §7.8 | Corruption architecture | Partial | `P/Private/Components/SovCorruptionComponent.cpp` coexists with `SovLegacyCorruptionComponent` (2,125 lines). Status requests reach only the legacy owner (PC2-12). |
| A.1 | Shared tuning | Mostly Impl | Shield, Echo, buffer and finisher duration match. Respawn 1.5 s is present without early end (PC2-11). |
| A.2 / A.3 | Protagonist tuning | Impl (costs Changed) | Stamina, evade, windows and Echo rewards match. Ability cost bands differ (see §5.2.4 row). |
| B.1 | Attack definition minimum fields | Partial | `P/Public/Melee/SovMeleeAttackDefinition.h:10-42` lacks owner permissions, telemetry ID, schema version, AI use rules, accessibility annotations, hit-stop/camera/VFX/haptic request IDs, Echo gain rule and finisher eligibility (PC2-14). |

## Findings

### PC2-01 — Protagonist melee bypasses the native TDD melee framework [P1, missing integration; content evidence]

**TDD:** §6.8 in full; §5.2.3 and §5.3.3; B.1; A.1 (0.22 s buffer); §7.3 (heavy multi-hit Echo).

**Evidence.**
- **Native framework.** `P/Private/Melee/SovGameplayAbility_Melee.cpp` with the sweep task and definition implements authored nodes, a swept socket path with temporal substeps (`SovAbilityTask_MeleeSweep.cpp:77-130`), a per-attack ledger, a cover check, a 0.22 s buffered branch (`SovGameplayAbility_Melee.cpp:305`, `NarrativeCombatInputBuffer.cpp:88-95`), charged release, defensive cancel and `USovEchoAttackReceipt` (`:296`).
- **Who uses it.** Content references to `SovGameplayAbility_Melee` and `SovMeleeAttackDefinition` exist **only** in `Content/Aurelion/Enemies/Abilities/GA_Eclipse{Elite,Linkbound,WallRunner,Weaver}_Melee` and their `DA_*_Melee` data.
- **Tarrik's melee.** `WI_Velkorran` grants `/NarrativePro/Pro/Demo/.../GA_Attack_Melee_Sword_1H_Tarrik`, whose parent is `/NarrativePro/Pro/Core/.../Melee/GA_Attack_Combo_Melee`.
- **Selene's melee.** `WI_Verity` grants `GA_SovVerityTwinAttack`, whose parent is the same `GA_Attack_Combo_Melee`.
- **What the Narrative base does.** Its serialized node names include `SphereTraceMultiForObjects`, `LineTraceSingleForObjects`, `ApplyDamageToTargets`, `GE_WeaponDamage_Heavy` and `EndMeleeDamageWindow`. It references no Sov melee class and no `SovEchoAttackReceipt`.
- **Receipts.** `USovEchoAttackReceipt::CreateForActiveAbility` has exactly one native caller (`SovGameplayAbility_Melee.cpp:296`). It is `BlueprintCallable` (`P/Public/Combat/SovEchoAttackReceipt.h:15-16`), but the name does not appear in either protagonist attack asset.

**Failure sequence.**
1. Player presses light attack as Tarrik.
2. `GA_Attack_Melee_Sword_1H_Tarrik` runs Narrative's Blueprint trace and damage path.
3. None of the following apply, because they exist only in the unused native class: per-node socket sweep and substep, native hit ledger, cover lane test, authored startup/active/recovery/branch windows, 0.22 s buffer and its accessibility extension, node-specific defensive cancel paid through Exertion, charged-release stamina tier, hit-confirm early branch, and the melee aim-assist setting (`SovGameplayAbility_Melee.cpp:236-237`).
4. `SovTarrikEchoGenerationComponent::HandleDamageResolvedAsSource` requires `Receipt->MatchesCommittedHeavyAttack` (`P/Private/Components/SovTarrikEchoGenerationComponent.cpp:911`). The Blueprint attack supplies no receipt, so "Heavy attack hits 3+ valid targets +8" (§7.3) can never be earned by the player.
5. Regressions such as `SovMeleeRuntimeTests.cpp:74-105` and `SovCombatActionTransactionRuntimeTests.cpp:216-235` pass against a fixture configuration the shipped player does not use.

Whether the Narrative Blueprint path is frame-rate independent, avoids repeat hits or honours cover needs runtime evidence. The graphs are unreadable here.

**Confidence.** High that protagonists are not wired to the native framework. Medium on the behavioural consequences.

**Fix inside existing owners.** Author Tarrik and Selene `USovMeleeAttackDefinition` data. Make `GA_Attack_Melee_Sword_1H_Tarrik` and `GA_SovVerityTwinAttack` Blueprint children of `USovGameplayAbility_Melee`, keeping the montages and `Combo_*` sets as node montages. Alternatively, if the Narrative combo must stay, route its damage through `CreateForActiveAbility` and the native sweep task. Add a content-validation test asserting that each protagonist melee weapon grants a `USovGameplayAbility_Melee` subclass with a valid definition. Do not build a third melee path.

### PC2-02 — Shared `UNarrativeCombatAbility` does not gate or interrupt on Poise break, Fatal or Frozen; protagonist attacks inherit nothing [P2, source-proven gate absence; runtime and content evidence needed for impact]

**TDD:** §6.6 (Broken produces stagger); §5.2.3 ("failed commitment should be punishable"); §6.5 (no unpredictable partial activation).

**Evidence.**
- The base constructor blocks only `State_Weapon_Equipping` (`Arsenal/Private/GAS/NarrativeCombatAbility.cpp:43-51`), and `CanActivateAbility` adds only `State_Evading` (`:57-62`).
- The break, fatal and freeze fences exist only in subclasses: `SovGameplayAbility_Melee.cpp:29-41,54-59,99-123` and `SovGameplayAbility_Echo.cpp:33-46,586-628`.
- Poise break adds only `Sov.State.Poise.Broken` (`P/Private/Components/SovPoiseComponent.cpp:105-108,1050-1060`). It adds no Busy or BlockFiring tag.
- Content: the FName tables of `GA_Attack_Combo_Melee`, `GA_Attack_ComboBase`, `GA_CombatAbilityBase`, `GA_SovVerityTwinAttack`, `GA_Attack_Melee_Sword_1H_Tarrik` and `GA_CinderlinePrimary` contain no `Sov.State.Poise.Broken`, `Sov.State.Fatal` or `Sov.State.Status.Frozen`. Freeze separately grants Busy and BlockFiring (`SovStatusComponent.cpp:398-414`), so Freeze is partly covered.
- No generic ASC cancellation exists for break states. The only `CancelAbilities` calls are for death (`Arsenal/Private/GAS/NarrativeAbilitySystemComponent.cpp:889,908`).

**Failure sequence.** An enemy heavy attack breaks Tarrik's Poise. Nothing native blocks a new `GA_Attack_Melee_*` or `GA_CinderlinePrimary` activation, or ends one already running. Unless an authored reaction montage or Blueprint listener happens to own that, the player keeps swinging or firing through "Broken".

**Confidence.** High for the native gate absence. Medium for observable impact, since a reaction montage could mask it.

**Fix.** Move the shared interruption set and its tag-event binding (Melee's `MeleeTransactionInterruptions` / `BindInterruptions`) into `UNarrativeCombatAbility`, with an explicit super-armor exemption hook. Add a regression that breaks Poise during a Blueprint-derived `UNarrativeCombatAbility`.

### PC2-03 — Any dealt or received damage permanently latches "encounter active", draining sprint stamina and running combat-only rules out of combat [P2, source-proven]

**TDD:** §4.6 (out-of-combat sprint stamina: no, BASELINE); §4.1 ("no out-of-combat stamina drain"); §7.2 (decay tied to combat participation).

**Evidence.**
- `USovEchoComponent::RecordCombatActivity` sets `bEncounterActive = true` (`P/Private/Components/SovEchoComponent.cpp:341-353`).
- It is called for any positive damage dealt or received: `HandleDealtDamage` at `:477-488` and `HandleReceivedDamage` at `:491-505`, bound at `:135-139`.
- `AddEcho` (`:236`) and `TrySpendEcho` (`:269`) also call it.
- The only reset is `EndEncounter` (`:318-339`), and its only callers are `ASovEncounterDirector::CompleteEncounter` and `FailEncounter` (`P/Private/Campaign/SovEncounterDirector.cpp:500,522`).
- Consumers: `USovExertionComponent::IsCombatActive` (`P/Private/Exertion/SovExertionComponent.cpp:184-189`) drives sprint drain (`:238-245`) and exhaustion gating (`:193`). `SovNarrativeCueComponent.cpp:62` also reads it.
- Content scan: `SovEncounterDirector` appears only in `L_Aurelion_M12`, not `L_Aurelion_M13` or the development maps.

**Failure sequence.**
1. During exploration the player takes fall or hazard damage, or strikes any damageable object or NPC outside a director.
2. `bEncounterActive` becomes true and never clears.
3. Every later sprint drains 14–16 stamina per second, exhausted sprint is refused, and narrative cues treat the player as in combat.
4. This persists until some director calls `EndEncounter`. On maps without a director, it persists for the whole session.

**Confidence.** High.

**Fix.** In `USovEchoComponent`, split "activity timestamp" from "encounter scope". `RecordCombatActivity` should update the inactivity clock only. Keep `bEncounterActive` owned by `BeginEncounter` and `EndEncounter`, or add a bounded hostile-engagement timeout in the same component. Add regressions: environmental damage outside an encounter must not enable sprint drain, and an encounter must clear on checkpoint reload.

### PC2-04 — Defence-window assistance and Story difficulty widen only evade invulnerability, not perfect guard or deflection [P2, source-proven]

**TDD:** §4.7 ("Difficulty settings may widen timing windows"); §6.14 (Story: wider defence windows; separately tunable defence-window assistance).

**Evidence.**
- Story sets `DefenseWindowScale >= 1.5` (`P/Private/Settings/SovGameUserSettings.cpp:309-311`), and the accessibility menu exposes it (`P/Private/UI/SovAccessibilitySettingsMenu.cpp:216`).
- The only gameplay consumer is Evade (`P/Private/Exertion/SovGameplayAbility_Exertion.cpp:150-152`).
- `USovGuardComponent` times the perfect window with the raw `PerfectDefenseWindow` (`P/Private/Components/SovGuardComponent.cpp:144-155`).
- `USovDeflectionComponent` does the same with raw `PerfectDeflectionWindow` (`P/Private/Components/SovDeflectionComponent.cpp:158-165`).
- Deflection is Selene's entire parry window, so Story and assist users receive an unassisted 0.11 s window.

**Failure sequence.** Select Story, or set "Defence window assistance" to 2.0. Tarrik's perfect guard stays 0.16 s and Selene's deflection stays 0.11 s. Only the evade i-frames scale.

**Confidence.** High.

**Fix.** Read `GetDefenseWindowScale()` in both components when opening the window, clamped as Evade does. Extend `SovGuardRuntimeTests` and `SovDeflectionRuntimeTests` with a scaled-window case.

### PC2-05 — Tarrik Echo economy diverges from §7.3: ranged cadence awards Echo for body-shot volume; heavy multi-hit and command-target sources are unreachable [P2, source-proven; design decision needed]

**TDD:** §7.2 ("earned through defined actions, not raw time or damage alone"); §7.3 table; §5.2.6 ("ranged tool … cannot replace melee engagement").

**Evidence.**
- **Cadence is auto-wired.** `HandleDealtDamage` (`P/Private/Components/SovTarrikEchoGenerationComponent.cpp:735-790`, bound at `:135-137`) adds cadence for every qualifying Cinderline primary-fire hit on a hostile. Body hits count `BodyHitCadence=1` (`P/Public/Components/SovTarrikEchoGenerationComponent.h:120-121`).
- **Rate.** A completed cadence of 6 awards `CadenceEchoReward=4` at most once per second (`.h:127-145`).
- **Not approved.** The change log approves ability rosters, ammo and drops, not an extra generation source.
- **Heavy 3+ unreachable.** It needs a native melee receipt (`.cpp:911-921`; PC2-01).
- **Command target unreachable.** It requires `State_CommandTarget_Window` (`.cpp:930`), which nothing in source produces and no scanned asset references (PC2-06).
- **Cost bands.** Ability costs are 50/35/50/90/90 (`SovGameplayAbility_TarrikEcho.cpp:206,524,918`; `SovGameplayAbility_TarrikCinderSlam.cpp:12`; `SovGameplayAbility_TarrikCinderlineRequiem.cpp:17`) against the TDD's 25–35 basic and 100 signature. The change log's "payloads" wording may cover this. The creator should confirm.

**Failure sequence.** Tarrik holds distance and body-shots a formation with Cinderline. He fills Echo through the rifle without guarding, countering or breaking poise, while the melee heavy reward can never fire. That inverts the §7.3 incentive.

**Confidence.** High for the mechanism. Game impact depends on fire rate and tuning.

**Fix.** Record a creator decision in `CampaignV2ChangeLog.md` or restrict cadence to precision hits in the existing component (`BodyHitCadence=0`, or a policy flag). Fix PC2-01 to restore heavy rewards, and add a command-target producer (PC2-06).

### PC2-06 — Priority marks and command targets have no producer; four TDD mechanics that depend on them cannot trigger [P2, missing integration]

**TDD:** §6.10 (priority mark selection for Selene, command target selection for Tarrik); §7.3 (+8 execute command target); §7.4 (+6 defeat during mark window); §5.4 (Tarrik perfect-guards a *marked* heavy, enabling SupportSever).

**Evidence.**
- Readers only: `SovSeleneEchoGenerationComponent.cpp:469` (`State_Target_Marked`), `SovTarrikEchoGenerationComponent.cpp:930` (`State_CommandTarget_Window`) and `SovResonanceComponent.cpp:225` (`State_Target_Marked`).
- Neither tag is added anywhere in source.
- The string `Sov.State.Target.Marked` appears in no scanned asset.

**Failure sequence.** No gameplay path can grant these states, so the corresponding Echo awards and the SupportSever Resonance offer never occur.

**Confidence.** High for source. Content scan high for `Marked`. Only an unscanned marketplace path could contain the command-target tag.

**Fix.** Add mark and command-target designation to `USovTargetingComponent` (selection) with a status definition in `USovStatusComponent` (duration and cleanse). Bind inputs in `DA_CombatInputs`.

### PC2-07 — Hard lock and target cycling are unbound in content, have no eligible targets, and aiming drops the lock as a "Cinematic" loss; soft lock is absent [P2, missing integration plus a P3 telemetry defect]

**TDD:** §4.1 (lock-on / threat focus binding); §4.3 (soft target focus, hard lock); §6.10; §6.15 (lock-on failures).

**Evidence.**
- Lock requires the actor tag `Sov.Target.HardLock` (`P/Private/Targeting/SovTargetingComponent.cpp:95`). The string is absent from all scanned project assets and all 242 level and external-actor files.
- Input arrives only through the semantic tags `Input_ThreatFocus` and `Input_CycleTarget*` (`:222-229`). No scanned asset contains `Narrative.Input.ThreatFocus` or `CycleTarget`, and `IMC_Combat` and `DA_CombatInputs` reference no such action.
- `CanControlCamera` is false while `State_Weapon_IsAiming` (`:65`), and Tick then clears the lock with `ESovLockLossReason::Cinematic` (`:191-194`), which is also recorded as a lock-failure diagnostic (`:128-129`). `GA_Weapon_Aim` is still granted by `WI_Cinderline`, `WI_Staccato` and `WI_Axiom`.
- Soft magnetism exists only inside the native melee aim correction (`SovGameplayAbility_Melee.cpp:234-286`), which protagonists do not use (PC2-01).

**Failure sequence.** A player can never lock. If lock were wired, pressing aim would drop it, and telemetry would report a cinematic loss.

**Confidence.** High.

**Fix.** Tag authored duel and elite archetypes. Add threat-focus and cycle input actions to the existing `DA_CombatInputs` and `IMC_Combat`. In `SovTargetingComponent`, keep the lock but suspend camera steering while aiming, and use a distinct loss reason. Add soft focus to the same component rather than a new camera owner.

### PC2-08 — No native camera owner or protagonist camera profiles [P2, missing]

**TDD:** §4.3 (`USovCameraControlComponent` owns arbitration, collision, shoulder swap, aim transition, framing, FOV, lag and effect requests); §4.4; §4.5.

**Evidence.**
- No such class exists and no project source defines shoulder swap.
- The camera is Narrative's generic `NarrativeCameraMode` (`Arsenal/Private/Camera/NarrativeCameraMode.cpp:11`, default arm 300 cm) plus UE GameplayCameras assets (`CameraAsset_SandboxCharacter`, `CameraRig_*`) referenced from `BP_SovTarrik`.
- The only project camera logic is lock steering and auto-camera in `SovTargetingComponent`.

**Failure sequence.** Tarrik and Selene share generic framing, so the §4.4 mass and precision distinction, off-screen warnings, finisher lens return and corruption-camera constraints have no engineering owner.

**Confidence.** High for source. Content camera assets might hold per-character values; that is unverified.

**Fix.** Either formally adopt Narrative's camera component and GameplayCameras as the §4.3 owner in the change log and author two profiles, or add the arbitration layer on top of `UNarrativeCameraComponent`. Do not add a competing camera actor.

### PC2-09 — Finisher, Resonance and Corruption systems are engineered and tested but not granted or placed [P2, missing content integration]

**TDD:** §6.11, §5.4 / §7.6, §7.7–7.8.

**Evidence.**
- Zero scanned assets or levels reference `SovGameplayAbility_Finisher`, `SovFinisherTargetComponent`, `SovResonanceTargetComponent`, `SovCorruptionSourceVolume`, `SovCorruptionSourceComponent`, `SovCorruptionProfile` or `SovCorruptionInteractable`. That includes `L_Aurelion_M12`, `L_Aurelion_M13` and their external actors.
- No native code grants the finisher ability. Grant sites are `SovResonanceComponent.cpp:264` (Resonance only), `SovProtagonistCompanionCharacter.cpp:155` (companion kit) and `SovTechniqueTypes.cpp:80` (techniques).
- `AC_Tarrik` and `AC_Selene` grant Evade, Sprint, Cover, Crouch, Jump, Reload, Wield, Death, unarmed punch and one grenade ability each.

**Failure sequence.** In the Aurelion vertical slice, which the TDD names for corruption and convergence, the player has no finisher and there are no corruption sources or Resonance offers.

**Confidence.** High for named-class absence.

**Fix.** Grant `USovGameplayAbility_Finisher` through `AC_*`. Add `SovFinisherTargetComponent` to eligible enemy archetypes. Place Resonance targets and corruption profiles in M12 and M13 through the existing encounter and mission definitions. Add a content-validation gate listing the required components per mission.

### PC2-10 — Staccato Zero uses a different collision and material policy from every other precision path [P2, source-proven omission; observable impact depends on physics assets]

**TDD:** §6.9 (validated hitscan; tracers may not contradict collision; high deterministic weak-point effectiveness); §6.4 (HitZoneModifier); §5.3.6.

**Evidence.**
- `P/Private/Abilities/SovGameplayAbility_SeleneStaccatoZero.cpp:52-56` traces `ECC_Visibility` without `bReturnPhysicalMaterial`.
- Native melee and Cinderline Judgement use Narrative's `WeaponTraceChannel` (`SovAbilityTask_MeleeSweep.cpp:92-94`, `SovGameplayAbility_TarrikEcho.cpp` `PerformTraceMulti`).
- The exec calc derives `HitZoneMultiplier` from `Hit->PhysMaterial` (`NarrativeDamageExecCalc.cpp:329-336`).
- Aim origin is the actor eyes with control rotation (`P/Private/Combat/SovSelenePayload.cpp:138-149`), not the player view point.

**Failure sequence.**
- An enemy with a `UNarrativePhysicalMaterial` head multiplier takes a Zero headshot. `PhysMaterial` is null, so the multiplier is 1.0, and Selene's signature precision shot does less than an ordinary primary headshot.
- If a hit proxy blocks the weapon channel but ignores Visibility, Zero passes through it.

Bone-based `SovWeakPointComponent` zones still work. Prior audit coverage risk 2 is still open.

**Confidence.** High for the omission. Impact depends on content.

**Fix.** Use `WeaponTraceChannel` with `bReturnPhysicalMaterial=true` in the existing ability, and share the Judgement eye-to-muzzle bridge helper.

### PC2-11 — Respawn invulnerability does not end on offensive action [P3, source-proven]

**TDD:** A.1 ("1.5 s or until offensive action, whichever first"); §6.13.

**Evidence.** `SovFatalRecoveryComponent::ApplyProtection` grants `State_Invulnerable` and `State_Invulnerable_Respawn` for a fixed duration (`P/Private/Recovery/SovFatalRecoveryComponent.cpp:196-209`, called at `:219` with `CheckpointProtectionSeconds=1.5`). No ability-activation or damage-dealt listener removes it early.

**Fix.** In the same component, bind `OnDealtDamage` or ability activation for `UNarrativeCombatAbility` for the protection lifetime and remove `ProtectionHandle` when either fires.

### PC2-12 — Generic corruption status requests are rejected for the managed-campaign player; legacy corruption volumes are dead [P3, source-proven]

**TDD:** §6.2 and §7.8 (no duplicate sources of truth; combat effects through GAS).

**Evidence.**
- `Sov.Status.Apply.Corruption` is routed only to `USovLegacyCorruptionComponent` (`P/Private/Components/SovStatusComponent.cpp:611-645`). The request is rejected as `RejectedIneligibleTarget` when that component is absent.
- The player owns the new `USovCorruptionComponent` (`P/Private/Characters/SovPlayerCharacterBase.cpp:37`), and managed restore refuses a legacy owner (`P/Private/Components/SovCorruptionComponent.cpp:591-595`, test `SovCorruptionRuntimeTests.cpp:519-541`).
- `SovCorruptionFieldVolume.cpp:187-188` and `SovCorruptionRemedyVolume.cpp:101-102` also target only the legacy component.

**Failure sequence.** An enemy attack authored with the documented status request tag silently applies no exposure, and a field volume placed by a designer does nothing in a campaign mission.

**Fix.** Route the status request to `USovCorruptionComponent` through its accepted-hit producer path. Deprecate the legacy volumes and component with validation errors.

### PC2-13 — No super-armor windows are produced for committed actions [P2, missing]

**TDD:** §6.6 ("Tarrik has high base poise and several super-armor windows during committed actions; Selene fewer"); §5.2.1.

**Evidence.** `State_Poise_SuperArmor` is read by the resolver (`Arsenal/Private/GAS/NarrativeAttributeSetBase.cpp:734,791`), `SovGameplayAbility_TarrikCinderSlam.cpp:100`, `SovSelenePayload.cpp:205` and `SovAurelionThermalFractureComponent.cpp:304`. Nothing in source adds it. Native melee nodes have no super-armor field (`SovMeleeAttackDefinition.h:10-42`).

**Fix.** Add a per-node super-armor interval to `FSovMeleeAttackNode`, and an ability-owned tag window in the Echo and melee bases, driven by data.

### PC2-14 — The native attack definition cannot express TDD heavy, ranged or burst branches, and lacks several B.1 fields [P3, source-proven; secondary to PC2-01]

**TDD:** §6.8 ("Heavy and ranged transition branches must be explicitly authored"); §6.9 (Cinderline burst from melee transition nodes); B.1.

**Evidence.** `FSovMeleeAttackNode` has one `NextNode`/`FollowUpInput` and one `DefensiveInput`, which must be an Evade (`P/Private/Melee/SovMeleeAttackDefinition.cpp:28-31`). It has no owner or archetype permissions, telemetry ID, schema version, AI use rules, accessibility annotation, hit-stop/camera/VFX/haptic request IDs, Echo gain rule or finisher eligibility. Prior coverage risk 5 is still open.

**Fix.** Extend the existing node struct with a small branch list (input tag to node or ability) and the B.1 metadata. Keep the graph finite (`SovMeleePolicy.h:13-14`).

### PC2-15 — Player-combat wiring evidence exists only on this workstation [P3, acceptance risk under a deliberate policy]

The content policy deliberately tracks only `Content/Aurelion` plus a few ability assets. As a result, the abilities, input and loadouts that make the native player kit playable are local-only (`.gitignore:54-60,70`). This includes `AC_*`, `PD_*`, `IC_*`, `WI_Velkorran`, `WI_Staccato`, `WI_Axiom`, `IMC_Combat`, `DA_CombatInputs`, `GA_Evade`, `GA_Sprint`, `GA_Tarrik_Guard`, `GA_Selene_Deflection`, `BP_SovTarrik`, `BP_SovSelene` and `BP_SovPlayerController`.

No automated check asserts this wiring. The existing content tests (`SovDemoLoadoutValidationTests.cpp`, `SovCampaignContentValidationTests.cpp`) were not traced in detail for these assets.

**Fix.** Either track the player-kit assets or add a content-validation automation test that loads them and asserts the grants. Run it as part of engineering acceptance on the workstation that owns the content.

## Prior-finding dispositions

| ID | Disposition | Evidence (current code) |
|---|---|---|
| PC-01 active melee ignores poise break | **Fixed** in the native class; not applicable to protagonists (PC2-01, PC2-02) | `SovGameplayAbility_Melee.cpp:29-41` adds Poise.Broken, Guard.Broken, Frozen, Fatal and others to the interruption set. `:99-123` binds tag events that call `FinishMelee`. `:77-98` extends `ContextValid`. Test `SovCombatActionTransactionRuntimeTests.cpp:216-235`. |
| PC-02 Echo payment resumes after cancellation | **Fixed** | `SovGameplayAbility_Echo.cpp:220-247` fences the epoch around `TrySpendEcho` and never writes stale results. `:355-376` commits and pays before Narrative `Super::ActivateAbility`, with rechecks. Scope-locked end at `:435-440`. Tests `SovCombatActionTransactionRuntimeTests.cpp:88-113,115-136,138-159,161-186,237-266`. |
| PC-03 ammunition not validated or atomic | **Fixed** | `Arsenal/Private/Items/WeaponItem.cpp:484` rejects `Amount<=0` and reentry. `:517-523` reserves the clip before exact inventory removal. `:524-536` refunds only when unreplaced and fails otherwise. Tests `SovResourceTransactionRepairTests.cpp:62-141`. |
| PC-04 Judgement detonates behind cover | **Fixed** | Eye-to-muzzle bridge on Visibility and weapon channels, plus backward-shot rejection, at `SovGameplayAbility_TarrikEcho.cpp:1244-1273`. Tests `SovCinderJudgementGeometryTests.cpp:106-157` and `SovCombatInterruptionRuntimeTests.cpp:156-175`. |
| PC-05 Judgement re-reads mutable ownership | **Fixed** | Immutable `FJudgementShotContext` at `SovGameplayAbility_TarrikEcho.cpp:863-898` and `IsJudgementShotCurrent` at `:900-907`. `ApplyJudgementDamage` uses `Shot.SourceASC` and a native receipt (`:1383-1464`), not aggregate deltas. Tests `SovCombatInterruptionRuntimeTests.cpp:177-255`. |
| PC-06 replay ledgers unbounded | **Partially fixed** | Selene perfect-deflection ledger bounded at `SovSeleneEchoGenerationComponent.cpp:274-292`. Still unbounded: Tarrik `ConsumedCombatTransactions`, `ConsumedHeavyAttacks` and `ConsumedProtectionTransactions` (`SovTarrikEchoGenerationComponent.h:171,185-186`; `.cpp:900-901,921,948`), and Selene `ConsumedDamageTransactions`, `ConsumedCommandLinkSeverTransactions` and `ConsumedBypassAttempts` (`SovSeleneEchoGenerationComponent.h:184,189-190`; `.cpp:316,421-422,516`). `SovResonanceComponent.cpp:215` uses the naive clear-at-256 pattern the prior audit warned against (low impact: it only re-offers). |
| PC-07 Selene effect-class fields ignored | **Still open** | Properties at `SovGameplayAbility_SeleneEcho.h:86-98,146-152,217-224,388-397` are still assigned only in constructors, with no validation. `SovGameplayAbility_SeleneAxiomNullPulse.cpp:173-175,380,591-596` shows the validate-or-fallback pattern to copy. |
| PC-08 recoil presets reversed | **Still open** | `Arsenal/Private/Items/RangedWeaponItem.cpp:214-215` still selects `Hip*` when aiming. Still observable because `GA_Weapon_Aim` is granted by `WI_Cinderline`, `WI_Staccato` and `WI_Axiom`. ADS removal did not remove Narrative aiming. |
| ED-01 nested damage overwrites committed hit | **Fixed** | Each setter reads the live resource: `NarrativeAttributeSetBase.cpp:711-743`. Fatal re-validated against current Health at `:860-881`. Tests `SovCombatRoutingRuntimeTests.cpp:301-354,429-455`. |
| ED-03 Deflection continues after cancellation | **Fixed** | Cleanup claimed before `BeginDeflection`, with `CanContinue` after every boundary (`SovGameplayAbility_SeleneDeflection.cpp:125-195`). Scope-locked end at `:207-212`. Owned flag set before tag publication (`SovDeflectionComponent.cpp:270`). Epoch recheck after `OnDeflectionStarted` (`:167-171`). Tests `SovDeflectionRuntimeTests.cpp:54-125`. |
| ED-06 Shield/Poise recovery lacks death and avatar fence | **Fixed** | Life-epoch, avatar and IsDead/Fatal gates at `SovShieldComponent.cpp:684-698,1355-1361` and `SovPoiseComponent.cpp:631-645,1130-1136`. Tests `SovPassiveDefenseRuntimeTests.cpp:64-356` and `SovPassiveResourceOwnershipTests.cpp:95-350`. |

## Test quality notes

- Native lifecycle tests are real GAS activations with reentrant probes and assert concrete invariants: debit counts, active state, bound delegates, Busy counts. They are good regression assets.
- They exercise the native classes through fixtures (`USovMeleeRuntimeTestAbility`, `USovCombatActionTransactionEchoAbility`), not the shipped Blueprint kit. No test asserts that `WI_Velkorran` or `WI_Verity` grant the native melee, that `IMC_Combat` binds threat focus, or that any protagonist attack honours Poise break.
- `SovMeleeRuntimeTests.cpp:74-105` moves a test mesh with `SetWorldLocation`. It does not evaluate real montages or root motion (prior coverage risk 1 still stands).
- No tests exist for: out-of-encounter sprint drain (PC2-03); scaled perfect windows (PC2-04); Tarrik cadence against the §7.3 table (PC2-05); mark or command-target production (PC2-06); respawn protection ending on attack (PC2-11); Staccato Zero physical-material routing (PC2-10).

## Domain alignment estimate

**Estimate: 42–56 % of §4–§7 and A.1–A.3/B.1 requirements genuinely met. Midpoint 49 %.**

**What is strong.** The numeric core and the transaction and lifecycle engineering. The damage resolver, shields, stamina and evade profiles, guard and deflection windows, the Echo meter and thresholds, poise states, status definitions, fatal recovery, difficulty presets, semantic input with toggles, and the revised ability rosters are implemented, match Appendix A values, and are fenced against reentry with real GAS regressions. All five prior P1s in this domain (PC-01..05, ED-01, ED-03) are fixed.

**What pulls the estimate down.** Several TDD pillars are engineered but not connected to the playable protagonists:
- The native melee framework drives only enemies. Players swing Narrative Blueprint combos, which puts §6.8, the §7.3 heavy reward and the buffer, cancel and charge rules outside the tested path.
- Hard lock, marks and command targets have no input, producer or eligible targets.
- Finishers, Resonance targets and corruption sources are absent from the Aurelion missions.
- There is no protagonist camera owner or profile, super-armor is never produced, and three status families are missing.
- Two economy and pacing defects (the encounter latch and the Cinderline cadence source) and an accessibility gap (defence-window scaling) are source-proven.

**Weighting.** By section: §4 about 45 % (input strong, camera weak), §5 about 45 % (kits and equipment present, cadence and progression content absent), §6 about 52 %, §7 about 55 % (meter solid, generation, Resonance and corruption content gaps).

**Uncertainty.** The spread reflects Blueprint graphs I could not read. A Narrative combo or reaction montage might already cover some of PC2-01 and PC2-02 behaviourally, which would push the figure up. Content that fails the §5.2.3 and §5.3.3 cadence targets would push it down.
