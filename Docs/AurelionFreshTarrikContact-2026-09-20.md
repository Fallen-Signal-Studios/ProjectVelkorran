# Fresh Tarrik commanded-contact investigation

The new `check_fresh_tarrik_focus_contact.py` earns quarantine entry with the
existing normal-input campaign drivers, retires their input callbacks, then calls
the public companion focus command against a living, non-Elite hostile. A passive
observer records attack candidates, weapon animation and native damage receipts.
It does not restore old checkpoint banks, grant abilities, move actors or supply damage.
This is a diagnostic, not a campaign or companion-combat acceptance test.

## Results

Evidence is under `Saved/Validation/Aurelion`:

- `FreshTarrikFocus-20260920-193718-dacfc219`: E1 passed, but Selene died during
  E2 with four of six enemies defeated. The wrapper recorded failure and ended PIE.
  It never reached the companion test. Do not count this as a pass or evidence of
  a companion defect. Editor PID 52552 terminated normally according to its exit log.
- `FreshTarrikFocusRepeat-20260920-194514-aa73de7b`: E1 passed with one native
  fatal recovery, followed by E2, meeting, rescue and quarantine entry. The driver
  callbacks retired before the contact observer started. The initial Linkbound had
  53.2 health and no invulnerability tag. Editor PID 41476 subsequently terminated
  normally; the contact observer and other observers stopped, with no contact errors.

The repeat recorded 1,707 contact samples over approximately 100 seconds:

| Observation | Result |
|---|---|
| Native Tarrik melee grants | Light and heavy present |
| Candidate checks near target | In range, line of sight, activatable and available |
| Opening allowance | 8 seconds configured; last sampled active at 7.219 seconds |
| First sample within 180 cm | 14.375 seconds |
| Minimum sampled distance | 68.35 cm |
| Invulnerable target samples | 0 |
| Attack montage samples / damage receipts | 0 / 0 |
| Target health | 53.2 throughout |

This distinguishes the fresh proxy from the earlier restored-kit bug: this Tarrik
does have the native melee grants. It also removes invulnerability as the explanation
for this particular no-damage observation. It does **not** prove a collision failure:
no swing occurred.

`SovCompanionCommands.cpp` gates contribution after the opening allowance using
player/companion contribution. The observer deliberately supplied no player combat
input, and Tarrik reached melee range after that allowance. These observations are
consistent with contribution gating, but the exact remaining budget and accepted
command-goal identity were not captured. The Python call returned an empty reason
string; it is recorded as such, not represented as an independently verified Boolean
admission result. Controller focus alone does not prove command ownership.

The next useful test should provide an ordinary player hit and verify its native
receipt before evaluating Tarrik's response against the same live unprotected target.
Do not increase melee reach, remove the contribution cap, or claim damage is fixed
from this no-swing result. No gameplay tuning or native code was changed here.

Both map hashes remain unchanged. The older restored companion grant repair and
hit-reaction integration still await their separate native scope approvals. This
investigation neither repairs them nor completes the broader Aurelion goal.

Validation: baseline full gate `20260920-193027-01279e57`; final full gate
`20260920-195858-0c175ad0` passed build without SkipBuild, 722 matching tests,
coverage and unchanged native-source integrity. These tests do not turn the
no-damage contact observation into a gameplay pass.

## Fresh native melee contact confirmed

`FreshTarrikPlayerHit-20260920-200446-d5355980` subsequently earned E1, E2,
meeting, rescue and quarantine entry in a fresh session. The new player-contribution
variant selected Staccato through ordinary wheel input and attempted to arrange
one player shot at the durable Elite while Tarrik focused the separate Linkbound.
No weapon damage, health, range, grants, transforms or contribution values changed.

**Tarrik landed a native melee hit before the player fired.** At 7.094 seconds the
source callback recorded transaction `33A469E24B34643BC5FF35AB53E670C4`, attack
`512612ED47245D9F485338AFCE2CD3F0`, from `BP_AurelionTarrikCompanion_C_0` to
`BP_AurelionLinkbound_C_13`: 43.478264 applied health damage, zero shield damage,
Edge channel accepted, no guard/deflection, nonfatal. The target's before-damage
tags contained no invulnerability tag. Its initial health was 53.2.

The observer recorded `AM_Sword_3P_1H_Attack_1_Tarrik` starting by 3.094 seconds
at 158.92 cm. This run reached a melee opportunity during the opening allowance,
unlike the preceding command-only run. Together, the montage samples and native
receipt confirm that the fresh proxy can wield its sword, animate an attack and
apply real health damage to an unprotected enemy. This closes that specific
evidence gap; it does not qualify all follow behavior or older restored kits.

The **player-contribution scenario itself remains failed**: the existing observer
stops on the first companion receipt, so it stopped before the intended player hit.
The wrapper correctly rejected the missing nonlethal player receipt rather than
counting this as an after-player-hit response. The fresh damage finding above is
independent evidence within that failed scenario, not a relabeling of its status.
Keep this distinction if reusing `check_fresh_tarrik_player_contribution.py`:
it can terminate early when Tarrik contributes first. A future after-player-hit
qualification must observe past an earlier companion hit.

Structured player/companion receipt fields were added to the shared observer;
it now supports a separate ordinary player aim target so the firearm need not kill
the companion's intended target. Default probes retain the same target for both.
The editor exited normally (PID 56216), observers retired and map hashes remained
unchanged. This run supplies no fresh screenshot-based animation-quality verdict.

Post-observation gate `Saved/Validation/20260920-201613-9ad58d32` passed build
without SkipBuild, all 722 matching tests, coverage and native-source integrity.
The three diagnostic scripts also passed syntax parsing. No gameplay assets changed.
