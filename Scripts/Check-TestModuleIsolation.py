#!/usr/bin/env python3
"""Check native fixture module boundaries and optionally supplied packaged-build metadata.

Never invokes UBT, UHT, cooking or native automation. Metadata checks are not a
binary/reflection runtime test or platform certification. Use fresh build outputs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


MODULE = "ProjectVelkorranTests"
ROOT = Path(__file__).resolve().parents[1]


class IsolationError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise IsolationError(message)


def audit_source(root):
    descriptor = json.loads((root / "ProjectVelkorran.uproject").read_text())
    entries = [m for m in descriptor.get("Modules", []) if m.get("Name") == MODULE]
    require(len(entries) == 1, "Exactly one test-module descriptor is required")
    module = entries[0]
    require(module.get("Type") == "Editor", "Reflected fixtures must belong to an Editor module")
    require(module.get("TargetAllowList") == ["Editor"], "Test module must explicitly exclude all Game targets")
    test_root = root / "Source" / MODULE
    require(test_root.is_dir(), "Test module source is absent")
    rules = (test_root / f"{MODULE}.Build.cs").read_text()
    require("Target.Type != TargetType.Editor" in rules and "throw new BuildException" in rules,
            "Test module needs a build-time non-Editor guard")
    game_target = (root / "Source/ProjectVelkorran.Target.cs").read_text()
    require(MODULE not in game_target, "Game target explicitly includes the test module")
    for rules_file in (root / "Source").rglob("*.Build.cs"):
        if not rules_file.is_relative_to(test_root):
            require(MODULE not in rules_file.read_text(), f"Runtime module depends on fixtures: {rules_file}")
    fixtures, registrations = set(), 0
    for base in (root / "Source", root / "Plugins"):
        for path in base.rglob("*.h"):
            text = path.read_text(errors="replace")
            if "Tests" not in path.parts or "UCLASS" not in text:
                continue
            require(path.is_relative_to(test_root), f"Reflected fixture outside Editor module: {path}")
            fixtures.update(re.findall(r"UCLASS\([^)]*\)\s*class\s+(?:\w+_API\s+)?(\w+)", text))
        for path in base.rglob("*.cpp"):
            text = path.read_text(errors="replace")
            if re.search(r"^\s*IMPLEMENT_(?:SIMPLE|COMPLEX)_AUTOMATION_TEST\s*\(", text, re.MULTILINE):
                require(path.is_relative_to(test_root), f"Native registration outside Editor module: {path}")
                registrations += len(re.findall(r"^\s*IMPLEMENT_(?:SIMPLE|COMPLEX)_AUTOMATION_TEST\s*\(", text, re.MULTILINE))
    require(fixtures and registrations, "Fixture and native test scans must be nonempty")
    return {"source_status": "passed", "fixture_classes": sorted(fixtures), "native_registrations": registrations}


def verify_packaged_metadata(receipt, manifest, staged_paths, fixtures, platform, configuration):
    require(isinstance(receipt, dict) and isinstance(manifest, dict), "Receipt and UHT manifest must be objects")
    require(receipt.get("TargetName") == "ProjectVelkorran", "Supply the Game receipt, not Editor receipt")
    require(receipt.get("TargetType", "Game") == "Game", "Receipt is not a Game target")
    require(configuration in ("Development", "Shipping"), "Only Development or Shipping Game evidence qualifies")
    require(receipt.get("Configuration") == configuration, "Receipt configuration does not match the requested gate")
    require(receipt.get("Platform") == platform and bool(platform), "Receipt platform does not match the requested gate")
    products = receipt.get("BuildProducts")
    require(isinstance(products, list) and products, "Receipt has no build products")
    require(any(isinstance(p, dict) and p.get("Type") == "Executable" and p.get("Path") for p in products),
            "Game receipt must contain an executable")
    modules = manifest.get("Modules")
    require(manifest.get("TargetName") == "ProjectVelkorran", "UHT manifest must describe the same Game target")
    require(isinstance(modules, list) and modules, "UHT manifest has no modules")
    names = [m.get("Name") for m in modules if isinstance(m, dict)]
    require(len(names) == len(modules) and all(isinstance(n, str) and n for n in names), "Malformed UHT module entries")
    require(len(set(names)) == len(names), "Duplicate UHT module names")
    require("ProjectVelkorran" in names and "NarrativeArsenal" in names, "Game/Narrative reflection evidence is missing")
    require(MODULE not in names, "Editor test module was included by Game UHT")
    require(isinstance(staged_paths, str) and staged_paths.strip(), "A nonempty UAT staged-file manifest is required")
    require(re.search(r"\.(?:exe|app|pak|utoc)\b", staged_paths, re.IGNORECASE), "Staged manifest has no executable or cooked container")
    combined = json.dumps(receipt) + "\n" + json.dumps(manifest) + "\n" + staged_paths
    require(MODULE.casefold() not in combined.casefold(), "Test module appears in Game build/staging evidence")
    require(not re.search(r"[\\/]+Private[\\/]+Tests[\\/]+", combined, re.IGNORECASE), "Runtime fixture header appears in Game UHT/build evidence")
    for name in fixtures:
        # Reflected UClass names omit the native A/U prefix.
        for spelling in {name, name[1:] if name.startswith(("A", "U")) else name}:
            require(not re.search(r"(?<!\w)" + re.escape(spelling) + r"(?!\w)", combined),
                    f"Fixture class appears in packaged metadata: {spelling}")
    return {"metadata_status": "passed", "platform": platform, "configuration": configuration,
            "uht_modules": len(names), "runtime_validation": "not performed"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--uht-manifest", type=Path)
    parser.add_argument("--stage-manifest", type=Path)
    parser.add_argument("--platform")
    parser.add_argument("--configuration", choices=("Development", "Shipping"))
    args = parser.parse_args()
    try:
        result = audit_source(args.source_root)
        evidence = (args.receipt, args.uht_manifest, args.stage_manifest, args.platform, args.configuration)
        if any(evidence):
            require(all(evidence), "Packaged gate requires receipt, UHT manifest, stage manifest, platform and configuration together")
            result.update(verify_packaged_metadata(json.loads(args.receipt.read_text(encoding="utf-8-sig")),
                json.loads(args.uht_manifest.read_text(encoding="utf-8-sig")), args.stage_manifest.read_text(encoding="utf-8-sig"),
                result["fixture_classes"], args.platform, args.configuration))
            result["evidence_sha256"] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (args.receipt, args.uht_manifest, args.stage_manifest)}
        else:
            result["metadata_status"] = "not supplied"
        result["fixture_classes"] = len(result["fixture_classes"])
        print(json.dumps(result, indent=2))
        return 0
    except (IsolationError, OSError, ValueError, TypeError) as error:
        print(json.dumps({"source_status": "failed", "error": str(error)}, indent=2))
        return 2


if __name__ == "__main__":
    sys.exit(main())
