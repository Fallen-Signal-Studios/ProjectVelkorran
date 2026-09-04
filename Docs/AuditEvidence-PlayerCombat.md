# Player combat and shared resource audit

Scope: current `ProjectVelkorran` source snapshot; August 14 2026 campaign TDD v2, especially §§5–7, and `Docs/CampaignV2ChangeLog.md`, `NarrativeFoundationPass.md`, `SeleneCoreLoop.md`, `CombatSustainAndCinderlineDamage.md`. Paths below are relative to the repository; `NA` abbreviates `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal`.

This is a source audit, not proof that binary Blueprint subclasses, weapon definitions, montages, input configurations, levels, or effects are correctly assigned. Initial missing source files were being materialized during review; negative claims below are bounded to implementation inspected rather than transient filesystem absence.

## Overall verdict

There is a substantial coherent combat foundation, not a blank game. The existing Narrative ASC/AttributeSet should be preserved. Tarrik has more native offensive payload than Selene, but neither complete TDD combat loop can be established from source. The largest immediate playable gap is Selene's earn-to-spend loop: Deflection, authored weak points, command links, and Echo rewards exist; the five revised Echo abilities predominantly declare configuration rather than execute the advertised gameplay. A native Axiom Null Pulse is the highest-value bounded extension because it connects these already-existing systems and the Dominion Handler encounter architecture.

Approved post-TDD changes are not defects: revised Tarrik/Selene kits, player Health regeneration, finite Cinderline magazines, transient ammo/Echo drops, and deterministic range damage are explicitly recorded in `Docs/CampaignV2ChangeLog.md`. Do not replace them with the TDD's old prototype names or remove their mechanics.

## Issue records

### PC01. Selene's native Echo expenditure does not deliver the advertised control kit [P0 playable gap]

1. Exists: `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_SeleneEcho.cpp` declares Stillpoint Grenade (17–46), Dispatch (48–79), Staccato Zero (81–107), Axiom Null Pulse (109–188), and Verity's Wake (190–218), with input, costs, weapon family and payload-property checks. Axiom adds `TrySeverAxiomCommandLink` (128–177), forwarding to the existing `USovCommandLinkComponent` and Selene generator. Shared `USovGameplayAbility_EchoBase` does authority spending, predicted presentation, cancellation and a maximum-duration failsafe.
2. Required: TDD §§5.3, 7.4 requires precision-created openings and meaningful system disruption. Approved revised kit replaces original names/payloads but preserves this purpose. Axiom's own description promises charged directed EMP, Shield collapse, recharge suppression and eligible device disable.
3. Missing/incorrect: no native activation/charge/release/target acquisition, shield damage, recharge-block application, device disable, or recovery in the baseline Selene classes. Assigning valid properties can satisfy `HasRequiredPayloadConfiguration` and allow spending without delivering any payload unless an unavailable Blueprint graph performs it. Existing helper validates maximum range and authority, but not pulse cone, LOS, charge-derived range or once-per-pulse authorization.
4. Extend Axiom in place first. Preserve public class, configured assets, Echo transaction and command-link ownership. Do not create a second EMP or link system. The other four abilities remain separate slices.
5. Dependencies: actual Axiom weapon-class allowlist and granting configuration; Narrative targeting/CharacterVisual ownership; Shield component/tag; compatible native Gameplay Effects; CommandLink lifecycle; participant specialist ability cancellation; weak-point reveal; source identity/team rules.
6. Risk: medium. Main risks are paying twice, existing Blueprint duplicate effects, severing through walls, multi-component duplicate hits, disabling immune/boss systems, active-tag/input lock leaks and spending on a stale weapon.
7. Order: first implementation slice. Validate the existing guard/Echo damage foundation alongside it, then author M02 encounter proof.

### PC02. Tarrik has three native payload paths, two configuration shells, and release still depends on Blueprint for two paths [P1]

