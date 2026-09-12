#!/usr/bin/env python3
"""Build Project Velkorran and verify its native automation report with UE 5.7 on macOS.

macOS is a SECOND supported engineering environment. It is NOT a replacement for the
Windows validation environment, and this script must never be read as satisfying a
Windows gate. A green run here qualifies exactly three things:

  1. the source compiles for Mac editor targets under Apple clang,
  2. the native automation suite passes on Mac,
  3. the source did not change while (1) and (2) ran.

Everything in WINDOWS_ONLY_GATES stays outstanding regardless of this script's result,
and is printed as outstanding on every successful run so a passing Mac run cannot be
mistaken for full validation. Scripts/Validate-Unreal.ps1 remains the Windows gate.

Mirrors Validate-Unreal.ps1's checks: source manifest before/after, editor build,
binary-freshness assertion, automation report field validation, per-test state
validation, and Check-UnrealReport.py source coverage.

.EXAMPLE
  Scripts/Validate-UnrealMac.py
  Scripts/Validate-UnrealMac.py --build-game --filter ProjectVelkorran
"""
import argparse
import datetime as _dt
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ENGINE = Path("/Users/Shared/Epic Games/UE_5.7")

# Gates that macOS structurally cannot satisfy. Printed on every successful run.
WINDOWS_ONLY_GATES = [
    "Windows editor build and automation (Scripts/Validate-Unreal.ps1).",
    "Packaged Win64 Game target (TDD alignment open item 5: editor-green does not prove a shipping build).",
    "Packaged Win64 cold-start sweep (Scripts/Run-AIStartupColdStarts.ps1) — startup ordering is "
    "platform-specific timing; a Mac run is a separate data point, never a reproduction.",
    "MSVC unity-build conformance — Apple clang and MSVC do not agree on every diagnostic.",
    "A runnable Mac .app — .app finalization needs xcodebuild, which this host's Xcode cannot run.",
]

FILTER_PATTERN = re.compile(r"^(?:ProjectVelkorran|NarrativeArsenal)(?:\.[A-Za-z0-9_]+)*$")


class ValidationError(RuntimeError):
    pass


def run_logged(run_dir: Path, name: str, command: list, timeout: int) -> int:
    """Run a command, tee stdout+stderr to the run directory, return its exit code."""
    log = run_dir / f"{name}.log"
    print(f"  -> {name}: {command[0]}", flush=True)
    with log.open("w", encoding="utf-8") as handle:
        handle.write(f"$ {' '.join(str(part) for part in command)}\n\n")
        handle.flush()
        completed = subprocess.run(command, stdout=handle, stderr=subprocess.STDOUT,
                                   timeout=timeout, text=True)
    return completed.returncode


def module_source_roots(source_root: Path) -> dict:
    """Map module name -> its source directory, from every <Module>.Build.cs in the tree."""
    roots = {}
    for base in (source_root / "Source", source_root / "Plugins"):
        if not base.is_dir():
            continue
        for rules in base.rglob("*.Build.cs"):
            if "Intermediate" in rules.parts:
                continue
            roots[rules.name[: -len(".Build.cs")]] = rules.parent
    return roots


def newest_source_mtime(directory: Path) -> tuple:
    """Newest build-input mtime under one directory, with the file that carries it."""
    newest, carrier = 0.0, None
    for path in directory.rglob("*"):
        if path.suffix.lower() not in {".h", ".cpp", ".inl", ".cs"}:
            continue
        if "Intermediate" in path.parts or "Binaries" in path.parts:
            continue
        stamp = path.stat().st_mtime
        if stamp > newest:
            newest, carrier = stamp, path
    return newest, carrier


def assert_binaries_fresh(source_root: Path) -> dict:
    """Refuse to run automation against a module binary older than that module's own source.

    Compared per module, never against the newest file in the tree: a plugin binary does
    not become stale because an unrelated test in another module changed. This assertion
    exists because a cook has already once consumed a binary that predated its source,
    producing results that looked valid and proved nothing.
    """
    roots = module_source_roots(source_root)
    # Only module binaries. A Mac build also stages third-party libraries (boost, tbb) into
    # Binaries/Mac; those are not built from project source and have no module to compare against.
    binaries = sorted(set(source_root.glob("Binaries/Mac/UnrealEditor-*.dylib")) |
                      set(source_root.glob("Plugins/*/Binaries/Mac/UnrealEditor-*.dylib")))
    if not binaries:
        raise ValidationError("No Mac editor binaries found. Build before validating.")
    checked, stale, unmapped = {}, [], []
    for binary in binaries:
        module = binary.stem[len("UnrealEditor-"):]
        root = roots.get(module)
        if root is None:
            unmapped.append(binary.name)
            continue
        newest, carrier = newest_source_mtime(root)
        relative = str(binary.relative_to(source_root))
        checked[relative] = module
        if newest > binary.stat().st_mtime:
            stale.append(f"{relative} predates {carrier.relative_to(source_root)}")
    if stale:
        raise ValidationError("Module binaries predate their own source and cannot qualify it: "
                              + "; ".join(stale))
    if unmapped:
        raise ValidationError("Binaries with no resolvable module source, so freshness is unprovable: "
                              + ", ".join(unmapped))
    return checked


