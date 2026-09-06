# UE 5.7 campaign qualification runner

`Scripts/Validate-Campaign.py` prepares a retained evidence run on a real Mac or Windows development machine. It builds the Editor and Development game targets, runs the complete native `ProjectVelkorran` automation inventory, validates the explicitly supplied authored mission set with the shipping campaign commandlet, and optionally cooks and packages explicitly supplied maps. Python 3.9 or later is required; the runner uses only the standard library.

The implementation and portable control-flow tests do not qualify the current game. No Unreal, UHT, UBT, AutomationTool, platform SDK or game content was available in the implementation workspace. The tests inject process doubles and write deliberately invalid fixture assets inside temporary directories. Their summaries explicitly label the engine unexecuted and leave every engine acceptance field false.

## Run on the content workstation

Restore the actual working project's `Content` and Narrative content, install the matching UE 5.7 plugins, and use the development platform's supported compiler/SDK. The preflight checks installation version, executable paths, host platform, enabled plugin descriptors and presence of project/Narrative assets. UBT remains responsible for verifying actual toolchain and plugin compatibility. The runner never installs or disables project dependencies.

Begin with compilation if content has not yet been restored:

```powershell
python Scripts/Validate-Campaign.py --engine-root "C:\Program Files\Epic Games\UE_5.7" --platform Win64 --build-only --non-unity
```

```bash
python3 Scripts/Validate-Campaign.py --engine-root "/Users/Shared/Epic Games/UE_5.7" --platform Mac --build-only --non-unity
```

For the full run, create a JSON file containing an array of the real mission assets' object paths, in the form `/Game/Directory/AssetName.AssetName`. Copy the paths from the authored assets. The runner supplies no sample missions, replacement assets, generated maps or qualifying defaults. The commandlet currently requires `M01_Mantle` and `M02_OneDegree`, every allowed successor in the manifest, and the actual M12/M13 mission definitions when validating that route. A list containing only M12 and M13 cannot satisfy the current opening campaign manifest contract.

```powershell
python Scripts/Validate-Campaign.py --engine-root "C:\Program Files\Epic Games\UE_5.7" --platform Win64 --mission-manifest "C:\VelkorranValidation\campaign-missions.json" --non-unity
```

```bash
python3 Scripts/Validate-Campaign.py --engine-root "/Users/Shared/Epic Games/UE_5.7" --platform Mac --mission-manifest "$HOME/VelkorranValidation/campaign-missions.json" --non-unity
```

Append `--additional-asset` for each full object path that is loaded dynamically and therefore absent from the mission dependency graph. Append `--package` and one `--map` per real map package path to cook and package. Map paths use `/Game/Directory/MapName`, without the object suffix. Include all route maps and every other map needed to enter and traverse the intended build. The runner does not infer or certify the relationship between the supplied map list and the mission assets.

`--dry-run` emits the stage plan and runs filesystem preflight without starting engine processes. Missing engine/content still fails preflight. `--build-only` runs just the two build targets. Neither mode can produce native-test, campaign or package acceptance. There is deliberately no skip-build mode that can qualify stale binaries. `--timeout` sets a bounded timeout for each process, defaulting to 3,600 seconds. `--output` selects an evidence parent directory; the default is `Saved/CampaignQualification`. Always use an output directory outside source, configuration and content inputs.

Paths with spaces are passed as single arguments. Windows batch paths containing shell expansion characters such as `%`, `!`, `&`, or embedded quotes are rejected explicitly. Mac uses `Engine/Build/BatchFiles/Mac/Build.sh`, `Engine/Build/BatchFiles/RunUAT.sh`, and the executable inside `UnrealEditor.app`; Windows uses `Build.bat`, `RunUAT.bat`, and `UnrealEditor-Cmd.exe`. These paths must exist in the selected installation.

## Retained evidence and failure behavior

Each invocation creates a unique UTC/UUID directory. A prior successful automation report cannot satisfy a new run. `summary.json` retains planned arguments, per-stage status, native exit code, timeout state, combined output log path, automation coverage and report hash. A failed stage stops subsequent stages, retaining completed and unstarted stages separately.

| Gate | Required evidence |
| --- | --- |
| Editor and Development game build | Both UBT process exits succeed; source/content/config inputs remain unchanged |
| Native automation | Fresh JSON report; every currently registered native test is represented and successful; no failed, unrun or incomplete tests; no hidden error events |
| Campaign preflight | Successful commandlet exit and its completion marker reporting the exact supplied mission count with zero errors; `-ShippingValidation` is always supplied |
| Optional package production | Successful UAT build/cook/stage/package/archive; fresh archive contains a nonempty regular game executable and `.pak`, or paired nonempty `.utoc`/`.ucas` containers; archive inventory is retained |

The full native test inventory and build-input hashes are captured before and after execution using the existing `Capture-SourceManifest.py`. Project content/config, the selected mission manifest, UE `Build.version`, and the explicitly enabled plugins' descriptors/content/config/source are separately hashed. Required enabled plugin dependencies are followed. Hashes stream file content, so large assets do not need to fit in memory. Changes invalidate the combined result, including a run whose processes all exited successfully.

This is scoped input evidence: it does not hash the entire Unreal installation, SDK, default-enabled engine plugins or their content. Linked/reparse source inputs fail closed. The fresh run and archive roots cannot be links or reparse points. Archived Mac framework symlinks are recorded by target string and must resolve within the physical archive; directory links cannot substitute for executable or container files. Every IoStore table requires a nonempty matching data file even when a `.pak` is also present. `archive-inventory.json` contains hashes for regular packaged files. Artifact names containing `ProjectVelkorranTests` are rejected, but absence from filenames does not prove reflected fixture types or forbidden assets are absent from cooked containers. Inspect the staged executable and containers before claiming that exclusion gate passed.

## Gates that remain separate

A successful runner invocation can establish that the requested build, native automation, commandlet and optional package-production stages passed for unchanged captured inputs on that machine. It always leaves `shipping_candidate_qualified` and `packaged_playthrough_verified` false.

Native automation runs with NullRHI. This does not verify rendering, real frame times, controller focus, spoken/captioned timing, streaming behavior, or the complete M12–M13 playthrough. The commandlet validates its existing native asset/dependency rules; it does not compile every Blueprint or validate placed actors, World Partition coverage, animation integration, complete translations or runtime ability cleanup. Packaging creates a Development artifact, not a Shipping or console-certified release.

Next, retain the actual runner evidence, inspect staged module/container contents, and execute the real M12–M13 route with saves, reloads, travel failure/recovery, protagonist handoff, combat interruption, dialogue pause, objective choice and accessibility cases. Do not raise runtime or campaign-completion claims from this tooling alone.

## Portable verification

```bash
python3 -m unittest discover -s Scripts/Tests -p TestCampaignQualification.py -v
```

The dedicated tests cover command argument formation, paths with spaces, required manifests/maps, platform/plugin/content preflight, dry-run and build-only behavior, partial stages, timeouts, missing/stale/incomplete reports, absent commandlet completion, source/content drift, missing package output and archive link/module checks. These are runner tests; they never execute Unreal and never count as native game test passes.

Epic's [automation command-line documentation](https://dev.epicgames.com/documentation/unreal-engine/run-automation-tests-in-unreal-engine?lang=en-US) describes test selection and JSON report export. Its [build operations documentation](https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine?lang=en-US) describes `BuildCookRun`, platform UAT entry points and using a Project Launcher profile to verify project-specific packaging arguments. The local installation and resulting retained logs remain the authority for whether the selected commands succeed.
