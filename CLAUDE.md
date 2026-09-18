# Project Velkorran

Unreal Engine 5.7 single-player campaign. GAS for abilities, Enhanced Input, UMG, Mass, and UE
GameplayCameras. Built on a **customised** Narrative Pro plugin under `Plugins/Narrativeed3f9374a6eV6/`
— the customisations are ours and the plugin is tracked in git, so treat it as first-party source.

## Layout

| Module | Purpose |
|---|---|
| `Source/ProjectVelkorran` | Runtime. Ships. |
| `Source/ProjectVelkorranEditor` | Editor-only tooling. |
| `Source/ProjectVelkorranTests` | Automation tests. **Editor targets only** — it throws a `BuildException` otherwise. |
| `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal` etc. | Customised plugin runtime. |

`Docs/` holds design contracts, audits and handoffs; `Docs/AdversarialAudit-2026-09-17/` is the current
audit and its item IDs (`PC2-08`, `AR2-17`, …) are the working vocabulary. `Scripts/` holds validation
and editor automation.

## Validating

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -DisableAura
```

- `-TestFilter` must match `^(?:ProjectVelkorran|NarrativeArsenal)(\.[A-Za-z0-9_]+)*$`. Use it to
  iterate on one area.
- `-DisableAura` always. Aura is off deliberately: its indexing crashed the editor and it force-enables
  remote Python.
- **`-SkipBuild` does not recompile untouched files.** It is for fast iteration only. An include-order
  break sat on the branch green for an hour because every run used it. **Run a full build before you
  commit.**
- Reports land in `Saved/Validation/<timestamp>/`. `Build.stdout.log` and `Automation.stdout.log` are
  where the real errors are; the console summary is not enough.

Editor automation, for anything needing a loaded project:

```powershell
.\Scripts\Validation\Aurelion\run-editor-script.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -ScriptPath 'Scripts\Validation\Aurelion\<script>.py'
```

Output goes to `Saved/Validation/Aurelion/<label>-<timestamp>/Editor.log`. Scripts that only inspect
are named `*_readonly.py` and must not save assets.

## Hard rules

**Never open a script-authored asset in its editor.** `BT_AurelionSecurityCrossfire` is authored by
script and exists only at runtime; opening and saving it strips every task, decorator and EQS
reference and the enemies stop attacking. This has already happened once. The same applies to
anything a `Scripts/Editor/*.py` writes.

**Load an asset to inspect it. Never conclude from scanning its packed bytes.** Two confidently wrong
claims have come from grepping strings out of `.uasset` files — a montage reported missing that was
present, and a "confirmed defect" that did not exist. If it matters, load it in the editor and read
the object.

**Do not commit the creator's content edits.** `*.uasset` and `*.umap` are gitignored by default, and
anything tracked needs an explicit allowlist entry (see `Content/Input`, `Content/Aurelion`). The
repository tracks source plus `Content/Aurelion` only; marketplace content stays untracked. Leave
modified assets in the working tree alone unless asked, and never commit the `ProjectVelkorran.uproject`
Aura change.

**Never push.** The creator handles pushes. Commit freely; stop there.

**Close editors gracefully** — `taskkill` without `/F`.

**Never change Windows system or security settings** (page file, remote Python execution).

## Writing code here

Comments explain *why*, in prose, and are worth the line. The house voice states the problem the code
exists to solve — often citing an audit ID — not what the statement below does. Match the density and
idiom of the file you are in.

Pure logic goes in a `Sov*Policy` namespace (`SovResonancePolicy`, `SovCameraPolicy`,
`SovCompanionApproachPolicy`) so it is testable without a world. Prefer that over a component method
whenever the rule does not need actor state.

## Writing tests

`IMPLEMENT_SIMPLE_AUTOMATION_TEST(FName, "ProjectVelkorran.<Area>.<Group>.<Behaviour>", EditorContext | ProductFilter)`.
Assertion messages are sentences describing the behaviour, not labels: *"A field the claim did not name
keeps the protagonist's own framing"*, not *"style check"*.

**Prove the test fails without the fix.** Disable the change, rebuild, confirm the failure, restore.
A test written after the fact usually passes for the wrong reason. Check that each assertion actually
discriminates — a fixture staged in the branch the code avoids anyway will pass either way.

Test-world gotchas that have each cost an hour:

- `World->Tick(LEVELTICK_TimeOnly, big)` is clamped by WorldSettings. Step in 0.05 s frames.
- Timers need `TGuardValue<uint64> Frame(GFrameCounter, ++FixtureFrame)` and a `GetTimerManager().Tick(0.f)`
  at world construction.
- `CreateWidget` needs a local player controller; use `NewObject` instead.
- A `UCLASS` cannot be declared in a `.cpp`. Test fixtures live in `Tests/*TestFixtures.h`.
- The module uses unity builds. A helper struct at file scope in one test `.cpp` can collide with
  another; put shared access structs in the file that already declares them, or namespace them.
- Reach private state through the `friend struct FSov*TestAccess` the subsystem already declares.

## Committing

Subject is a short declarative sentence saying what changed for the player or the code, lowercase after
the first word, no type prefix: *"Stop a downgrade from eating the save it cannot read"*. The body
explains the defect and why this is the fix, and names the audit ID. Attribution line:

```
Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

## Known capability limits

Blueprint **graph** authoring from Python is not safe and should not be attempted; Unreal also does not
expose Blueprint interface function signatures to Python, so those must be read in the editor. Content
work of that kind belongs to a human or an in-engine agent — see the handoff documents in `Docs/`.