# Errors that mean an asset was absent, not that code misbehaved. Content outside
# Content/Aurelion/ is deliberately untracked, so a fresh clone of either platform cannot load it.
CONTENT_ABSENCE_MARKERS = (
    "does not exist on disk",
    "Failed to find object",
    "SkipPackage",
    "Failed to load",
)


def describe_failures(report: dict) -> str:
    """List failing tests with their first error, flagging probable content absence.

    This only labels; it never excuses. A content-absence failure is still a failure of this run,
    because a suite that cannot load its assets has not qualified the project.
    """
    lines = []
    for test in report.get("tests", []):
        if test.get("state") in ("Success", "SuccessWithWarnings"):
            continue
        errors, absence = [], False
        for entry in test.get("entries", []):
            event = entry.get("event", {})
            message = str(event.get("message", ""))
            if any(marker in message for marker in CONTENT_ABSENCE_MARKERS):
                absence = True
            if event.get("type") == "Error" and len(errors) < 2:
                errors.append(message.strip()[:160])
        label = " [probable content absence, not a code regression]" if absence else ""
        lines.append(f"  - {test.get('fullTestPath')}{label}")
        lines.extend(f"      {error}" for error in errors)
    if not lines:
        return ""
    return "\nFailing tests:\n" + "\n".join(lines)


# A test that genuinely cannot execute because a declared prerequisite is absent emits this marker.
# It is deliberately a distinct state from both pass and fail: the code was never exercised, so the
# run carries no evidence about it either way. Nothing may treat it as a pass.
UNRUNNABLE_MARKER = "PREREQUISITE_MISSING"


def classify_tests(report: dict, filter_name: str) -> tuple:
    """Split selected tests into passed, failed and unrunnable.

    Unrunnable is detected from the test's own entries rather than inferred by the gate, so a test
    declares its own inability to run and cannot be silently excused from outside.
    """
    passed, failed, unrunnable = [], [], []
    for test in report.get("tests", []):
        path = test.get("fullTestPath", "")
        if not path.startswith(filter_name):
            continue
        entries = test.get("entries", [])
        declares_unrunnable = any(
            UNRUNNABLE_MARKER in str(entry.get("event", {}).get("message", ""))
            for entry in entries)
        if test.get("state") not in ("Success", "SuccessWithWarnings"):
            failed.append(path)
        elif declares_unrunnable:
            unrunnable.append(path)
        else:
            passed.append(path)
    return passed, failed, unrunnable


def validate_report(report_path: Path, filter_name: str) -> tuple:
    """Validate the automation report itself. A zero process exit code is not a pass."""
    if not report_path.is_file():
        raise ValidationError(
            f"Automation produced no JSON report: {report_path}. "
            "A zero process exit code alone is not a test pass.")
    report = json.loads(report_path.read_text(encoding="utf-8-sig"))
    for field in ("succeeded", "succeededWithWarnings", "failed", "notRun", "inProcess", "tests"):
        if field not in report:
            raise ValidationError(f"Automation report is missing required field '{field}'.")
    total_succeeded = report["succeeded"] + report["succeededWithWarnings"]
    if report["failed"] or report["notRun"] or report["inProcess"]:
        raise ValidationError(
            f"Automation did not complete successfully: passed={total_succeeded} "
            f"failed={report['failed']} notRun={report['notRun']} inProcess={report['inProcess']}."
            + describe_failures(report))
    selected = [t for t in report["tests"] if t.get("fullTestPath", "").startswith(filter_name)]
    if not selected:
        raise ValidationError(f"No automation tests matched filter '{filter_name}'.")
    for test in selected:
        state = test.get("state")
        if state not in ("Success", "SuccessWithWarnings"):
            raise ValidationError(f"Automation test did not pass: {test.get('fullTestPath')} (state={state}).")
    _, _, unrunnable = classify_tests(report, filter_name)
    return report, selected, unrunnable


PREREQUISITE_EXITS = {
    0: "all satisfied",
    2: "a required production dependency is MISSING — dependent tests are expected to fail",
    3: "a prerequisite is missing, so dependent tests are UNRUNNABLE",
    4: "the prerequisite manifest itself is violated",
}


