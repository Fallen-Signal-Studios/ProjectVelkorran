# Guard lifecycle validation

The existing `USovGuardComponent` remains the sole native owner of Guard,
Perfect Guard, counter-window, and guard-break loose-tag contributions.
`USovGameplayAbility_TarrikGuard` remains the held-input GAS entry point.

## Changes

- Direct `BeginGuard` and ability activation reject dead, fatal, interacting,
  sequencer-controlled, ragdoll, poise-broken, guard-broken, Echo-active,
  deflecting, and externally Busy states. The original minimum-start Stamina
  check remains in the component.
- Those states also end an existing Guard. Live tag counts include active
  GameplayEffects. A Blueprint child that adds Busy to its activation-owned
  tags may retain its single Busy contribution; any additional contribution
  interrupts it. Direct Blueprint calls receive no Busy exemption.
- Synchronous tag, component, Blueprint, and GAS callbacks cannot resume an
  ended activation and create a new input task or timing window. Owned flags
  are updated before GAS dispatch. Ability removal and component rebinding
  clean up the previous Guard.
- A valid counter opportunity survives normal release or an attack becoming
  Busy, so the existing `Damage.Source.GuardCounter` reward path still works.
  Incapacity and conflicting Echo/deflection states clear it. Counter reward
  consumption occurs before Echo callbacks and only consumes this component's
  owned window.
- Perfect defense still succeeds if its five-Stamina cost exhausts the
  defender. It blocks the hit and grants the normal 12 Echo reward, then breaks
  Guard without opening a counter. Damage math was not changed here.

## Automated validation

Eight tests under `ProjectVelkorran.Campaign.Guard` create transient worlds and use
the real Narrative ASC and attribute set, without loading authored assets:

1. `AdmissionAndDirectInterrupts`: all direct-entry blocking states and live
   interruption cleanup.
2. `GASCancellationAndEffectOwnership`: actual GAS activation, own versus
   external Busy, active-effect ragdoll, reactivation, and ability removal.
3. `ReentrantStartAndTagOwnership`: interruption during initial tag dispatch
   and start notification, then preservation of independent external tags.
4. `PerfectDefenseLastStamina`: real damage routing at five remaining Stamina,
   mitigation, Echo reward, break, and absence of a counter.
5. `CounterSurvivesAttackBusy`: real perfect-defense and counter damage,
   Busy transition, and one-shot reward consumption.
6. `StaminaAdmissionImpactAndBreakRecovery` (added 12 September, PC09): the
   8-Stamina start threshold on both entries, Narrative's tag-input release,
   perfect-timing expiry without ending the hold, ordinary impact cost and
   quarter mitigation, unaffordable and heavy breaks, and each break expiring
   back to a startable Guard with no residual tag or Busy.
7. `CounterWindowReleaseExpiryAndSingleOwnership` (added 12 September, PC09):
   the counter surviving release and concurrent attack Busy changes, one
   contribution and one reward after re-guarding, timer expiry, clearing by
   poise break and Deflecting, and reactivation afterwards.
8. `RebindingReleasesOwnedStateAndReactivates` (added 12 September, PC09):
   component rebinding while guarding with a counter, and while broken, leaves
   no owned tag on either ASC, cancelled timers apply nothing, and Guard
   reactivates.

Example command after building the UE 5.7 editor target:

```powershell
UnrealEditor-Cmd.exe F:\ProjectVelkorran\ProjectVelkorran.uproject -unattended -NullRHI -ExecCmds="Automation RunTests ProjectVelkorran.Campaign.Guard; Quit" -TestExit="Automation Test Queue Empty" -log
```

The first five tests were originally authored without an engine. On 12 September
all eight were compiled and executed on Mac under UE 5.7 and pass, including
timer expiry, and negative controls confirmed they fail when the guarded seams
are broken; see PC09 in
[EngineeringBacklogReconciliation-2026-09-12.md](EngineeringBacklogReconciliation-2026-09-12.md).
Blueprint compatibility, montage behavior and network prediction/replication
remain unverified: no authored asset grants or presents Guard yet.

## PIE checks

Hold Guard while applying each blocking state. Confirm its ability and animation
end, unrelated effect-granted tags remain, and Guard can start again when the
blocking state clears. Test death, ragdoll, interaction, protagonist switching,
and removing the ability while the input remains held. Verify regular release
does not erase the short counter opportunity, and authored counter attacks carry
the existing counter classification. Repeat owning-client cancellation under
latency with an authoritative server. Guard does not observe
`Narrative.State.Weapon.Equipping` (Echo, Melee and Deflection do); confirm the
authored equip flow either grants Busy or that guarding through an equip is
acceptable.
