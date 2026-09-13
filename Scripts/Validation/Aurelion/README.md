These are Aurelion QA tools for UE 5.7 Editor. Publication does not establish a route pass. Keep the generated run reports, source hashes, native build/test evidence and matching asset hashes together. A process exiting zero only establishes that the process exited successfully.

Retained visible launches use the deferred `PY` console command so the editor remains available for ordinary UI input. Non-retained runs still use the process Python executor. The entry driver waits for Narrative's native loading gate before starting the wheel timer, within the unchanged entry-stage deadline. These launcher/driver changes require their own real route evidence.

`-DisableAura` is an explicit per-process isolation option for the editor assistant plugin, which can launch a second headless editor and lock build outputs. It does not edit the project descriptor; excluded plugins are recorded in `launch.json`. A run using it must retain that environment distinction in its evidence.

The September 13 E1 pilot revision retains a living visible target, uses more responsive
look input, strafes during clear in-range shots/reloads and periodically requests the
authored Evade action. These are ordinary input requests; native collision, stamina,
ability admission and damage still govern the result. Reports record the pilot source
hash. Earlier static-fire failures remain failures; the revised pilot needs its own
completed live report and does not qualify physical keyboard/mouse operation.
The pilot also queries reachable cover, rechecks exposure before waiting for native
shield recovery, reacts to actual approaching rockets and approaches existing matching
ammo pickups. Cover/pickup reports describe attempts, not successful protection or
collection. No ammo, health, projectile or encounter state is supplied by the driver.

The evidence snapshot records actual M12 completion with all 22 native receipts and normal mission travel to M13. This was earned across retained-world continuations, preserved QA failures and three explicitly documented layout iterations: the shared handoff destination, rescue-door dimensions and Heat station. Full Meeting, rescue/cage/Quarantine/Recognition scenes, E3 victory, two Weaver link severs, Tarrik handoff, Frost/Heat/Poise, Core followup and conventional E4B victory have separate actual reports. The normal M13 Cinderline wield restored its lazy ammo cache to the saved five rounds without changing inventory. The first M13 coaction then failed companion readiness before any M13 receipt. A later actual earned22 M12 checkpoint load passed its saved player/companion/storage/Heat checks and three standing non-ragdoll game seconds. A subsequently observed historical NPC roster problem remains under investigation, so whole-world recovery, a fresh complete route, M13 completion, CP9 reload and packaged execution remain unqualified. Older failed reports remain in the manifest beside later successful segments.

The manifest records the exact completed current native Editor/Game/non-unity builds, test totals, warnings and source fingerprint supplied to the generator. It also records the matching saved two-map assembly, all 18 station surfaces, six handoff destinations and label checks. These engineering and static content checks do not establish a fresh gameplay pass. The complete authoring log and handled diagnostics remain visible in package_basis.authoring_log_caveat.

The fresh route starts in `/Game/Aurelion/Maps/L_Aurelion_M12`. M13 is reached through the real mission transition. Direct M13 loading is useful for inspection or package startup checks and does not qualify campaign entry, handoff or completion.

Build the current Editor target, including `ProjectVelkorranEditor`, before running. The wheel selector requires `SovAurelionPIEInputLibrary`; navigation requires `SovAurelionNavigationProfileLibrary` and `SovAurelionNavigationLibrary`. The route also needs the full authored M12/M13 maps, the authoring modules in `Scripts/Editor` listed in the manifest, the established player kit/UI content, and the licensed Narrative/character assets already installed in the project. Python uses Unreal plus its standard library. No account token, credential, external Python package or dated machine-specific cache belongs in this directory.

Reusable modules are inert on import. The two dedicated process entry scripts, `validate_aurelion_entry_pie.py` and `probe_aurelion_roster_startup_timeline.py`, deliberately begin their editor lifecycle when executed; run them through the launcher with the required fresh profile. Do not import either entry script into an existing live editor session. Every listed Python file receives syntax and dependency checks; exact module counts are recorded in the manifest.

From the repository root, set `$EngineRoot` to the installed UE 5.7 directory and run the retained route candidate:

