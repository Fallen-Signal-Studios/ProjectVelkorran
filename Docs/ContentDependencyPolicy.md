# Content dependency policy and validation semantics — 11 September 2026

Resolves the fresh-clone reproducibility question raised when the macOS suite reported five failures
that the Windows machine did not. The conclusion is not that macOS differs: **no clone of this
repository could pass those five**, and the Windows run that reported 612/612 did so against content
the repository does not contain.

Machine-readable form: `Scripts/Manifests/ContentPrerequisites.json`, enforced by
`Scripts/Check-ContentPrerequisites.py`.

## The rule

An automation test may depend only on **version-controlled** content. Tracked roots are `Source/`,
`Config/`, `Scripts/`, `Tests/`, `Content/Aurelion/`, `Content/Cues/`, and `Plugins/`. A test that
loads anything else is describing one machine, not the code.

## Validation semantics — three states, and why

| State | Meaning | Counts as |
|---|---|---|
| **Passed** | The test executed and its assertions held. | evidence |
| **Failed** | The test executed and an assertion did not hold, **or a required production dependency is absent**. | defect |
| **Unrunnable / PrerequisiteMissing** | A declared external or optional prerequisite is absent, so the code was never exercised. | **nothing** |

The third state exists because collapsing it into either of the others is how a suite comes to look
qualified when it is not. It carries no evidence, so:

- An aggregate gate may report full qualification **only** when no required prerequisite is missing
  and no test is unrunnable. `Validate-UnrealMac.py` returns exit 3 and prints `NOT QUALIFIED` when
  either holds, even though every executed test passed.
- A test declares its own unrunnability by emitting `PREREQUISITE_MISSING` in its output. The gate
  detects it from the test's own entries; it is never granted from outside, so no test can be excused
  by editing the gate.
- `Check-ContentPrerequisites.py` **refuses a manifest** that softens a `project_owned` dependency's
  `on_absence` from `fail` to `unrunnable`. Category fixes semantics; one field cannot override it.

This is the same principle the performance harness encodes with `Insufficient` and `InvalidBudget`:
absence of evidence is never equivalent to pass.

## The five failing tests, classified

| Test | Missing asset | Origin | Production depends on it | Tracking permitted | Long-term behaviour |
|---|---|---|---|---|---|
| `Campaign.Validation.GameplayCuesStillResolve` | `/Game/Cues` (11 assets) | **Project-authored** overrides of the Narrative cue originals | **Yes — required at runtime** | Yes | **Failed** until the assets are committed |
| `Campaign.PlacedNPC.AuthoredDefinitionPrecedesNativeASCStartup` | `/Game/SciFi_Drone_1/.../NPC_ReformationCombatDrone` | Project-authored, left inside a marketplace pack folder | No | Moot | Dependency **removed**; now uses tracked production data |
| `Campaign.PlacedNPC.SpawnerAndRestoreDefinitionsOwnNativeStartup` | same | same | No | Moot | same |
| `Campaign.Cinematic.RequiredCharacterUsesActualNativeStartup` | same | same | No | Moot | same |
| `Campaign.Cinematic.RequiredCharacterStillRefusesUnreadyOrRetiredState` | same | same | No | Moot | same |

### `/Game/Cues` — category 1, project-owned production asset

Examined with particular scrutiny, because the cue configuration deliberately makes this the
authoritative override root.

**Determination: these are project-owned production assets and must be version-controlled.**

Evidence:

- `Config/DefaultGame.ini` declares exactly one scan root: `+GameplayCueNotifyPaths="/Game/Cues"`.
  There is no second entry anywhere in `Config/`.
- An editor enumeration by native parent class in this clone finds **22 cue notify registry entries,
  all under `/NarrativePro/Pro/Core/Abilities/Cues`, and zero under `/Game/Cues`**. Priority 3
  recorded 22 entries (11 assets) in *each* root on the authoring machine.
- So in a clone, the only configured root contributes nothing and **no project cue notify registers
  at all**. This is a runtime defect, not merely a test gap.
- The assets are forked copies of the Narrative originals sharing their GameplayCue tags — authored
  project overrides, which is precisely why the configuration privileges them.

Licensing and policy permit tracking: the repository **already tracks 10,270 `.uasset` files** of the
customised Narrative fork under `Plugins/`, including the eleven cue originals these override.
Tracking project-authored derivatives of already-tracked content adds no new exposure.

Actions taken:

- `.gitignore` now permits the path (`!/Content/Cues/`, `!/Content/Cues/**/*.uasset`), verified with
  `git add --dry-run` on both a top-level and a nested asset while the rest of `Content/` stays
  ignored.
