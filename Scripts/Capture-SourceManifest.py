#!/usr/bin/env python3
"""Capture/verify build inputs and native registrations. Does not run Unreal."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys


SCHEMA = 1
EXCLUDED = {"Binaries", "Intermediate", "Saved", "DerivedDataCache", "StagedBuilds", "__pycache__", ".git"}
EXTENSIONS = {".h", ".hpp", ".inl", ".cpp", ".c", ".cs", ".ini", ".json", ".py", ".ps1", ".sh", ".uproject", ".uplugin", ".usf", ".ush"}


def reject_link(path, root):
    # Windows junctions and other reparse directories must not silently hide
    # build inputs from rglob either. Generated output is excluded by the caller.
    if path.is_symlink() or (getattr(path.lstat(), "st_file_attributes", 0) & 0x400):
        raise ValueError(f"Build input is a link/reparse point and cannot be attested: {path.relative_to(root).as_posix()}")


def source_files(root):
    files = []
    for folder in ("Source", "Plugins", "Config", "Scripts", "Tests"):
        directory = root / folder
        if directory.exists() or directory.is_symlink():
            reject_link(directory, root)
        if not directory.is_dir():
            continue
        for path in directory.rglob("*"):
            relative = path.relative_to(root)
            if any(part in EXCLUDED for part in relative.parts):
                continue
            reject_link(path, root)
            if path.suffix.lower() not in EXTENSIONS:
                continue
            if path.is_file():
                files.append(path)
    for path in root.glob("*.uproject"):
        reject_link(path, root)
        if path.is_file():
            files.append(path)
    return sorted(set(files), key=lambda path: path.relative_to(root).as_posix())


def inventory(root, prefix):
    spec = importlib.util.spec_from_file_location("velkorran_report_check", Path(__file__).with_name("Check-UnrealReport.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return sorted(module.source_tests(root, prefix), key=str.casefold)


def capture(root, prefix="ProjectVelkorran"):
    root = root.resolve()
    records = [{"path": path.relative_to(root).as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
               for path in source_files(root)]
    if not records:
        raise ValueError("No build inputs found")
    tests = inventory(root, prefix)
    payload = {"files": records, "test_filter": prefix, "native_tests": tests}
    fingerprint = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    try:
        revision = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], check=True,
                                  text=True, capture_output=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        revision = None
    return {"schema_version": SCHEMA, "git_revision": revision, "fingerprint": fingerprint,
            "file_count": len(records), "native_test_count": len(tests), **payload}


def verify(expected, actual):
    if expected.get("schema_version") != SCHEMA or actual.get("schema_version") != SCHEMA:
        raise ValueError("Unsupported source manifest schema")
    for manifest in (expected, actual):
        if not isinstance(manifest.get("files"), list) or not isinstance(manifest.get("native_tests"), list):
            raise ValueError("Malformed source manifest")
        payload = {key: manifest[key] for key in ("files", "test_filter", "native_tests")}
        digest = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
        if digest != manifest.get("fingerprint") or manifest.get("file_count") != len(manifest["files"]) or manifest.get("native_test_count") != len(manifest["native_tests"]):
            raise ValueError("Source manifest contents do not match its fingerprint/counts")
    if expected["fingerprint"] != actual["fingerprint"]:
        before = {row["path"]: row["sha256"] for row in expected["files"]}
        after = {row["path"]: row["sha256"] for row in actual["files"]}
        changed = sorted(path for path in before.keys() | after.keys() if before.get(path) != after.get(path))
        detail = ", ".join(changed[:20]) or "native test inventory/filter changed"
        raise ValueError("Build inputs changed during validation: " + detail)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--filter", default="ProjectVelkorran")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--verify", type=Path)
    args = parser.parse_args()
    try:
        actual = capture(args.source_root, args.filter)
        if args.verify:
            verify(json.loads(args.verify.read_text(encoding="utf-8-sig")), actual)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(actual, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"status": "passed", "fingerprint": actual["fingerprint"],
                          "git_revision": actual["git_revision"], "file_count": actual["file_count"],
                          "native_test_count": actual["native_test_count"], "engine_executed": False}))
        return 0
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(json.dumps({"status": "failed", "error": str(error), "engine_executed": False}))
        return 2


if __name__ == "__main__":
    sys.exit(main())