```powershell
.\Scripts\Validation\Aurelion\run-editor-script.ps1 `
  -EngineRoot $EngineRoot `
  -ScriptPath .\Scripts\Validation\Aurelion\validate_aurelion_entry_pie.py `
  -Label Route -Visible -KeepEntryOpen -ContinueE1 -ContinueRoute
```

The launcher creates a unique `Saved/Validation/Aurelion` run directory with an isolated `UserData` profile. `-OutputRoot`, `-ProjectPath` and an optional explicit `-PipInstallPath` are configurable. `-Visible` omits offscreen rendering; the helper process still launches hidden, so bring the editor to the foreground for rendered review. Retained sessions return after launch and leave the editor available for review; the driver deadlines still bound input. Close the editor normally when finished. An offscreen non-retained process may be stopped at its explicit launcher deadline. The launcher disables sound, so these runs never qualify audio.

The chained candidate is entry → E1 → E2 → E3 entry → E3 rescue → E4 entry → E4A → E4B → M13 entry → M13 departure. Weapon selectors hold the existing weapon-wheel action and deliver ordinary mouse axes through the editor-only input adapter. They inspect the actual highlighted sector, release the action, and require the requested weapon in the real wielded set. At Axiom, the operator must make one ordinary left mouse click in the visible wheel to choose the main hand; the selector reads the current owned main-hand slot before releasing. It never calls a widget handler or equipment setter. Other wheel selections retain their prior defaults. Combat uses existing movement, look, attack, aim, reload and ability inputs. E4B requests the existing companion HoldPosition command through its native admission owner. No driver grants resources, supplies damage, heals, teleports, retries or manufactures journal/encounter/cinematic receipts. Their preconditions deliberately reject an altered or already-progressed starting state.

Each driver qualifies its own boundary only when its report passes. Later drivers require actual native handoffs, independent Weaver link receipts, Thermal Fracture and conventional victory, mission travel, full scenes, exact evidence provenance, both characters physically riding the lift, and separate departures. Rendered presentation, physical keyboard/mouse operation, packaged route completion and explicit save/reload require separate evidence. The route chain starts each successor only after the actual predecessor passes and retires its input callback. To stop a retained chain, execute `continue_aurelion_route_input.stop()` in that same editor Python session; it stops only its owned active child. Each standalone driver also exposes `stop()`. No second driver should run concurrently.

After that complete route actually passes, the reviewed `probe_m13_native_checkpoint_reload.py` can observe the earned final checkpoint. It is an explicit post-route operation, outside the automatic input chain. Run it in the **same retained Editor Python session**, after all input observers have retired; do not launch it in a new Editor process or load M13 directly. With these adjacent modules already on that session's Python path:

```python
import os
from pathlib import Path
from uuid import uuid4
import probe_m13_native_checkpoint_reload as cp9

cp9_output = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / ('CP9Reload-' + uuid4().hex[:8])
cp9_run = cp9.start(cp9_output)
```

Import is inert. `start()` requires the actual passed `continue_aurelion_m13_input._RUN`, its untouched report, all 35 earned receipts, the original evidence provenance, the completed paired lift and the exact successful CP9 slot-0 generation. It checks that completed observers hold no old-world references, retains the GameInstance/save owner/delegate wrapper, and makes exactly one public `LoadSlot(CHECKPOINT, 0)` request on a later tick. It supplies no input, resource, damage, transform, checkpoint capture or story-state writes. Request acceptance alone is not a pass.

`cp9_run.done` and `cp9_run.report['qualified']` expose completion. Qualification requires a different native world/controller/GameMode, a fresh native load token, one successful completion callback for the exact header, and four seconds of ready living protagonists at their separate exits with unchanged raw journal/evidence, stable identities, inventory/resources and completed lift. The observer allows 180 seconds overall. Its public-resource comparison deliberately refuses to guess private checkpoint values; natural regeneration since the completed route can conservatively fail this check. `cp9.stop()` retires only this observer and records an unqualified result. It does not cancel the native load already requested. No CP9 execution or reload success is claimed by this publication candidate.

If Unreal terminates before the observer can finish, the host process monitor may call the inert standard-library helper `finalize_m13_checkpoint_interruption.record_interruption`. Use only the actual observation directory and process result, from a host Python session with the candidate directory on its import path:

```python
from finalize_m13_checkpoint_interruption import record_interruption

