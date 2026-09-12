#!/usr/bin/env python3
"""Check the declared content prerequisites of the automation suite.

Reads Scripts/Manifests/ContentPrerequisites.json and reports, per prerequisite, one of three
states. The three are deliberately distinct because collapsing them is how a suite comes to look
qualified when it is not:

  SATISFIED              the content is present.
  MISSING_REQUIRED       a project-owned production dependency is absent. This FAILS validation.
                         A missing required dependency is a defect, never an excuse.
  PREREQUISITE_MISSING   an external or optional dependency is absent, so its tests cannot execute.
                         Reported as UNRUNNABLE. Never counted as passed.

Exit codes, so a caller cannot treat any of it as a pass by accident:
  0  every prerequisite satisfied and every expected-absent fixture still absent
  2  at least one MISSING_REQUIRED
  3  no MISSING_REQUIRED, but at least one PREREQUISITE_MISSING (unrunnable tests exist)
  4  manifest itself is unusable, or an expected-absent fixture now exists

Read-only. Never invokes UBT, the editor, or a cook.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "Scripts/Manifests/ContentPrerequisites.json"
GIT_CANDIDATES = (
    "/Library/Developer/CommandLineTools/usr/bin/git",
    "/usr/bin/git",
    "git",
)

SATISFIED = "SATISFIED"
MISSING_REQUIRED = "MISSING_REQUIRED"
PREREQUISITE_MISSING = "PREREQUISITE_MISSING"

# A category's absence semantics are fixed by the manifest's own policy block; on_absence must agree
# with the category so a prerequisite cannot be quietly downgraded by editing one field.
CATEGORY_ABSENCE = {
    "project_owned": "fail",
    "external": "unrunnable",
    "optional": "unrunnable",
}


class ManifestError(RuntimeError):
    pass


def git_binary() -> str:
    for candidate in GIT_CANDIDATES:
        try:
            subprocess.run([candidate, "--version"], capture_output=True, check=True)
            return candidate
        except (OSError, subprocess.CalledProcessError):
            continue
    raise ManifestError("No usable git binary found; cannot determine tracking state.")


def is_tracked(git: str, disk_path: str) -> bool:
    result = subprocess.run([git, "-C", str(ROOT), "ls-files", "--", disk_path],
                            capture_output=True, text=True)
    return bool(result.stdout.strip())


def content_present(disk_path: str, kind: str) -> bool:
    target = ROOT / disk_path
    if kind == "directory":
        # A directory counts as present only if it actually holds assets; an empty folder left by a
        # tool is not the content.
        return target.is_dir() and any(target.rglob("*.uasset"))
    return any((ROOT / (disk_path + suffix)).exists() for suffix in (".uasset", ".umap", ""))


def evaluate(manifest: dict, git: str) -> dict:
    report = {"prerequisites": [], "expected_absent_violations": [], "policy_violations": []}
    for entry in manifest.get("prerequisites", []):
        category = entry.get("category")
        declared = entry.get("on_absence")
        expected = CATEGORY_ABSENCE.get(category)
        if expected is None:
            report["policy_violations"].append(
                f"{entry.get('id')}: unknown category '{category}'")
            continue
        if declared != expected:
            # Refuse a manifest that would soften a required dependency into an unrunnable one.
            report["policy_violations"].append(
                f"{entry.get('id')}: category '{category}' requires on_absence '{expected}', "
                f"manifest says '{declared}'")
        present = content_present(entry["disk_path"], entry.get("kind", "asset"))
        tracked = is_tracked(git, entry["disk_path"])
        if present:
            state = SATISFIED
        else:
            state = MISSING_REQUIRED if expected == "fail" else PREREQUISITE_MISSING
        report["prerequisites"].append({
            "id": entry.get("id"),
            "content_path": entry.get("content_path"),
            "category": category,
            "state": state,
            "present": present,
            "tracked": tracked,
            "declared_tracked": entry.get("tracked"),
            "dependent_tests": entry.get("dependent_tests", []),
            "action_required": entry.get("action_required"),
        })
        if present and not tracked:
            # Present locally but untracked is exactly how the suite came to look green on one
            # machine and fail everywhere else.
            report["policy_violations"].append(
                f"{entry.get('id')}: present on disk but untracked, so no other clone has it")

    for group in manifest.get("expected_absent_fixtures", []):
        for path in group.get("content_paths", []):
            if not path.startswith("/Game/"):
                continue
            disk = "Content/" + path[len("/Game/"):]
            if any((ROOT / (disk + suffix)).exists() for suffix in (".uasset", ".umap")):
                report["expected_absent_violations"].append(path)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--json", type=Path, help="Write the machine-readable report here.")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"FAILED: unusable manifest {args.manifest}: {error}", file=sys.stderr)
        return 4
    if manifest.get("schema_version") != 1:
        print(f"FAILED: unsupported schema_version {manifest.get('schema_version')}", file=sys.stderr)
        return 4

    report = evaluate(manifest, git_binary())
    required_missing = [p for p in report["prerequisites"] if p["state"] == MISSING_REQUIRED]
    unrunnable = [p for p in report["prerequisites"] if p["state"] == PREREQUISITE_MISSING]
    report["summary"] = {
        "satisfied": sum(1 for p in report["prerequisites"] if p["state"] == SATISFIED),
        "missing_required": len(required_missing),
        "prerequisite_missing": len(unrunnable),
        "qualifies": not required_missing and not unrunnable
                     and not report["policy_violations"]
                     and not report["expected_absent_violations"],
    }
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")

    if not args.quiet:
        for item in report["prerequisites"]:
            print(f"  {item['state']:20} {item['content_path']}  ({item['category']})")
            for test in item["dependent_tests"]:
                print(f"      dependent test: {test}")
            if item["state"] != SATISFIED and item["action_required"]:
                print(f"      action: {item['action_required']}")
        for violation in report["policy_violations"]:
            print(f"  POLICY VIOLATION     {violation}")
        for path in report["expected_absent_violations"]:
            print(f"  FIXTURE NOW EXISTS   {path} was declared absent; its test may be vacuous")

    if report["expected_absent_violations"] or report["policy_violations"]:
        print("\nFAILED: manifest policy violated.", file=sys.stderr)
        return 4
    if required_missing:
        print(f"\nFAILED: {len(required_missing)} required content prerequisite(s) missing. "
              "A missing production dependency is a defect, not an excuse.", file=sys.stderr)
        return 2
    if unrunnable:
        print(f"\nNOT QUALIFIED: {len(unrunnable)} prerequisite(s) missing, so dependent tests are "
              "unrunnable. Unrunnable is never a pass.", file=sys.stderr)
        return 3
    if not args.quiet:
        print("\nAll declared content prerequisites satisfied.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
