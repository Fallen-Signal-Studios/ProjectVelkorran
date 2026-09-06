# Combat transaction reliability: first current-main repair batch

## Status and scope

Source changes are based on the exact tree of Mac-tested `main` commit
`caf939e701cdeaabfe22530f025b2c66e0960cb2`. This is a focused reconciliation of
combat repairs, not a wholesale merge of the older audit/PR #32 branch. Native
tag initialization, default-valued save serialization, UE 5.7 compatibility,
the current typed status/corruption bridge, and the single-initialization Mac
test-world setup are retained.

This batch is source-complete pending engine validation. It does not establish
an increased measured TDD completion percentage, a passing campaign playthrough,
a console-qualified build, or an airtight engineering layer.

## Repaired contracts

| Area | Failure addressed | Implementation and retained behavior |
| --- | --- | --- |
| Damage admission | Team-policy callbacks could change source/target life or avatar before admission finished. Depth limits alone allowed explosive branching. | Capture canonical ASC/avatar, registered AttributeSet, Health-life and actor-info generations. Recheck after admission callbacks. Share depth 32 and work 128 limits across nested admission/resolution callback trees. Independent top-level hits start fresh. |
| Damage routing | Earlier resource/death callbacks could make later writes use stale Health/Poise and misattribute a nested kill. Large finite coefficients could overflow. | Debit each resource against its current value, record this packet's contribution before notification, and publish explicit breaks after writes. Saturating finite arithmetic preserves routing; a source policy's approved Health contribution cannot grow after a healing callback. |
| Result ownership | A retained result could replay after weak-point recovery or affect a restored character. | Canonical native receipts share consumption across C++ and reflected copies. Proof retains weak target identity and life/actor-info generations, with at most 32 consumers and 8 channels per result. Weak points no longer retain an actor-lifetime hit GUID set. |
| Status delivery | An early status listener could restore the target before a later listener applied the same request. | Damage-produced typed requests carry the same optional native origin proof. Per-leaf, aggregate-event, handler and structural admission checks reject retired origins. Direct authoritative status requests still work. Magnitude, duration, level, exact tags, native-owned exclusion and source ability provenance are retained. |
| Echo and melee actions | Cancellation during payment/startup or scope-locked teardown could resume an old action; active melee could ignore new interruptions. | Capture activation, exact ASC/avatar/AttributeSet and life generations. Reserve payment and cleanup ownership before callbacks. Retire pending continuations immediately, perform deferred GAS cleanup once, and bind charge timers to their action. Check Frozen, equipment and immediate zero-Health transitions. |
| Ammo and inventory | Negative counts, recursive fire/reload, changed reserve stacks and post-callback net quantity accounting could corrupt loaded ammunition or duplicate grants. | Exact admitted reserve debit; loaded-round reservation before callbacks; conditional refund only for unchanged resources; permission queried once; identity-checked empty-stack cleanup. Count committed grants before callbacks and recompute remaining capacity between writes. Existing Narrative inventory remains the single resource owner. |
| Sustain and rewards | Reentrant overlap could claim the same pickup twice; a later fatal/reward listener could act on a restored target. | Reserve collection before granting, preserve partial ammo remainder and reject payload mutation or dead/unpossessed collectors. Drop and hero reward consumers recheck current target life around mutable callbacks. Selene's source-owned Exposed kill payoff is retained unchanged. |

Object allocation guards do not confer gameplay ownership. Both are checked:
an allocated actor can still be destroyed, unpossessed, restored or rebound.
The ASC actor-info generation detects an avatar handoff away and back, including
`ClearActorInfo`, without changing Narrative's vehicle-avatar retention policy.

## Main implementation locations

The Narrative paths below are relative to
`Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal`.

| Owner | Files/classes |
| --- | --- |
| Canonical GAS transaction | `Public/Private/GAS/NarrativeAttributeSetBase.h/.cpp`, `Public/Private/GAS/NarrativeAbilitySystemComponent.h/.cpp`, `Private/GAS/NarrativeDamageExecCalc.cpp` |
| Native proof and portable rules | `Public/GAS/SovCombatTypes.h`, `Private/GAS/SovDamageConsumptionReceipt.cpp`, `Public/GAS/SovCombatTransactionPolicy.h` |
| Shared inventory and weapons | `Public/Private/Items/InventoryComponent.h/.cpp`, `Public/Private/Items/WeaponItem.h/.cpp`, narrowly `Private/Items/NarrativeCinematicTransaction.cpp` |
| Project actions | `SovGameplayAbility_Echo`, `SovGameplayAbility_Melee`, `SovGameplayAbility_TarrikLifecycle`, `SovGameplayAbility_SeleneEcho`, `SovGameplayAbility_SeleneDispatch`, `SovGameplayAbility_SeleneAxiomNullPulse` |
| Project consumers | `SovWeakPointComponent`, `SovStatusComponent`, `SovCombatSustainDropComponent`, `SovSeleneEchoGenerationComponent`, `SovTarrikEchoGenerationComponent` |
| Project resource actors | `SovCombatSustainPickup`, `SovAmmoCombatSustainPickup`, `SovEchoCombatSustainPickup` |

## Validation performed here

