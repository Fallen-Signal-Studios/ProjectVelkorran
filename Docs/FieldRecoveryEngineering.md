# Field recovery charges

TDD §10.8 requires two Standard field-recovery charges, a fixed percentage heal during a short interruptible action, and refill at major checkpoints or authored supply stations. Approved passive health regeneration remains in place; it does not replace this separate limited action.

## Native ownership and behavior

`USovFieldRecoveryComponent` is a default subobject of `ASovPlayerCharacterBase` and participates in its existing readiness gate. Charges are private saved component state inside the protagonist's existing Narrative PawnRecord. There is no inventory item, consumable crafting system or second save ledger.

| Setting | Default | Design status |
|---|---:|---|
| Capacity | 2 charges | TDD Standard baseline |
| Heal | 35% of current maximum health, clamped to missing health | Editable prototype; TDD does not prescribe the percentage |
| Duration | 1 second | Editable prototype for the short recovery action |
| Consumption | Completed healing | Interrupted recovery preserves the charge by default |
| Early consumption | Disabled | Explicit authored opt-in only |

`USovGameplayAbility_FieldRecovery` uses `Narrative.Input.FieldRecovery` and owns `Sov.State.FieldRecovery` plus one Narrative Busy contribution. It rejects full health, no charges, invalid resources, an unready/dead or mismatched ASC/avatar, conflicting combat/interaction states and a separate cost GE. The component reserves a unique use ID; completion verifies the same action, elapsed duration and live protagonist before committing exactly one charge and the bounded health change.

Real health loss and committed shield/health/poise damage interrupt it. Death, cinematic control, weapon equip, interaction, traversal, ragdoll, frozen state, broken guard/poise and controller/readiness/avatar changes also cancel. An optional montage runs through the existing GAS montage task at the configured use duration; interruption or failed playback cancels. Presentation can use the typed start/end events when a montage has not yet been authored. The action grants no invulnerability or poise protection.

Charge consumption occurs before the health attribute callback, so reentrant callbacks cannot produce a free heal. If a callback replaces loaded state or possession, the already committed charge/heal are retained and stale completion notifications are suppressed. Ending owns and removes only its delegates, timer, reservation, montage task and GAS action tags. A full-health result caused by passive regeneration before completion spends no default charge. Explicit consume-on-start mode retains its already spent charge on interruption.

## Refill and persistence

`ASovFieldRecoveryStation` extends the existing `ASovTechniqueSafePoint`; its native `USovFieldRecoveryStationInteraction` uses Narrative's completed interaction route. It refills only the actual live protagonist inside the safe point, within interaction range and line of sight, with no active recovery or nearby threat. The existing safe-point encounter and action-state checks remain authoritative.

An ordinary Technique safe point does not implicitly refill charges. Mark a major checkpoint with `bRefillsFieldRecovery` and invoke `RefillAtSafePoint` through its actual completed checkpoint interaction, or place a native supply station. The API revalidates the physical actor and line of sight, so a remote Blueprint call or a caller-supplied boolean cannot authorize a refill. Repeated safe visits refill to the existing capacity; they do not increase capacity. This baseline does not add an inventory stock economy.

The existing PawnRecord stores schema, protagonist identity and remaining charges. In-progress mutation/use refuses save serialization. Loading rejects impossible charge values, unknown schema and another protagonist's charge record; inactive counts restore without granting fresh charges. Encounter retry restores its recorded entry count through the existing component restore route. Returning to a saved protagonist retains that protagonist's own count. Brand-new components begin at their authored capacity; existing pre-feature saves without the component record use that first-entry default.

## Validation and editor handoff

Executed: 18,391 production-policy checks covering charge/capacity combinations, dead/full-health rejection, bounded percentage healing, exact completion timing and nonfinite inputs. C++17 compilation used warnings as errors and undefined-behavior sanitization. The combined `Scripts/Test-NativePolicies.py` run passed all 26 suites available at the time of this validation.

Authored UE tests, not run in this workspace:

1. `ProjectVelkorran.Campaign.FieldRecovery.CompletedHealCancellationAndExhaustion`: real ASC healing, minimum duration, unsafe-save rejection, exactly-once consumption, Busy cleanup, immediate health-loss interruption, preserved interrupted charge and exhausted rejection.
2. `ProjectVelkorran.Campaign.FieldRecovery.PawnRecordAndPhysicalRefill`: existing Narrative component record round-trip, no refill on load, distant/unmarked/obstructed safe-point rejection and bounded physical refill.
3. `ProjectVelkorran.Campaign.FieldRecovery.CallbackOwnershipAndEarlyConsumption`: explicit early consumption, cinematic interruption and suppression of stale completion notification after a synchronous restored-state callback.

Unreal build/UHT/automation still require the actual UE5.7 toolchain. Remaining editor work is granting/mapping the native ability in each protagonist definition, selecting the recovery montage and feedback, placing/marking supply checkpoints, and displaying the component's charge count. Validate damage and montage interruption at 30/60/120 fps, retry and protagonist handoff, passive regeneration reaching full health during use, and station line of sight in representative geometry. Optional difficulty-specific capacity/refill increases require an explicit authored rule; the Standard baseline is implemented without inventing one.
