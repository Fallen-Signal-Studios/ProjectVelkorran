# Editor-only native tests and packaged-game evidence

Slice 3 follows save/travel ownership and combat continuation repairs. It closes the source/module inclusion issue in B5 of `AdversarialAudit-2026-09-05/BuildAndShippingAudit.md`; it does not replace UHT, UBT, cooking, runtime tests or console qualification.

## Architecture

All native automation registrations and reflected fixtures formerly in game `Private/Tests`, NarrativeArsenal `Private/Tests`, and the plugin's outlying `NarrativeGameStateTest.cpp` now live in `Source/ProjectVelkorranTests/Private/Tests`. The fixtures keep their names, real runtime inheritance and existing friend seams. Production implementations stay in their existing runtime modules. The faction test now spawns its GameState in a real transient world instead of using unsupported `NewObject<AActor>` construction.

`ProjectVelkorranTests` is an explicit Editor module, loaded at PostEngineInit, included only by the Editor target, with an additional build-rules rejection for non-Editor targets. This excludes its generated UClass registrations and constructors from **Development Game as well as Shipping Game**. Test translation units are non-unity to avoid test-local helper collisions; production module unity policy is unchanged.

The module declares its direct UI, cinematic, GAS, physics, Mass and Narrative dependencies. Three existing production helper contracts moved to Public (`Platform/SovPlatformServicesAdapter.h`, `Targeting/SovAimAssistPolicy.h`, `UI/Dialogue/SovDialoguePresentationState.h`) instead of giving tests access to the entire game Private directory. Cross-module data/interface/factory declarations carry the runtime API export. No helper implementation was duplicated.

Fixture script paths now belong to `/Script/ProjectVelkorranTests`. These are transient test types, not production asset types, so no runtime redirects were added. The lifecycle test now derives its GameMode path from `StaticClass()` instead of hardcoding the old fixture package. Do not author production assets against fixture classes.

## Checks available without UE

```sh
python -m unittest discover -s Scripts/Tests -p 'Test*.py'
python Scripts/Test-NativePolicies.py
python Scripts/Check-TestModuleIsolation.py
```

The new source check requires the Editor-only descriptor and target boundary, finds reflected fixture headers and native registrations, rejects any remaining runtime test placement or reverse runtime dependency, and requires nonempty coverage. Existing generated-include/unity checks follow the moved test files. `Check-UnrealReport.py` already scans Source and Plugins recursively: missing registration coverage after the move fails, rather than quietly accepting a lower hardcoded count.

## Required engine matrix

| Target | Required evidence |
| --- | --- |
| UE 5.7 Editor Development, Mac and modular Win64 | UHT/UBT, all source-discovered native tests, no-PCH/non-unity production qualification and a normal unity build |
| UE 5.7 Game Development | Game UHT manifest, executable receipt, fresh UAT cook/stage manifest, packaged launch and M01→M02/recovery route |
| UE 5.7 Game Shipping | Separate Game UHT manifest/receipt/stage outputs; no fixture module/reflection or fixture-only content; launch and feature smoke route |
| Xbox Series / PlayStation 5 family | Licensed UE/platform extensions and SDKs; repeat Game build, cook, launch, suspend/account/controller/storage cases on hardware |

Use the existing `Scripts/Validate-Unreal.ps1` on the Win64 engine host for Editor build/automation; its source inventory remains authoritative. On Mac, build `ProjectVelkorranEditor Mac Development` using the installation's `Engine/Build/BatchFiles/Mac/Build.sh`, then run the same ProjectVelkorran automation filter and validate its fresh JSON report with `Check-UnrealReport.py`. The test module is selected through the descriptor/Editor target, not a special automation console command.

Run the preserved `NarrativeArsenal` filter separately too; the runner now permits that explicit safe prefix. Pass `--filter NarrativeArsenal` to `Check-UnrealReport.py` for its report. The source inventory contains 352 ProjectVelkorran registrations plus the existing Narrative faction test, 353 in total. All 351 ProjectVelkorran names present immediately before relocation survived exactly; the additional native DroneNPC death-suppression regression brings that group to 352. There are 158 reflected fixture classes, all in the test module. No native success is inferred from these counts.

Build/cook/stage the Game target separately using the existing project/engine UAT workflow. For each actual platform/configuration, check fresh metadata:

```sh
python Scripts/Check-TestModuleIsolation.py \
  --receipt /absolute/path/to/ProjectVelkorran.target \
  --uht-manifest /absolute/path/to/ProjectVelkorran.uhtmanifest \
  --stage-manifest /absolute/path/to/Manifest_UFSFiles_Win64.txt \
  --platform Win64 --configuration Shipping
```

Supply the actual output paths from that build (not a copied Editor receipt). The gate rejects incomplete evidence, wrong target/platform/configuration, missing executable or reflection modules, test modules/headers in UHT, and fixture module/class references in staging metadata. It reports artifact hashes and explicitly says runtime validation was not performed. When using non-UFS staging manifests, combine the actual UAT lists if needed so the selected list covers the executable and/or cooked containers. Do not invent manifests or treat host fixture-generated unit-test metadata as engine evidence.

Metadata inspection cannot prove the absence of a symbol in a monolithic binary or a hidden asset reference inside a container. Inspect actual loaded modules/UClass inventory and cook dependency manifests on the packaged build; audit any fixture names or `/Script/ProjectVelkorranTests` reference. Editor binary/module exports must also be qualified on Win64, where a successful monolithic build would not reveal missing DLL exports.

## Definition of done and limitations

Source completion means all existing registrations survive the move, runtime test directories are empty, source/host tests pass, and the packaged evidence checker rejects its negative cases. Engine completion additionally requires every row above and real native test success. This workspace has no UE installation or licensed console SDK, so those results are **pending**, not inferred from host checks. The move does not certify input, RHI, console save services, performance, authored content or plugin binary compatibility.
