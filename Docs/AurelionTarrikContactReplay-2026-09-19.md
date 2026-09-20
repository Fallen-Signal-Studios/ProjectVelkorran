# Tarrik contact replay — 19 September 2026

Tarrik's melee contact is still unqualified. No attack range, blade geometry,
damage, animation or C++ changed in this increment. The previous goal turn made
implementation progress by committing the rendered HUD weight refinement.

The contact probe now supports either controlled protagonist and records the
companion's actual montage position, native melee state and blade edges at a
25 ms sampling interval. Its optional passive mode supplies no player input;
the existing E4A driver owns normal controls. The checkpoint bootstrap validates
the exact requested encounter boundary and waits up to eight seconds for native
E4A activation before starting that driver. It never starts an encounter itself.

## Observations

The unmodified save banks came from the genuinely earned E4A entry in
`CampaignQualityRoute-20260919-180911-b8ff8cfb`. They were hash-checked and copied
only into isolated test profiles; originals and the creator's save were untouched.

- `TarrikCompanionContact-20260919-231201-53799fb3`: native reload succeeded and
  the focus request was admitted, but the target stayed 2456.58 cm from Tarrik.
  No attack montage, player damage or companion damage occurred. This does not
  establish a sword-contact defect.
- `TarrikE4AContact-20260919-231656-c0ecd81b`: the E4A input driver rejected its
  initial state because the encounter was not active. It supplied no combat
  qualification. The observer ended cleanly without errors.
- `TarrikE4AReadyContact-20260919-232028-e6472bb7`: native reload again succeeded.
  After the bounded wait, the encounter was FAILED, Selene stood at approximately
  (0, 18630.78, -1109.85), the capsule overlapped StartVolume, and initial-entry
  retry was enabled. LastError was “Encounter objective requires its current
  ready, living protagonist and active campaign.” The callback checks had found
  a ready, living Selene; LastError may describe an earlier transition and is not
  itself proof of the failure cause. No E4A driver or attack observation began.

This older checkpoint is unsuitable for diagnosing Tarrik's melee until its
restore failure is understood. Do not widen traces or alter damage based on it.
Next: obtain a fresh E4A entry checkpoint from current content, capture the
encounter transition and participant states, and observe actual close-range
attacks. The earlier full route and CP9 reload pass do not qualify this boundary.

Baseline: full build/automation `20260919-230556-cc9b5224`, 719 tests.
The protected creator map and grenade assets remain excluded.
Post-change full gate `20260919-232427-286930b4` passed the build invocation
without SkipBuild, all 719 matching automation tests, coverage and source
integrity. No tracked edits occurred during the gate. Syntax and whitespace
checks passed. These checks validate the regression suite, not Tarrik's hits.