- The test **stays Failed**, not unrunnable. A missing required production dependency is a defect.
- The cue configuration is **unchanged**. Widening it back to `/Game` or adding the fork root would
  accommodate the absence at the cost of correctness: Priority 3 measured that adding the fork root
  registers duplicate tags and silently discards some project overrides.

**Outstanding, and the only way to close it:** commit `Content/Cues` from the authoring machine. It
cannot be done from a clone that does not have the assets.

### `NPC_ReformationCombatDrone` — project-authored, but not a production dependency

**Determination: project-authored, not marketplace content, and nothing in production references
it.** The dependency has been removed rather than tracked.

- It is an instance of `NPCDefinition`, a Narrative Pro class. An art pack cannot ship an instance of
  a class from a plugin it knows nothing about, so it was authored in a Narrative-based project and
  left in the `SciFi_Drone_1` pack's `Textures/` folder. It is not marketplace content, and it is not
  Narrative Pro content.
- Never tracked: zero add-commits anywhere in history.
- Referenced only by two test files. No tracked production asset, `Config` entry, source file or
  plugin asset references it or its pack.
- The authored campaign drones `NPC_AurelionSecurityDrone` and `NPC_AurelionContaminatedDrone` were
  loaded and inspected: both use `/NarrativePro/Pro/Core/Abilities/Configurations/AC_NPC_ReformationDrone`
  **directly**, with `GE_DroneStartupAttributes` and four `DefaultAbilities` entries.

The four tests used the asset for exactly one thing — a real shipped `AbilityConfiguration`. They now
load `/Game/Aurelion/Enemies/NPC_AurelionSecurityDrone`, which is tracked, carries that same
configuration, and is actual campaign data. This is **stronger** than before, not weaker: the tests
moved from a stray fixture onto shipped production data. Single declaration in
`Source/ProjectVelkorranTests/Private/Tests/SovTrackedContentPaths.h`.

Negative control: pointing that constant at a non-existent asset fails three tests; restoring it
passes all twelve in the suite. The tests are not vacuous.

No asset was committed to achieve this, and no third-party content was added to the repository.

## A content defect the untracked fixture was masking

Repointing the tests onto production data surfaced a pre-existing problem.
`AC_NPC_ReformationDrone` — tracked, present, and shared by **both** authored campaign drones — has
four `DefaultAbilities` entries of which **the first two are `None`**:

```
[0] None
[1] None
[2] GA_Weapon_Wield_C
[3] GA_Death_C
```

For comparison, `AC_Enforcer` has six entries, all populated. The old untracked seed must have
pointed at a configuration with no empty slots, which is why these assertions passed on the authoring
machine.

The tests now skip unset entries — asserting that a null class was granted asserts something
incoherent — but they **cannot pass vacuously**: each requires at least one real ability to have been
granted, and each emits a warning naming the configuration and the number of unset slots, so the
defect stays visible in every run.

**This is a content authoring question, not an engineering one, and it is left open:** whether the
authored SecurityDrone and ContaminatedDrone are meant to ship with two empty ability slots needs the
owner's intent. It is not a tracking problem — the asset is present and tracked.

## Absent paths that are correct as they are

A sweep of every `/Game/` and `/NarrativePro/` path referenced by any test found 23 absent paths.
Only `/Game/Cues` is a genuine missing dependency. The rest are recorded in the manifest's
`expected_absent_fixtures` so they are not "fixed" later by creating assets:

- **Negative-path fixtures** (`/Game/Tests/CloudMissingMap`, `/Game/FailedDestination`,
  `/Game/Tests/DA_MissingPauseDefinition`, and five more). These must *not* resolve; the tests assert
  how absence is handled. Creating an asset at one of these paths would make its test vacuous, so the
  checker fails if one appears.
- **String-only fixtures** (`/Game/Items/Weapons/Weapon_AurelionRifle`, the `DA_*` campaign and
  dialogue paths, `/Game/Missions/M01`, the `LS_*` level-sequence paths). Passed to pure
  classification helpers and never loaded. `Weapon_AurelionRifle` is additionally the intended
  Enforcer fix, and is expected to exist later without changing those tests.

## Current state

| | |
|---|---|
| Suite | 616 passed, 1 failed, 0 unrunnable |
| The one failure | `GameplayCuesStillResolve`, because `/Game/Cues` is absent — correct signal |
| Gate verdict | `NOT QUALIFIED`, by design, until the cue assets are committed |
| Third-party assets added | none |
| Cue configuration | unchanged |