def check_prerequisites(run_dir: Path, python_exe) -> dict:
    """Run the content prerequisite check before automation, so a content cause is visible first.

    Never raises: the run continues so every other result is still gathered. The aggregate verdict
    at the end refuses qualification on anything but exit 0.
    """
    checker = ROOT / "Scripts/Check-ContentPrerequisites.py"
    report_path = run_dir / "content-prerequisites.json"
    exit_code = run_logged(run_dir, "ContentPrerequisites",
                           [str(python_exe), str(checker), "--json", str(report_path)], 300)
    meaning = PREREQUISITE_EXITS.get(exit_code, f"unexpected exit {exit_code}")
    print(f"     content prerequisites: {meaning}")
    detail = {}
    if report_path.is_file():
        try:
            detail = json.loads(report_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            detail = {}
    return {"exit": exit_code, "meaning": meaning, "summary": detail.get("summary", {})}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine-root", type=Path, default=DEFAULT_ENGINE)
    parser.add_argument("--project", type=Path, default=ROOT / "ProjectVelkorran.uproject")
    parser.add_argument("--filter", default="ProjectVelkorran")
    parser.add_argument("--output-directory", type=Path)
    parser.add_argument("--python", type=Path, help="Python for the manifest/coverage helpers.")
    parser.add_argument("--automation-timeout", type=int, default=1800)
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--build-game", action="store_true",
                        help="Also compile the Mac Game target. Proves the runtime module carries no "
                             "editor-only dependency; does NOT package or cook.")
    parser.add_argument("--non-unity", action="store_true")
    args = parser.parse_args()

    if platform.system() != "Darwin":
        raise ValidationError(f"This is the macOS gate; host is {platform.system()}. "
                              "Use Scripts/Validate-Unreal.ps1 on Windows.")
    if not FILTER_PATTERN.match(args.filter):
        raise ValidationError(f"Refusing an unconstrained automation filter: '{args.filter}'.")

    engine = args.engine_root
    build_script = engine / "Engine/Build/BatchFiles/Mac/Build.sh"
    editor = engine / "Engine/Binaries/Mac/UnrealEditor"
    for required in (build_script, editor):
        if not required.exists():
            raise ValidationError(f"Engine component missing: {required}")

    python_exe = args.python or (engine / "Engine/Binaries/ThirdParty/Python3/Mac/bin/python3")
    if not Path(python_exe).exists():
        python_exe = Path(shutil.which("python3") or sys.executable)

    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = (args.output_directory or (ROOT / "Saved/MacValidation") / stamp)
    run_dir.mkdir(parents=True, exist_ok=True)
    print(f"Mac validation run: {run_dir}")

    summary = {
        "host": "macOS",
        "hostVersion": platform.mac_ver()[0],
        "arch": platform.machine(),
        "engineRoot": str(engine),
        "filter": args.filter,
        "startedUtc": _dt.datetime.now(_dt.timezone.utc).isoformat(),
        "build": "not run",
        "gameBuild": "not run",
        "automation": "not run",
        "windowsGates": "OUTSTANDING — not addressed by this script",
        "windowsOnlyGates": WINDOWS_ONLY_GATES,
    }

    def flush_summary():
        (run_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    flush_summary()

    manifest_tool = ROOT / "Scripts/Capture-SourceManifest.py"
    before_manifest = run_dir / "source-before.json"
    if run_logged(run_dir, "SourceBefore", [str(python_exe), str(manifest_tool),
                                            "--source-root", str(ROOT), "--filter", args.filter,
                                            "--output", str(before_manifest)], 300) != 0:
        raise ValidationError("Could not capture build inputs and native registrations.")

    prerequisites = check_prerequisites(run_dir, python_exe)
    summary["contentPrerequisites"] = prerequisites
    flush_summary()

    if not args.skip_build:
        targets = ["ProjectVelkorranEditor"] + (["ProjectVelkorran"] if args.build_game else [])
        for target in targets:
            command = [str(build_script), target, "Mac", "Development",
                       f"-Project={args.project}", "-WaitMutex"]
            if target == "ProjectVelkorran":
                # Compile and link only. The engine default bUseModernXcode=true finalizes a .app
                # by shelling out to xcodebuild, which cannot run on a host whose Xcode predates
                # its macOS. Overriding it on the command line keeps the engine and project config
                # untouched. This therefore qualifies exactly one thing -- that the runtime module
                # compiles and links with no editor-only dependency -- and NOT a runnable Mac .app,
                # which stays outstanding while the host Xcode is broken.
                command.append("-ini:Engine:[/Script/MacTargetPlatform.XcodeProjectSettings]:"
                               "bUseModernXcode=False")
            if args.non_unity:
                command.append("-DisableUnity")
            name = "Build" if target == "ProjectVelkorranEditor" else "BuildGame"
            exit_code = run_logged(run_dir, name, command, 7200)
            key = "build" if target == "ProjectVelkorranEditor" else "gameBuild"
            summary[key] = (f"exit {exit_code}" if target == "ProjectVelkorranEditor" else
                            f"exit {exit_code}; compiled and linked only, .app not finalized")
            flush_summary()
            if exit_code != 0:
                raise ValidationError(f"Mac build failed for {target} (exit {exit_code}). See {name}.log")
    else:
        print("  !! Build skipped. You are responsible for binaries matching the current source.")

    summary["binariesChecked"] = assert_binaries_fresh(ROOT)
    flush_summary()

    if args.build_only:
        summary["automation"] = "not run (--build-only)"
        flush_summary()
        print("Build skipped; module binary freshness verified. Automation was not run."
              if args.skip_build else "Mac build succeeded. Automation was not run.")
        if prerequisites["exit"] != 0:
            print(f"  content prerequisites: {prerequisites['meaning']}")
        report_outstanding()
        return 0 if prerequisites["exit"] == 0 else 3

    report_dir = run_dir / "AutomationReport"
    editor_log = run_dir / "UnrealEditor.log"
    automation_command = [
        str(editor), str(args.project), "-unattended", "-nop4", "-NullRHI", "-nosplash",
        "-stdout", "-FullStdOutLogOutput",
        f"-ExecCmds=Automation RunTests {args.filter}",
        "-TestExit=Automation Test Queue Empty",
        f"-ReportExportPath={report_dir}", f"-AbsLog={editor_log}",
    ]
    exit_code = run_logged(run_dir, "Automation", automation_command, args.automation_timeout)
    summary["automation"] = f"process exit {exit_code}; report not yet validated"
    flush_summary()
    if exit_code != 0:
        raise ValidationError(f"Unreal automation process failed with exit code {exit_code}.")

    report, selected, unrunnable = validate_report(report_dir / "index.json", args.filter)

    coverage = run_dir / "coverage.json"
    if run_logged(run_dir, "ReportCoverage", [str(python_exe), str(ROOT / "Scripts/Check-UnrealReport.py"),
                                             "--report", str(report_dir / "index.json"),
                                             "--source-root", str(ROOT), "--filter", args.filter,
                                             "--output", str(coverage)], 300) != 0:
        raise ValidationError("Automation source coverage validation failed. Inspect ReportCoverage.log.")

    after_manifest = run_dir / "source-after.json"
    if run_logged(run_dir, "SourceAfter", [str(python_exe), str(manifest_tool),
                                           "--source-root", str(ROOT), "--filter", args.filter,
                                           "--verify", str(before_manifest),
                                           "--output", str(after_manifest)], 300) != 0:
        raise ValidationError("Source changed during validation. This run cannot qualify the current source.")

    summary["sourceIntegrity"] = ("unchanged during automation; binary freshness asserted by mtime"
                                  if args.skip_build else "unchanged during build and automation")
    summary["automation"] = (f"passed {len(selected) - len(unrunnable)} matching tests; "
                             f"unrunnable={len(unrunnable)}; warnings={report['succeededWithWarnings']}")
    summary["unrunnableTests"] = unrunnable
    summary["finishedUtc"] = _dt.datetime.now(_dt.timezone.utc).isoformat()

    # Aggregate verdict. Every automation test passed by this point, but that alone is not
    # qualification: a missing prerequisite or an unrunnable test means the run carries no evidence
    # about part of the suite, and absence of evidence is never a pass.
    blockers = []
    if prerequisites["exit"] != 0:
        blockers.append(f"content prerequisites: {prerequisites['meaning']}")
    if unrunnable:
        blockers.append(f"{len(unrunnable)} test(s) declared themselves unrunnable: "
                        + ", ".join(unrunnable[:5]))
    summary["verdict"] = "NOT QUALIFIED" if blockers else "QUALIFIED"
    flush_summary()

    if blockers:
        print(f"\nmacOS run NOT QUALIFIED, though every executed test passed "
              f"({len(selected) - len(unrunnable)} of {len(selected)} matched tests ran).")
        for blocker in blockers:
            print(f"  - {blocker}")
        print("Unrunnable and missing prerequisites are not passes.")
        print(f"Report: {report_dir / 'index.json'}")
        report_outstanding()
        return 3

    print(f"\nmacOS validation passed: {len(selected)} matching automation tests, "
          f"0 unrunnable, all content prerequisites satisfied.")
    print(f"Report: {report_dir / 'index.json'}")
    report_outstanding()
    return 0


def report_outstanding():
    print("\n" + "=" * 78)
    print("macOS is a second engineering environment, not a Windows substitute.")
    print("These gates remain OUTSTANDING and are not addressed by this run:")
    for gate in WINDOWS_ONLY_GATES:
        print(f"  - {gate}")
    print("=" * 78)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValidationError, subprocess.TimeoutExpired) as error:
        print(f"\nFAILED: {error}", file=sys.stderr)
        sys.exit(1)
