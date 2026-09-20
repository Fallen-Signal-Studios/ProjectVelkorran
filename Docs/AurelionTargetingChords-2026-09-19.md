# Controller targeting chord implementation

The engine handoff's LB layer is authored in IMC_Combat:

- LB + Y: Threat Focus.
- LB + X: Designate.
- LB + D-pad Left/Right: cycle target.
- Ability3, Interact, Reload, OpenInventory and QuickUseItems mappings on those
  shared buttons have blockers keyed to IA_WeaponWheel.

The context now has 60 mappings, previously 56. Keyboard mappings and the
unbound gamepad cinematic skip action are unchanged. All nine authored triggers
reference IA_WeaponWheel. Chorded rows precede their base rows.

## Implementation evidence

Fresh Blueprint exports in `TargetingChordInspection-20260919-212118-ad6fe4b7`
show wheel selection driven by right-stick values from BP_SovPlayerController,
with no face-button or D-pad wheel-selection binding found in the exported
wheel inheritance chain. This static review does not prove CommonUI hardware
routing or simultaneous gameplay behavior.

The old authoring script could not set EditInstanceOnly ChordAction on a trigger
already owned by an asset. A transient instance can be configured, then renamed
into the context. `TargetingChordInspection-20260919-212330-04adb6e8` verified
that construction path. The existing script also created null-source blockers;
these now explicitly reference the wheel action. Unreal normally generates
blockers against the chorded action at mapping rebuild. The explicit blockers
here implement the handoff's stronger requirement that base actions remain
unavailable throughout the LB hold, even before a target chord fires.

`TargetingChordAuthoring-20260919-212458-cd59522e` backed up the original input
asset, saved only IMC_Combat and read back four chords/five blockers. No native
source or mission map was edited. Fresh runtime and full validation results
follow below. Actual physical gamepad dispatch, selection, suppression and
release behavior remain unqualified until exercised end to end.

## Fresh runtime result

`TargetingChordRuntime-20260919-213245-4a0cdd5e` passed controlled chord semantics.
The fresh asset loaded all nine context-owned triggers with the exact mapped
wheel action (`/NarrativePro/Pro/Core/Data/Input/IA_WeaponWheel`). The fixture
injected targeting actions with their authored mapping triggers plus the wheel
action through the actual Enhanced Input subsystem. With wheel released there
were no targeting presses. With wheel held, all four native semantic press
events arrived. Releasing the wheel emitted all four release events. The
controller's actual WeaponWheelHeld followed false/true/false, and neither look
nor move input was ignored in any sample.

Limitations remain explicit: action injection bypasses hardware key routing and
does not prove the base-action blockers, CommonUI button handling, real target
acquisition or accidental weapon selection. Those need an end-to-end gamepad
pass. Earlier attempts are retained: a wrong same-named wheel action caused a
startup identity assertion; a post-tick direct UpdateState probe returned None
while held and did not establish gameplay dispatch. The successful test observes
actual semantic events rather than inferring them from that post-tick probe.

Full post-change validation `20260919-213502-30a3c905` passed build invocation
without SkipBuild, all 719 matching automation tests, report coverage and source
integrity. No tracked edits occurred during the gate. Protected M12 hash remained
B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5.