1. Exists: `SovGameplayAbility_TarrikEcho.cpp`: Cinder Slam constructor/configuration check (187–213); Hunger native projectile/effect defaults (215–298), `ReleaseVelkorransHungerFromAim` and once-only projectile release (301–506); Sticky Grenade defaults/ballistic release (509–802); Judgement native automatic release/recovery and damage/explosion/presentation (804–1685); Requiem constructor/configuration check (1688–1715). Hunger reuses Cinder Grenade Burn rather than a second Burn stack.
2. Required: approved kit must make Tarrik's pressure, heavy commitment, control and forward movement playable; TDD §§5.2, 6.8 and approved roster supersession.
3. Missing: Slam has no native radial damage/ward payload; Requiem has no penetrating/chained detonation payload. Hunger `ActivateAbility` (240–249) and Grenade `ActivateAbility` (537–546) only reset release flags then call shared activation; a Blueprint hook/notify must call release and end recovery. These are real native payloads but not self-executing CDO abilities. Binary integration remains unverified.
4. Preserve Hunger/Grenade/Judgement. Extend Slam and Requiem independently. Add native fallback release/recovery only with migration to avoid doubling existing notify-driven releases.
5. Dependencies: correct weapon granting source object/allowlists, animations/notifies, projectile meshes/collision, shared Burn GE, combat lane geometry, target filters and Poise.
6. Risk: medium/high for existing notify-driven assets; low/medium for new missing payloads. Cinder content already used by creator must not regress.
7. Order: after Axiom and core encounter integration; finish the first required signature before late-campaign kit completeness.

### PC03. Tarrik's identity-defining Echo generation is incomplete [P1]

1. Exists: Guard perfect defense gives +12 and guarded counter hit +10 (`SovGuardComponent.cpp:355–462`). `USovTarrikEchoGenerationComponent` contains authoritative Cinderline cadence/precision kill rewards, source-weapon verification, timeout/cooldown, fatal/active-Echo/full-meter filtering, replicated cadence and owning-client reward events (`SovTarrikEchoGenerationComponent.cpp:278–420, 533–641, 695–784`). Pickups add approved sustain.
2. Required: TDD §7.3 also calls for enemy Poise break +15, ally/civilian intercept +15 with cooldown, heavy attack hitting 3+ targets +8, and command-target execution +8. Receiving unguarded Health damage must give zero.
3. Missing: no source event consumers/native tags for those four remaining reward types in the inspected generation implementation. Cinderline cadence is an approved addition, not a replacement for all melee/protection rewards. Counter reward exists but its attack payload/branch must be authored with `Sov.Damage.Source.GuardCounter`.
4. Extend existing Tarrik generator and typed damage-result event path; retain Guard ownership and Cinderline behavior. Do not grant generic Echo per raw damage or incoming hit.
5. Dependencies: authoritative Poise-break result, attack-instance hit ledger for three-target heavy, ally intercept attribution, command marks and target-execution lifetime.
6. Risk: medium, mainly duplicate awards and ability self-funding. Ally/command additions should wait for their real systems rather than guessed tags.
7. Order: deterministic hostile Poise-break reward/counter validation early; cleave reward after attack ledger; ally/command rewards when those vertical slices exist.

### PC04. Selene generation deliberately covers only three reward sources [P1]

1. Exists: `SovSeleneEchoGenerationComponent.cpp:309–367` gives +10 perfect Deflection and +8 first authored weak-point break; 238–273 consumes unique command-link Sever transactions for +12. Source/target identity, hostility, authored broken zone and Echo-ability filtering are explicit. Body shots do not impersonate precision.
2. Required: TDD §7.4 additionally specifies mark/exposure-window kills +6, distinct-target precision chains +4 capped, and undetected live-threat bypass +15 encounter-limited. TDD says hitting an unbroken weak point; current explicit source docs define the reward as the first break, a narrower phase-one choice.
3. Missing: mark/chain/bypass event sources and reward consumers. Deflection currently negates the damage but does not expose/reposition/reflect the attacker; phase-one docs explicitly defer these.
4. Preserve robust three-source transaction ownership; extend with real marked-target, threat and attack-instance records. Keep physical projectile reflection a separate contract.
5. Dependencies: mark state, stealth/detection encounter state, deterministic target-chain identity, Deflection follow-up design, checkpoint lifecycle.
6. Risk: medium. Reward farming on resets, dead/friendly targets, self-funded Echo casts and active-state callbacks are principal hazards.
7. Order: Axiom first; then marks/exposure and a Deflection payoff, later stealth/bypass integration for M02.

### PC05. Shared signature-readiness feedback disagrees with revised ability thresholds [P1 concrete mismatch]

