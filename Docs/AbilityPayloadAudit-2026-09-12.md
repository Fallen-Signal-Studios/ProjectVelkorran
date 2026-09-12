# PC01–PC04 per-ability payload audit — 12 September 2026

Each ability traced independently, end to end: **activation → targeting/commit → payload creation →
application → observable gameplay result**. Existence of a class, callback, tag or entry point was not
accepted as closure; every CLOSED verdict below rests on an automation test asserting the observable
outcome.

Evidence came from current source and the test registry. Audit line citations were not trusted:
`SovEchoComponent.cpp:417–445` is unchanged and still misleading, because the behaviour it describes
moved to another component.

**A search caveat worth repeating.** These suites assert through gameplay tags and numeric payoffs,
not C++ symbol names. Searching for `Echo_Source_HeavyMultiHit` returns nothing; the test that proves
it reads *"Three distinct heavy victims award 8 once"*. Symbol-name greps under-reported coverage three
times during this audit. Search for behaviour.

## Verdicts

| Finding | Verdict |
|---|---|
| PC01 — Selene Echo expenditure does not deliver the control kit | **CLOSED** |
| PC02 — Tarrik release depends on Blueprint for two paths | **CLOSED** |
| PC03 — Tarrik Echo generation incomplete | **CLOSED** |
| PC04 — Selene generation covers only three reward sources | **PARTIAL** |

## PC01 — Selene spenders (CLOSED)

| Ability | Activation → payload → application | Observable-result evidence |
|---|---|---|
| Axiom Null Pulse | native charge/release, cone selection, `HasAxiomLineOfSight` occlusion tracing | 12 suites under `Campaign.AxiomNullPulse.*`, incl. `ReleaseAndDamage`, `TargetingAndImmunity`, `DeviceEligibilityAndEffectOwnership` |
| Staccato Zero | paid precision shot | `SelenePayload.ZeroPaidPrecision`: *"One Echo spend"*, *"Exactly one direct damage transaction"*, *"Damage scalar is applied once"* |
| Stillpoint Grenade | field application incl. late entrants | `SelenePayload.StillpointFieldAndLateEntrants`; `FrozenDOTStopsOnThaw`: *"Frozen target receives actual periodic damage"*, *"Thawed target receives no frozen-only damage"* |
| Verity's Wake | lane payload with cover | `SelenePayload.WakeLaneAndWall`: *"Center receives one hit"*, *"Flank receives one hit"*, *"Wall shields downstream target"* |
| Dispatch | outbound + recall ledger | `SelenePayload.DispatchRecallAndCancellation`: *"Outbound path applies one target hit"*, *"Same target can be hit once on return"*; plus `DispatchQueuedGASRecall` |
| Deflection | zero-damage defensive resolution | `SelenePayload.DefenseAndImmunity`: *"Perfect deflection negates damage"*; `Campaign.Deflection.*` reentrancy suites |

The audit's core claim — that valid properties could satisfy `HasRequiredPayloadConfiguration` and
allow **spending without delivering a payload** — is directly refuted by `ZeroPaidPrecision`, which
asserts the spend *and* the resulting damage transaction in one test.

## PC02 — Tarrik payloads and release (CLOSED)

Both specific claims are stale:

- *"Slam has no native radial damage/ward payload."* `ReleaseCinderSlam()` applies a ward spec to self
  via `MakeOutgoingSpec`/`ApplyGameplayEffectSpecToSelf`, performs
  `OverlapMultiByObjectType(... MakeSphere(SlamRadius) ...)`, and applies damage through
  `SovTarrikPayload::ApplyDamage` with `RadialDamageEffectClass`.
- *"Requiem has no penetrating/chained detonation payload."* It carries
  `PenetratingDamageEffectClass` and `LineDetonationEffectClass` with line traces, penetration
  accumulation, and authored spacing/interval.

