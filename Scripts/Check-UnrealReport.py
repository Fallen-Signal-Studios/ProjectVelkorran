#!/usr/bin/env python3
"""Fail closed unless an Unreal JSON report covers this source's selected native tests.

This checks an existing engine report. It never runs or simulates Unreal tests.
"""
import argparse
import json
from pathlib import Path
import re
import sys


class ReportError(ValueError):
    pass


def selected(name, prefix):
    return name.casefold() == prefix.casefold() or name.casefold().startswith(prefix.casefold() + ".")


def uncomment(source):
    # Preserve quoted strings: URLs or comment-like text inside test names are not comments.
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(tokens, lambda m: " " if m[0].startswith(("//", "/*")) else m[0], source)


def source_tests(root, prefix):
    names = {}
    macro = re.compile(r'IMPLEMENT_SIMPLE_AUTOMATION_TEST\s*\(\s*\w+\s*,\s*"([^"\n]+)"')
    for directory in (root / "Source", root / "Plugins"):
        if not directory.is_dir():
            continue
        for path in sorted(directory.rglob("*.cpp")):
            for match in macro.finditer(uncomment(path.read_text(encoding="utf-8-sig"))):
                name = match[1]
                if not selected(name, prefix):
                    continue
                key = name.casefold()
                if key in names:
                    raise ReportError(f"Duplicate native test registration: {name} in {path} and {names[key][1]}")
                names[key] = (name, path)
    if not names:
        raise ReportError(f"No native simple-test registrations found for {prefix}; cannot certify coverage")
    return {value[0] for value in names.values()}


def nonnegative_int(obj, key, where):
    value = obj.get(key)
    if type(value) is not int or value < 0:
        raise ReportError(f"{where}.{key} must be a nonnegative integer")
    return value


def verify(report, expected, prefix):
    if not isinstance(report, dict):
        raise ReportError("Report must be an object")
    counts = {key: nonnegative_int(report, key, "report") for key in
              ("succeeded", "succeededWithWarnings", "failed", "notRun", "inProcess")}
    if counts["failed"] or counts["notRun"] or counts["inProcess"]:
        raise ReportError("Report contains failed, unrun or incomplete tests")
    rows = report.get("tests")
    if not isinstance(rows, list) or not rows:
        raise ReportError("Report must contain a nonempty tests array")
    if sum(counts.values()) != len(rows):
        raise ReportError("Report totals do not match its test rows")
    seen, matched, warning_rows = set(), set(), set()
    for row in rows:
        if not isinstance(row, dict) or not isinstance(row.get("fullTestPath"), str) or not row["fullTestPath"]:
            raise ReportError("Every test row needs a fullTestPath")
        name = row["fullTestPath"]
        key = name.casefold()
        if key in seen:
            raise ReportError(f"Duplicate report row: {name}")
        seen.add(key)
        if row.get("state") != "Success" or nonnegative_int(row, "errors", name) != 0:
            raise ReportError(f"Test did not pass: {name}")
        entries = row.get("entries", [])
        if not isinstance(entries, list):
            raise ReportError(f"Malformed event entries: {name}")
        for entry in entries:
            if not isinstance(entry, dict) or not isinstance(entry.get("event"), dict):
                raise ReportError(f"Malformed event: {name}")
            if str(entry["event"].get("type", "")).casefold() in ("error", "fatal"):
                raise ReportError(f"Error event hidden by successful aggregate: {name}")
            if str(entry["event"].get("type", "")).casefold() == "warning":
                warning_rows.add(key)
        if "warnings" in row and nonnegative_int(row, "warnings", name):
            warning_rows.add(key)
        if selected(name, prefix):
            matched.add(key)
    required = {name.casefold(): name for name in expected}
    missing = sorted(required[key] for key in required.keys() - matched)
    if missing:
        raise ReportError("Native tests missing from engine report (stale binaries or excluded modules): " + ", ".join(missing))
    if not matched or not expected:
        raise ReportError("No matching required native tests")
    return {"status": "passed", "test_filter": prefix, "source_tests": len(expected),
            "reported_matching_tests": len(matched), "warnings": max(counts["succeededWithWarnings"], len(warning_rows))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--filter", default="ProjectVelkorran")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        expected = source_tests(args.source_root, args.filter)
        result = verify(json.loads(args.report.read_text(encoding="utf-8-sig")), expected, args.filter)
    except (ReportError, OSError, UnicodeError, json.JSONDecodeError) as error:
        result = {"status": "failed", "error": str(error)}
    serialized = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(serialized, encoding="utf-8")
    print(serialized, end="")
    return 0 if result["status"] == "passed" else 2


if __name__ == "__main__":
    sys.exit(main())