1. Exists: `SovEchoComponent.h:169–173` defaults Resonant threshold 75 and SignatureReady threshold 100; `SovEchoComponent.cpp:365–386` emits thresholds. Tarrik Slam/Requiem and Selene Dispatch have 90 activation threshold/cost (`SovGameplayAbility_TarrikEcho.cpp:191–192,1691–1692`; `SovGameplayAbility_SeleneEcho.cpp:51–52`).
2. Required: TDD §7.5 readiness feedback must truthfully communicate available signature release. The revised 90-cost roster is approved.
3. Incorrect: native `IsSignatureReady()` is false at 90–99 even when the revised signature can activate. A generic Echo-only threshold also cannot express missing weapon/configuration/cooldown or story unlock.
4. Refactor feedback to query the equipped/granted signature's actual activation readiness or derive the meter threshold from the approved ability definition; preserve numerical Echo storage. Do not revert revised cost to 100 solely to fit UI.
5. Dependencies: HUD binding, weapon/loadout active ability and unlock/cooldown state.
6. Risk: low/medium, mostly existing Blueprint assumptions and ready cues.
7. Order: first HUD/ability-readiness slice following a functional spender.

### PC06. Echo encounter/checkpoint API exists but end-to-end wiring is not demonstrated [P0 mission gate / P1 implementation]

1. Exists: `SovEchoComponent.cpp:227–280` has authored restore, begin and reserve-normalizing end; `TickComponent:43–91` enforces 6-second inactivity then 8/s decay toward 25. Every Add/Spend marks activity. Echo is stored in Narrative AttributeSet, not duplicated in the component.
2. Required: TDD §7.2: encounter exit normalizes to mission reserve, checkpoint restores authored value, no exploit from pre-death meter.
3. Missing: repository search finds no native callers of BeginEncounter/EndEncounter/RestoreEchoFromCheckpoint outside their declarations/definitions. Binary level Blueprint callers cannot be assessed. Initial Echo is 0 (`NA/Private/GAS/NarrativeAttributeSetBase.cpp:20–25`) and needs authored opening reserve. Pausing for an M01/M02 scene cannot be assumed to reset resource correctly.
4. Extend mission/checkpoint integration to call existing APIs; preserve component and one AttributeSet truth.
5. Dependencies: encounter start/finish/failure manager, authored checkpoint record, protagonist handoff and level transition owner.
6. Risk: high if save semantics change without versioning; moderate for explicit encounter integration. Avoid awarding 25 every frame/infinite entry triggers.
7. Order: immediately after Axiom when making Level 1/2 replayable from checkpoint.

### PC07. Successful full-meter Selene Deflections do not refresh Echo combat activity [P2 concrete bug]

1. Exists: Echo component observes legacy OnDealtDamage/OnDamagedBy, accepting only >0 applied Shield+Health (`SovEchoComponent.cpp:417–445`). Deflection's authoritative result intentionally applies zero damage (`NA/Private/GAS/NarrativeAttributeSetBase.cpp:390–409`) and legacy notification immediately returns at zero (179–205). Selene reward handler returns when full (`SovSeleneEchoGenerationComponent.cpp:167–177,309–318`).
2. Required: TDD §7.2 says active guarding/defense participation refreshes decay regardless of resource cap.
3. Incorrect: at 100 Echo, successfully Deflecting without another activity does not call AddEcho or RecordCombatActivity. After six seconds the player can lose Echo despite repeatedly performing successful Deflections. Once below cap, the next reward refreshes it. Guard avoids this because it calls AddEcho even at cap.
4. Extend shared activity listening to typed damage outcomes or explicitly record successful Deflection before reward-cap filtering; do not award overflow.
5. Dependencies: typed result dispatch, authority-only activity, deflection success one-shot semantics.
6. Risk: low; avoid classifying rejected/invulnerable hits as activity accidentally.
7. Order: small fix with Echo activity/feedback validation; does not outrank Axiom.

### PC08. Damage transaction supports most ordering but zero-damage Poise/status-only packets fall outside it [P1 architectural limitation]