- *"Release still depends on Blueprint for two paths."* `ActivateAbility` calls `ArmTarrikPayload`,
  a native timer fenced by an activation serial, which invokes `ExecuteAutomaticTarrikPayload()` —
  overridden by all four spenders (Slam, Requiem, Velkorran's Hunger, Cinder Sticky Grenade). The
  Blueprint-callable release remains an **optional** authority-only early release, exactly as
  `ReleaseAxiomNullPulseFromAim` is for Axiom.

Observable-result evidence, `Campaign.Tarrik.*`:

- `CinderSlam.DamageWardAndFiltering`: *"One resolved radial hit"*, *"Hostile Shield reduced"*,
  *"Friendly unchanged"*, *"Occluded target unchanged"*, *"Immune target unchanged"*, *"Ward does not
  grant Shield"*.
- `CinderlineRequiem.PenetrationCoverAndPersistentChain`: *"First enemy penetrated once"*, *"World
  cover stops penetration"*, *"Off-axis hostile receives line only"*.
- `ProjectileRelease.NativeTimerAndWeaponRevalidation` — the native timer itself.

## PC03 — Tarrik Echo generation (CLOSED)

All four reward types the audit called missing are implemented **and** proven with exact values in
`Campaign.Echo.AttackReceiptsAndTarrik`:

| TDD §7.3 reward | Implementation | Test assertion |
|---|---|---|
| heavy attack hitting 3+ targets, +8 | `AttackId`-grouped `FHeavyAttackProgress`, `Targets.Num() >= 3`, consumed once | *"Three distinct heavy victims award 8 once"*; *"Repeated first victim is not a third target"* |
| enemy Poise break, +15 | `Result.bPoiseBroken` | *"General confirmed poise break awards 15"* |
| command-target execution, +8 | `bFatal` + `State_CommandTarget_Window` captured pre-damage | *"Fatal hit in pre-hit command window awards 8"* |
| ally/civilian intercept, +15 with cooldown | `ConsumeProtectionIntercept` + `SovProtectionAwardPolicy::HasCooldownElapsed` | `Campaign.Echo.Protection.DefenseAndReceipt` |

Replay safety is asserted too: *"Result replay does not double award"*. Target-life fencing is covered
by `Campaign.Echo.NativeRewardTargetLife` (*"Neither hero receives an old-life kill payoff"*).

## PC04 — Selene Echo generation (PARTIAL)

Five of six sources are implemented and proven:

| TDD §7.4 reward | Evidence |
|---|---|
| perfect Deflection, +10 | `Echo.PerfectDeflectionTargetLife`: *"A fresh native perfect defense still grants 10"* |
| first authored weak-point break, +8 | `Echo.UnbrokenWeakPointHit` |
| command-link Sever, +12 | `AxiomNullPulse.CancellationWeaponAndLinkReward`, `PreservesOneSeverPerHitCommandNode` |
| mark / exposure-window kill, +6 | `Echo.ExposureAndMarkSingleReward`: *"Status exposure plus mark awards only one six-point payoff"*, *"Fatal result replay cannot award exposure again"* |
| distinct-target precision chain, +4 capped | `SovEchoAwardPolicy::TPrecisionChain` (portable-tested); `Echo.SelenePrecision`, `Echo.PrecisionChainSurvivesEchoAbility` |
| **undetected live-threat bypass, +15** | **no test of the award** |

### The one gap, stated precisely

1. **TDD requirement.** §7.4: undetected live-threat bypass, +15, encounter-limited.
2. **Current implementation.** `USovSeleneEchoGenerationComponent::ConsumeUndetectedBypass` awards
   15.0 under `Echo_Source_UndetectedBypass`, deduplicated by `ConsumedBypassAttempts`.
   `ASovEchoBypassGate` owns entry/exit volumes, threat perception binding and a traversal timer.
3. **Missing behaviour.** None in production — the gap is *evidence*. `SovEncounterRuntimeTests`
   proves the encounter director's attempt receipt claims once and cannot replay after load
   (*"Validated attempt receipt claims once"*), but nothing asserts that a completed undetected
   traversal actually awards +15, exactly once.
4. **Source-only?** Yes, but it needs an AI/perception world fixture. `ConsumeUndetectedBypass` is
   private and mutually friended with the gate — deliberately, *"Gate-owned native receipt only; no
   freely callable boolean reward API"* — so a test must drive the real overlap-and-perception path
   rather than call the award. The existing `FEchoWorld` builds with `CreateAISystem(false)` and
   cannot host it.
5. **Priority.** Low-medium. One reward of six, with its claim side already covered.
6. **Verification path.** Spawn the gate with entry/exit volumes and NPC threats carrying registered
   sight perception; overlap the player through entry then exit inside `MaximumTraversalSeconds` with
   no perception update; assert +15 once and that a second traversal of the same attempt awards
   nothing. **I judged this impractical to add right now** rather than doing it badly: it needs a new
   AI-enabled world fixture, and this host currently takes minutes per verification cycle. It is a
   clean, small, scoped slice — not something to bolt onto this audit.

### Attempt on 12 September, and where it stopped

The AI-enabled fixture was built as specified and is preserved at
`Docs/Attic/SovBypassRewardRuntimeTests.cpp.wip`. It does **not** expose `ConsumeUndetectedBypass`, add
a test friend, or weaken the gate-owned receipt model. It establishes every precondition the gate
enforces: an `ASovEncounterRuntimeTestDirector` seeded Active with a valid attempt id, a registered
hostile threat participant, an `AAIController` carrying a registered `UAIPerceptionComponent` with a
configured Sight sense, a Selene-tagged player carrying `USovSeleneEchoGenerationComponent`, and
box-volume traversal inside `MaximumTraversalSeconds`. Three cases were written: undetected traversal
awards +15 once, detected traversal awards nothing, inactive encounter awards nothing.

**It is not committed to the suite because it does not pass, and the reason is a harness problem rather
than a product one.** The gate requires `EntryVolume->IsOverlappingActor(Player)`, and that never
becomes true in this headless world. Measured, not guessed:

```
player  loc=(0,0,0) capsuleEnabled=QueryOnly objType=Pawn respToWorldStatic=Overlap genOverlap=1
entry   loc=(0,0,0) extent=200 enabled=QueryOnly objType=WorldStatic respToPawn=Overlap
        genOverlap=1 registered=1 hasValidPhysicsState=1
world   OverlapMultiByObjectType(Pawn) at the volume  -> FOUND, hits=1
comps   capsule->IsOverlappingComponent(entry)=0   entry->IsOverlappingActor(player)=0
```

So the capsule **is** in the physics scene and **is** geometrically inside the volume — a world query
finds it — but neither component records the overlap. Tried without effect: initialising the world for
play before spawning (the order the working `SovCorruptionRuntimeTests` fixture uses), `DispatchBeginPlay`
on every actor, explicit `UpdateOverlaps()` on both sides, `RecreatePhysicsState()`, and ticking the
world between moves.

Two hypotheses remain untested, and are the place to resume:

1. **Component mobility.** `EntryVolume` is the gate's root `CreateDefaultSubobject<UBoxComponent>`,
   which defaults to Static. `UpdateOverlaps` has different behaviour for static primitives, and the
   normal gameplay flow depends on the *moving* capsule registering the overlap rather than the volume.
2. **Character capsule overlap bookkeeping.** The shared Axiom fixture character disables movement
   component ticking and ignores all channels but Visibility. Overriding one response may be
   insufficient; a capsule that never moves through the movement component may never run the overlap
   update path that gameplay relies on.

A false start worth recording: the first version of these tests **passed** the two "awards nothing"
cases while the overlap was silently never happening. They were vacuous. Asserting the entry overlap
first is what exposed it, and any resumption should keep that assertion.

I did **not** widen production access to make it testable. Adding a test friend or a public reward
call would weaken the encapsulation the comment deliberately establishes.

## Content and Blueprint dependencies, listed separately

These are **not** source gaps:

- Authored montages, Niagara, sound and HUD feedback for every spender — `AxiomNullPulse.md` and
  `DefenseAndWeakPointEngineering.md` record the editor acceptance routes.
- Blueprint early-release hooks (`ReleaseCinderSlam`, `ReleaseCinderlineRequiemFromAim`,
  `ReleaseAxiomNullPulseFromAim`) are optional authority-only aim releases. Native automatic release
  covers the path when they are absent, so their absence is a content choice, not a missing
  implementation.
- Removing legacy Blueprint damage/status payload graphs from abilities that now own gameplay
  natively, per `AxiomNullPulse.md`'s migration note.
