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

Five tests under `ProjectVelkorran.Campaign.Guard` create transient worlds and use
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

Example command after building the UE 5.7 editor target:

```powershell
UnrealEditor-Cmd.exe F:\ProjectVelkorran\ProjectVelkorran.uproject -unattended -NullRHI -ExecCmds="Automation RunTests ProjectVelkorran.Campaign.Guard; Quit" -TestExit="Automation Test Queue Empty" -log
```

The tests were authored and reviewed but not executed in the audit environment:
Unreal Engine, UnrealBuildTool, UnrealHeaderTool, and the editor were absent.
Engine compilation, Blueprint compatibility, timer expiry, montage behavior,
and network prediction/replication still require the UE environment.

## PIE checks

Hold Guard while applying each blocking state. Confirm its ability and animation
end, unrelated effect-granted tags remain, and Guard can start again when the
blocking state clears. Test death, ragdoll, interaction, protagonist switching,
and removing the ability while the input remains held. Verify regular release
does not erase the short counter opportunity, and authored counter attacks carry
the existing counter classification. Repeat owning-client cancellation under
latency with an authoritative server.