1. Exists: `NA/Private/GAS/NarrativeDamageExecCalc.cpp:195–370` handles immunity, friendly-fire, deterministic hit location, source/ability/difficulty/armor/resistance and emits one Damage packet. `NarrativeAttributeSetBase.cpp:208–651` resolves Guard/Deflection, Shield bypass/coefficient overflow, Health, Poise, requested status, death, then typed target/source notifications. AttributeSet names differ from the TDD but storage is singular and compatible with Narrative assets.
2. Required: TDD §§6.3–6.7 needs one ordered routing contract, separate coefficients and disruption/control which need not damage Health.
3. Limitation: execution returns when BaseDamage <= epsilon (281–284) and only publishes if ResolvedDamage > epsilon (362–369); an explicitly authored zero-damage hit with positive SetByCaller PoiseDamage will never reach routing. Legacy direct PoiseDamage meta branch (`NarrativeAttributeSetBase.cpp:654–695`) adjusts Poise but does not publish the same FSovDamageResult or run normal defense/team/channel path. Status-request event is emitted only if applied Shield/Health/Poise >0 (591–598). Mixed damage channels intentionally have one scalar and only reject when all declared channels are immune (ExecCalc 140–184); no split per-channel mitigation.
4. Preserve the main architecture. Extend the unified packet when control-only attacks are introduced. Do not add a duplicate Sov AttributeSet or solve control-only packets using unchecked direct attribute writes. Axiom can use positive Shield-only damage with Health coefficient zero and an independent duration GE without changing this subsystem now.
5. Dependencies: all weapon/projectile effects, typed listeners, current weak-point/sever/drop claims, status definitions.
6. Risk: high if wholesale replacement, moderate with focused backwards-compatible transaction extension. Need coefficient and zero-magnitude test matrix.
7. Order: after first Axiom payload; before introducing zero-damage heavy/knockback/status packets. Per-channel weights only when an authored attack requires them.

### PC09. Defenses are native, but guard cancellation is weaker than the newer Deflection/Echo adapters [P1/P2]

1. Exists: `SovGuardComponent.cpp:102–165,355–462` owns guard/perfect/counter/break tags and one-hit perfect-window consumption. Damage route implements facing, standard/heavy/unblockable, start 8, perfect 5, ordinary 8–20. Selene Deflection is a separate component and ability, not copied sustained Guard. Existing full-meter/window exhaustion outcomes are deliberately documented.
2. Required: TDD §§4.7,6.5,6.12: clean exhausted/cancelled defense, no ordinary mitigation for heavy/red attacks, death/ragdoll/cinematic safety.
3. Weakness: `SovGameplayAbility_TarrikGuard.cpp:18–24,216–237` blocks initial Busy/Fatal/Poise etc. but observes only death, PoiseBroken and Sequencer while active, not ragdoll/interacting/new Busy/Fatal independently. Newer Echo/Deflection adapters cover more cancellation states and close activation-to-binding races. Actual guard counter animation/attack must exist in binary content. No source proof of all required input/attack-class assignments.
4. Refactor Guard cancellation using the existing tag lifecycle pattern, preserving defensive balance and public component events. Extend actual counter branch in weapon ability content rather than inventing another guard.
5. Dependencies: Narrative input and weapon equip state, death state convergence, ragdoll/interactions, montage cancellation.
6. Risk: medium because adding Busy cancellation can cancel guard unexpectedly if a guard montage itself grants Busy; test the actual asset graph first.
7. Order: defense reliability slice before dense encounters; no verified showstopper displacing Axiom.

### PC10. Shield/Health/Poise are implemented; balance and content gates remain [P1 integration]

1. Exists: `SovPlayerCharacterBase.cpp:15–47` owns Echo, Shield, player-only Health recharge and Poise. `SovShieldComponent.h:131–135` sets 3s and 5%/s; native overlay discovery/rebinding/MIDs and recharge-block tag are implemented. `SovHealthRechargeComponent.cpp:295–355,391–489` restarts on applied Shield/Health/Poise/guard-Stamina and regenerates after a delay; owner must be Narrative player; no revive. `SovPoiseComponent.cpp:189–208,336–373,600–807` supports stable/pressured/broken/recovering, timed fallback, recovery immunity and tags; AttributeSet applies a floor for recovering/super-armor (567–575).
2. Required: TDD resources, distinct protagonist profiles, Shield break vulnerability/feedback, authored stagger/finisher windows. Health no-natural-regeneration rule is explicitly superseded by approved creator decision.
3. Missing/unverified: binary initial stats/difficulty profiles, shield-break vulnerability GE/audio/material hookups, hit reactions and finisher implementation. Resource components being present is not proof Tarrik has higher Shield/Poise or Selene the intended lower profile. Native code does not replace required authored animations/effects.
4. Preserve components and ordered routing; extend data validation and selected content integrations. No new duplicate resource stack.
5. Dependencies: Player/NPC definitions, damage GEs, appearance setup, tags, AnimBP and HUD.
6. Risk: medium for tuning and reparenting; high for wholesale replacement. Migration docs require removing Blueprint duplicate components.
7. Order: mandatory authoring smoke pass for first encounter; fuller balance after both base loops work.

### PC11. Stamina, movement, combos and weapon transitions need binary/content verification [P1 integration / P2 completeness]

