# Performance capture harness

Priority 5 of the away pass. Built and verified on macOS; see
[MacEngineeringEnvironment-2026-09-11.md](MacEngineeringEnvironment-2026-09-11.md) for what that
does and does not qualify.

## What this is, and what it deliberately is not

This is the code-reachable part of the performance gate: admission of frame samples, bounded
retention, and a reproducible verdict against a configured budget. It is **not** a performance
result, and it produces none on its own.

TDD Appendix F's performance requirement is gated on console frame-time and memory captures on
target hardware. The 11 September alignment review scores that row as *not closable by code* —
it needs devkits. Nothing here changes that. What code can do is make a capture **repeatable,
self-describing and impossible to quote out of context**, and that is the whole scope.

## Layout

| Piece | Location | Verified by |
|---|---|---|
| Budget and statistics rules | `Source/ProjectVelkorran/Public/Diagnostics/SovPerformancePolicy.h` | `Tests/Portable/SovPerformancePolicyTests.cpp` — 60,856 checks, no Unreal build |
| Capture plumbing | `Source/ProjectVelkorran/{Public,Private}/Diagnostics/SovPerformanceCaptureSubsystem.*` | `Source/ProjectVelkorranTests/Private/Tests/SovPerformanceCaptureTests.cpp` — 5 automation tests |

The split is deliberate and follows `SovDiagnosticsPolicy.h`: the rules are pure C++ with no Unreal
dependency, so they are exercised by the portable harness on any platform with a compiler, and the
subsystem owns only what needs an engine — cvars, ticking, and the file write.

## Design decisions worth keeping

**Absence of data is never a pass.** `EVerdict` has four states, and a capture with too few
steady-state samples returns `Insufficient`, not `Pass`. This is the direct lesson of the Priority 4
retraction in [AwayEngineeringLog-2026-09-11.md](AwayEngineeringLog-2026-09-11.md): a measurement
that never happened must not read as a measurement that succeeded. `QualifiesCapture()` exists so a
caller cannot treat `Insufficient` as an outcome by forgetting to check.

**An unusable budget is its own verdict.** A budget whose hard-stall threshold sits at or below the
target would fail every frame that merely meets the target. That is a configuration error, and
reporting it as `InvalidBudget` keeps it from being read as either a pass or a code regression.

**Percentiles are nearest-rank with no interpolation.** The result is always an observed sample, so a
verdict is reproducible across platforms and compilers rather than depending on floating-point
blending. The portable test pins this against `std::ceil` for every size 1..200 and every percentile
1..100 — added after a first version of the test failed to catch a floor-instead-of-ceiling defect,
because every case it checked used a sample count where the two agree.

**Percentile input is sorted; streak input is chronological.** Two orderings, named in the
parameters, because passing one where the other belongs is silently wrong rather than loudly wrong.

**Every capture records its own host.** `platform`, `build_configuration`, `editor_build` and
`rendering_disabled` are in the summary and in the exported file, plus a `scope` line stating the
capture is local to that platform and is not a console or certification capture. A frame-time number
without its host is not interpretable, and a desktop number must never be quotable as a console one.

**A capture with rendering disabled says so.** Automation runs under `-NullRHI`, where no GPU work
happens, so a frame time measured there cannot qualify a frame budget.
`ProjectVelkorran.Diagnostics.Performance.RecordsPlatform` asserts the flag matches
`FApp::CanEverRender()`, and the negative control for it fails exactly one test when the flag is
hardcoded false.

**No new module dependency.** The export writes JSON by hand rather than through the `Json` module,
matching `USovDiagnosticsSubsystem`, which writes TSV by hand for the same reason. Because there is
no serialiser to escape text, every field passes through `SafeField`, whose permitted character set
is `SovPerformancePolicy::ReportSafeCharacter` and is covered portably across all printable ASCII.

## Use

Arm before the world exists if you want the earliest frames, the same constraint the AI startup trace
documents — `-ExecCmds` runs too late to catch startup:

```bash
-dpcvars=sov.PerfCapture=1,sov.PerfCapture.TargetMs=16.6667
```

| Cvar | Default | Meaning |
|---|---|---|
| `sov.PerfCapture` | `0` | Arms capture. Never on by default. |
| `sov.PerfCapture.TargetMs` | `16.6667` | Steady-state budget. 60Hz. |
| `sov.PerfCapture.HardStallMs` | `100` | Any single frame at or above this fails outright. |
| `sov.PerfCapture.AllowedOverFraction` | `0.05` | Share of frames permitted above the target. |
| `sov.PerfCapture.JudgedPercentile` | `0.95` | Percentile the target is judged at. |

Bounds: 4096 samples retained, first 60 frames discarded as warm-up, 120 samples minimum before any
verdict. Export is an explicit call and writes one file under `Saved/Diagnostics/`.

## Outstanding

- **Console frame-time and memory captures on target hardware.** Needs devkits. Not code-closable,
  and this harness does not claim to substitute for it.
- **A Windows capture.** Everything here is verified on macOS. The rules are platform-independent and
  portably tested, but no Windows run has been made from this branch.
- **Memory is not captured.** This harness covers frame time only. The TDD row also requires memory.
- **No budget is authored yet.** The cvar defaults are a 60Hz placeholder, not an approved target per
  platform.