- `python3 -m unittest discover -s Scripts/Tests -p 'Test*.py'`: 30 passing checks,
  including reflected-header and known unity-layout checks.
- `python3 Scripts/Test-NativePolicies.py`: 39 portable C++ suites passed with
  GCC, C++17, warnings as errors and undefined-behavior sanitizer. The production
  transaction policy now has 30 assertions, including saturating arithmetic and
  a branching callback tree that exhausts its finite work budget.
- `git diff --check`: clean.
- Existing source/report scanner: 274 unique native test registrations, with no
  duplicate names. The base had 237; 37 are newly authored in this batch.
- Independent source cross-review of damage/receipt and action/resource boundaries.
- No UE headers, UnrealBuildTool, UnrealHeaderTool or UnrealEditor are installed
  in this workspace. Native tests are authored, not run. An optional Clang rerun
  could not start because `clang++` is not installed; no Clang pass is claimed.

The earlier Mac evidence belongs to the base revision, not these changes:
116 of 237 selected native tests passed and 121 failed in that prior run.
See [UE 5.7 work-PC compatibility](UE57WorkPCCompatibility.md). This batch does
not erase that failure baseline or classify all remaining failures as content.

## Authored regression coverage and engine gate

All files in this table are under `Source/ProjectVelkorran/Private/Tests`.

| Test implementation | New registrations |
| --- | ---: |
| `SovCombatRoutingRuntimeTests.cpp` | 14 |
| `SovCombatActionTransactionRuntimeTests.cpp` | 9 |
| `SovResourceTransactionRepairTests.cpp` | 7 |
| `SovDamageReceiptRuntimeTests.cpp` | 4 |
| `SovHeroRewardLifeRuntimeTests.cpp` | 3 |

New cases exercise real GAS and Narrative inventory paths, not only mirror
policy models. The suite includes nested break/attribute callbacks, target and
source restoration during admission/publication, avatar ABA, finite coefficients,
source-approved damage after healing, reflected receipt copies and bounded
consumers, late status/weak-point listeners, payment cancellation, same-instance
restart, deferred End, immediate interruption, nested ammo consumption,
rejected/replaced/refilled reserves, callback spending, pickup reentry and partial
remainder. Tests retain the Mac single-world-initialization pattern; dynamic
callbacks in non-BeginPlay worlds explicitly opt into execution where required.

Use `Scripts/Validate-Unreal.ps1` with the actual Windows UE 5.7 installation and
project/plugin content available. Build/UHT must run before interpreting tests:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -BuildOnly
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -SkipBuild -TestFilter ProjectVelkorran.Campaign
```

For diagnosis, isolate `ProjectVelkorran.Campaign.Defense`,
`ProjectVelkorran.Campaign.WeakPoint`,
`ProjectVelkorran.Campaign.Transactions.Actions`, and
`ProjectVelkorran.Campaign.Repairs.Resources`. Also rerun existing Echo generation,
status/corruption, protection, inventory/cinematic and finisher regressions: the
shared systems changed their ordering and lifetime admission deliberately.
Run the full native selection afterward and account for missing/not-run tests.

Before accepting the slice:

1. Compile Editor and Development game targets against the real UE 5.7 headers.
   Include non-unity/IWYU checks, generated struct copying, actor-info override,
   const strong-pointer instantiations and new fixture UHT.
2. Pass the new native cases and compare all previously passing Mac tests. Do not
   suppress failed assertions or mask missing assets to create a green report.
3. Verify Guard/Deflection and fatal notification ordering with real characters.
   Check callback-caused heal/restore, action cancellation, and a pickup grant
   whose resource-change listener immediately spends the grant.
4. Validate shared-ammo weapons, client prediction/correction, real overlaps,
   deferred drop construction, component removal and lifecycle cleanup in-engine.
5. Keep the PR draft until the available engine evidence is attached. Packaging,
   authored campaign completion, performance and console testing remain separate
   acceptance gates.

## Remaining work deliberately outside this batch

- Generic status checkpoint capture/restore still needs integration with the
  canonical Narrative save lifecycle. This batch only secures damage-origin
  status delivery, not persistence.
- Save ownership, asynchronous travel failure recovery and save-safe deferred
  finisher outcomes require a separate transaction slice.
- Some hero reward histories still use GUID ledgers. Target-life fencing here
  does not claim all command-link, protection or campaign ledgers are bounded.
- The remaining combat audit includes Cinder Judgement eye-to-muzzle obstruction,
  Reformation drone DeviceDisabled continuations, and passive Shield/Poise writes
  from retired owners. They are not silently claimed fixed by the shared action
  changes in this batch.
- Test fixtures remain in the current game module to preserve the known Mac
  layout. Move them to an Editor test module in a separately validated change;
  Shipping isolation and packaged validation are not established here.
- Timing, animation notifies, authored Blueprint hookups, camera/weapon
  presentation, encounter assets, platform SDKs and console certification are
  not source-only validation.

Next separate source batches: status/lifecycle persistence, save/travel recovery,
then Editor test isolation and repeatable full-suite build evidence. Recheck the
remaining combat liabilities alongside those batches rather than treating the
TDD as an implementation-completeness checklist.
