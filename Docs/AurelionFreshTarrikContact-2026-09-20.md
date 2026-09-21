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