record_interruption(observed_directory, reason=observed_reason,
                    process_id=observed_unreal_pid,
                    process_exit_code=observed_exit_code)
```

The variables above must come from the real process observation. The helper creates a new `checkpoint-reload-termination.json` containing the hash of the untouched last observer report. It refuses to overwrite a termination record or relabel a completed pass, does not control any process, and cannot supply a missing native completion callback. Preserve both files. It imports no Unreal module and makes no gameplay request.

For fresh startup retention, launch `probe_aurelion_roster_startup_timeline.py` in its own isolated process. It observes 24 enemies and 10 story NPCs against all 19 authored definitions through the requested first 30 game seconds, retains actual sample times, and checks stable identity/placement and native readiness. It acknowledges the unchanged isolated accessibility profile and starts/stops real PIE; it does not supply gameplay inputs or force initialization. `inspect_aurelion_enemies_pie.inspect_enemies(world, output_path, require_initialized=True)` is its read-only helper. The older enemy-only probe and date-suffixed identical roster copy are intentionally omitted.

`probe_aurelion_navigation_readonly.run(output_directory)` inspects the current reopened editor or PIE world without loading/saving or starting PIE. It checks the persisted native navigation profile and a bounded entry path. That is not a complete route/navigation certification. Keep it adjacent to the entry driver so the import resolves without a workspace subdirectory.

For selected-map Development packaging, use the existing cook script from the repository root:

```powershell
.\Scripts\Cook-CombatPlaytest.ps1 `
  -EngineRoot $EngineRoot `
  -ArchiveDirectory (Join-Path (Get-Location) 'Saved\AurelionPlaytest') `
  -MapPackages @('/Game/Aurelion/Maps/L_Aurelion_M12', '/Game/Aurelion/Maps/L_Aurelion_M13') `
  -ExcludeMetaHumanAuthoringData
```

This preserves the script's explicit `/HairStrands/Emitters/StableRodsSystem` inclusion and its two existing editor-data exclusions (`/MetaHumanCharacter/BuildPipeline`, `/MetaHumanCoreTech/RealtimeMono`). Keep runtime groom, mesh, material, RigLogic and normal dependency traversal. `-SkipBuild` is appropriate only when matching Editor/Game binaries have already been validated against the frozen source tree. The launcher in `Scripts/Play-Aurelion.ps1` must receive the extracted archive directory and starts M12 explicitly; the project's unrelated default main menu is not this route's entry point.

After the final content save, verify both map packages and their authored mission/GameMode, scene, enemy, companion, player-kit and UI dependencies in the actual staged container/UFS inventory using installed UnrealPak tools. Record the archive, executable, source and asset hashes, both direct-map startup logs, all warnings/errors and separate route evidence. Selected-map cooking still traverses effective AssetManager roots; it does not certify the full shipping campaign or remove legacy systems by implication. This candidate has not launched, cooked, changed config, or saved assets.

The carrier clearance probe verifies the actual corridor with native navigation and capsule queries before and after Meeting. The owned HUD observer checks the current player, weapons, ammo text and native Shield at Entry and E2 boundaries, with a fixed one-second settling allowance. These read-only observers return primitive reports and do not retain the old world. Their inclusion and host checks do not substitute for the corresponding successful live reports.


The composed candidate includes six pinned QA amendments: legitimate None trace handling; bounded normal approach changes when an NPC wins focus; the Axiom main-hand click wait; feasible Heat-range approach, north Frost approach and native Heat focus admission; and the floor-derived west route around the survivor crowd. The M13 entry helper also waits until destination readiness before checking retained travel recovery, uses the native new-mission proxy identity contract and compares public stored magazine/source fields separately from runtime ammo caches. Native interaction range, focus, hold time, thermal window, proof and receipt gates remain authoritative. The same helper schema is used consistently by future M13 and CP9 reports; do not hot-reload it into a session with older report schemas.
