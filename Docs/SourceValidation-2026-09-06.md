# Reproducible source validation and Editor test isolation

The August TDD alignment review found that the retained Unreal run predated current main, and reflected test fixtures were compiled into runtime modules. This change separates those fixtures and records the inputs consumed by each future validation run.

## What changed

- `ProjectVelkorranTests` is an Editor module loaded at `PostEngineInit`. The project and Narrative Arsenal test fixtures and registrations live together there. Game targets and runtime modules have no dependency on it. Its translation units compile without unity to expose missing includes. The moved lifecycle test GameMode uses its new reflected script path.
- `Capture-SourceManifest.py` records Git revision, hashes of source/config/build scripts, and the complete selected native registration inventory. The fingerprint includes untracked source files, so a clean Git SHA cannot conceal local code changes. Generated build output is excluded.
- `Validate-Unreal.ps1` captures that manifest before building, verifies it after the run, and retains the engine's `Build.version` in `summary.json`. A changed input or incomplete automation report fails validation. `-BuildGame` adds a Development game build; `-NonUnity` disables unity for the requested builds. `-SkipBuild` explicitly leaves binary freshness unverified.
- The campaign commandlet now dispatches generic status structural validation and checks shipping string-table references for status names, objective failure rules and choice reconciliation notes. Its final message identifies the gates it does not cover.

The input fingerprint is a reproducibility check, not a signature, engine-build identity, content inventory or proof of packaging. Test fixtures changing module paths may invalidate old fixture-only serialized data. Shipping campaign content must never reference them; the package gate must confirm that.

## Available host checks

```sh
python3 -m unittest discover -s Scripts/Tests -p 'Test*.py' -v
python3 Scripts/Test-NativePolicies.py
python3 Scripts/Capture-SourceManifest.py --output Saved/Validation/source.json
python3 Scripts/Capture-SourceManifest.py --verify Saved/Validation/source.json
git diff --check
```

These checks exercise report rejection, source drift detection, module descriptors, reflected-header layout and production-used portable C++ policies. They do not parse Unreal reflection, link game modules or execute gameplay.

## Required engine validation before merge

From the complete licensed UE 5.7 working project, with its project/Narrative assets and required plugins restored:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -BuildGame -NonUnity
```

Retain the complete output directory. Review UHT, Editor and Development game build logs, engine version, before/after source manifest and every registered automation result. The current CaaS workspace has neither Unreal nor PowerShell; this command and the new module boundary have not been executed here. The prior Mac result of 116 passes and 121 failures belongs to an older source revision and is not replaced by host-policy passes.

Then run the campaign commandlet with the complete shipping mission manifest and `-ShippingValidation`, including dynamic-only dependencies through `-AdditionalAssets`. Supply the authored assets; an empty fixture or reduced manifest cannot qualify the campaign.

Finally cook, stage, package and boot a representative Development campaign build. Confirm the test module/classes are absent; exercise actual accepted-then-failed travel, origin recovery and explicit retry; test save/restore after choice/failure and repeated passive modifier expiry/respec. Run the integrated M12–M13 proving route with both protagonists. Map actors/World Partition, ability cleanup, Blueprint compilation, translation coverage, performance captures and licensed consoles remain separate required evidence.

Module loading and API visibility follow Epic's [Unreal module documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-modules?application_version=5.7) and [module API specifier rules](https://dev.epicgames.com/documentation/en-us/unreal-engine/module-api-specifiers-in-unreal-engine?application_version=5.7).
