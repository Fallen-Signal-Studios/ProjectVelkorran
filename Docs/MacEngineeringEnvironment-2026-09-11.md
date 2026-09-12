# macOS as a second engineering environment — 11 September 2026

This machine is a supported engineering environment for Project Velkorran. It is **not** a
replacement for the Windows validation environment, and no result produced here should be recorded as
satisfying a Windows gate.

Host: macOS 26.6.2, arm64, UE 5.7 at `/Users/Shared/Epic Games/UE_5.7`.

## What is qualified here

| Capability | Status |
|---|---|
| `ProjectVelkorranEditor Mac Development` build | Works. Apple clang, Mac SDK 15.2, UBA. |
| `ProjectVelkorran Mac Development` (Game target) compile and link | Works. Proves the runtime module carries no editor-only dependency. |
| Native automation suite | Runs. `Scripts/Validate-UnrealMac.py`. |
| Portable C++ policy suites | Run. `Scripts/Test-NativePolicies.py`. |

## What is not, and must stay outstanding

| Gate | Why macOS cannot close it |
|---|---|
| Windows editor build and automation | `Scripts/Validate-Unreal.ps1`. No PowerShell on this host. |
| Packaged Win64 Game target | TDD alignment open item 5. Editor-green does not prove a shipping build, and a Mac build does not prove a Windows one. |
| Packaged Win64 cold-start sweep | `Scripts/Run-AIStartupColdStarts.ps1` measures millisecond startup ordering. A Mac run is a **separate data point, never a reproduction** of the Windows finding. |
| MSVC conformance | Apple clang and MSVC disagree on some diagnostics. Clang-clean is not MSVC-clean. |
| Console frame-time and memory captures | Devkits. Not code-closable on any desktop host. |
| A runnable Mac `.app` | Blocked by the host Xcode fault below. |

`Scripts/Validate-UnrealMac.py` prints this outstanding list on **every successful run**, so a green
Mac run cannot be mistaken for full validation by whoever reads the output later.

## Host Xcode fault, and why the build works anyway

Xcode 16.2 is installed and cannot run on macOS 26.6.2. Every tool routed through the `/usr/bin`
shims aborts before doing anything:

```
Symbol not found: _XPCTypeBool
  Referenced from: .../CoreDevice.framework/Versions/A/CoreDevice
  Expected in:     .../Mercury.framework/Versions/A/Mercury
```

That takes out `xcodebuild`, `xcrun`, and therefore the `/usr/bin` wrappers for `clang`, `git` and
`python3`. The compilers and SDKs themselves are fine; only tool *discovery* is broken.

Unreal is unaffected for compilation because UBT resolves the toolchain itself rather than asking
`xcrun`. Consequences for anyone working here:

- Use `/Library/Developer/CommandLineTools/usr/bin/git`, not `git`.
- Use `/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Mac/bin/python3` (3.11.8),
  not `python3`.
- For standalone compiles, export
  `SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk`, or the compiler cannot find
  `<string>`.
- The Game target needs `.app` finalization suppressed, because finalization shells out to
  `xcodebuild`. `Validate-UnrealMac.py` passes
  `-ini:Engine:[/Script/MacTargetPlatform.XcodeProjectSettings]:bUseModernXcode=False` on the command
  line. **The engine and project config are deliberately left untouched** — `bUseModernXcode=true` is
  an engine default in `BaseEngine.ini`, not a project setting, so nothing in this repository is at
  fault and nothing in it needed changing.

The real fix is a current Xcode plus `sudo xcode-select -s`. That needs the machine owner's password
and is not something an agent should do.

## Commands

```bash
# Full Mac gate: build, freshness, automation, report coverage, source integrity.
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Mac/bin/python3" \
  Scripts/Validate-UnrealMac.py --build-game
```

```bash
# Portable suites only. No Unreal build.
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Python3/Mac/bin/python3" \
  Scripts/Test-NativePolicies.py --compiler /Library/Developer/CommandLineTools/usr/bin/clang++
```

## The Mac gate mirrors the Windows one

`Validate-UnrealMac.py` reproduces `Validate-Unreal.ps1`'s checks rather than inventing looser ones:
source manifest captured before and verified after, editor build, automation with the same arguments
and the same `-TestExit`, report field and per-test state validation, and `Check-UnrealReport.py`
source coverage. A zero process exit code is not treated as a pass anywhere.

It adds one check the PowerShell gate does not have: **per-module binary freshness**. Each
`UnrealEditor-<Module>.dylib` must postdate the newest build input under that module's own directory.
This exists because a packaged cook in this away pass once consumed a binary that predated its
source, producing results that looked valid and proved nothing.

Two things that check got wrong before it was right, both caught on real data and both worth
recording because the failure mode is the same one the check is meant to prevent — a validation step
that reports something other than what it claims to:

1. It first compared every binary against the *globally* newest source file, so an unrelated test
   edit marked plugin binaries stale. Freshness is now compared per module.
2. It then treated any unmappable binary as a failure, and a Mac Game build stages third-party
   libraries (boost, tbb) into `Binaries/Mac`. It now considers only `UnrealEditor-*` module
   binaries, and still fails on a module binary it cannot map.

Its negative control: touching one runtime source file makes the check name exactly
`Binaries/Mac/UnrealEditor-ProjectVelkorran.dylib` and nothing else.