1. Exists: Narrative AttributeSet exposes replicated Stamina, MaxStamina and StaminaRegenRate; defense cost checks/consumption are native. Concrete `ASovTarrikCharacter` and `ASovSeleneCharacter` differ in resource-generation/defense components and canonical identity. Shared Echo weapon gates inspect actual wielded/granting weapon objects (`SovGameplayAbility_Echo.cpp:465–522`).
2. Required: TDD §§4–6: protagonist-specific speed/evade/camera, delayed/regime-dependent stamina regen, finite light chains, explicit heavy/ranged transition branches and input buffering, bespoke Verity collision/animation.
3. Missing/unverified: no native stamina regeneration policy or spend/regen-delay observer in inspected source; implementation may be existing Narrative Blueprint GEs. Native protagonist constructors do not establish distinct movement/camera/stat profiles. Generic attack/weapon framework cannot prove authored 3–4 link Tarrik chain, Selene precision exits, costs or montage windows. Shared Echo gate checks weapon identity but does not itself require `IsWielded()`/visual Ready and does not observe weapon change during a long cast; actual Narrative wield/equip lifecycle must be exercised.
4. Preserve Narrative movement/weapon/input systems and extend authored definitions, native validation or narrow policy where needed. Do not create parallel combat state, stamina meter or weapon inventory.
5. Dependencies: imported assets, AnimBPs, montages, input mappings, Narrative AbilityConfigurations, weapon presentation state and stamina GE.
6. Risk: medium/high if blindly changing existing GEs while adding native regen (double regeneration); medium for narrow validation.
7. Order: content inventory/smoke test early; movement and combo feel slice after one working offensive/defensive loop per character.

### PC12. Automated coverage proves construction/contracts more than combat behavior [P0 verification gate]

1. Exists: `Source/ProjectVelkorran/Private/Tests/SovCampaignFoundationTests.cpp` has compile-time inheritance assertions and CDO/component/default/tag checks (45–325); source docs list manual functional/PIE matrices. This is useful scaffolding.
2. Required: user asks compile/test; TDD §§6.15 and acceptance criteria require real damage/resource/action outcomes and end-to-end encounters.
3. Missing: no baseline executable functional test of Selene activation -> payment -> timed release -> target resolution -> link sever -> reward -> recovery; no equivalent established tests of guard boundary, repeated callback, resource normalization and checkpoint reset. CDO defaults do not prove gameplay succeeds. Engine/editor availability and binary assets limit actual build and PIE execution, which must be stated honestly.
4. Extend tests around existing production code and real GAS/World actors; preserve smoke tests but do not inflate them into runtime proof.
5. Dependencies: UE5.7 build tools/runtime, actual plugin dependencies, minimum fixture/map, Blueprint compile commandlet.
6. Risk: low for tests; avoid mocks that only mirror implementation math and conceal GAS lifecycle errors.
7. Order: add meaningful Axiom functional coverage in the first slice and execute UE build/automation when available; record explicit blocked gates separately from passing checks.

## Recommended first vertical slice

**Axiom Null Pulse: earn, charge, disrupt, exploit.** Extend `USovGameplayAbility_SeleneAxiomNullPulse` and existing CommandLink APIs. Add native Shield-only damage, recharge-suppression/device-disable duration effects and minimal native presentation support or exposed events. Reuse existing overlap/CharacterVisual ownership/hostility/LOS patterns and one transaction resource owner.

Validation: at 30 Echo, a valid Axiom cast spends exactly once; charged cone changes range/angle/suppression within authored bounds; only live hostile in-cone visible eligible targets change; no Health damage from EMP; Shield suppression expires and recharge resumes; one active command link severs once and gives exactly +12; participant abilities cancel and red weak-point reveal occurs; an already severed link cannot farm reward; cancelled/failed casts clean tags/timers; repeated release cannot duplicate effects. Check zero Shield, immunity, boss resistance, friendly target, CharacterVisual multi-component overlaps, behind wall, near/far edge, weapon swap, death/cinematic/ragdoll and input release races.

Definition of done: native authority gameplay reaches recovery/end with no Blueprint gameplay required beyond granting/configuring Axiom, existing authored BP gameplay migration is documented, effects are shared/owned consistently, automated behavior tests exist and run where UE is available, content/editor-only checks are explicitly gated rather than claimed complete.

Next player-focused slices: (1) encounter/checkpoint/resource/input integration to prove M01/M02 restart cleanly, (2) Tarrik counter + hostile Poise-break Echo and first complete signature, (3) Selene Deflection payoff/mark/exposure with remaining revised spenders staged one at a time.
