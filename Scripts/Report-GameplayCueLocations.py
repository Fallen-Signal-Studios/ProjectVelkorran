"""Enumerate every GameplayCue notify asset the project can see.

Run with UnrealEditor-Cmd and the PythonScript commandlet.

Motivation: the packaged runtime warns that no GameplayCueNotifyPaths are configured, so
the engine scans all of /Game/. Narrowing that is only safe if every cue notify lives in a
stable, enumerable set of roots - including any shipped by marketplace content, which a
name-based search would miss.

This reports by native parent class from the asset registry rather than by asset name, so a
cue notify called anything at all is still found. Read-only: nothing is modified or saved.
"""

import json
from collections import defaultdict

import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()

CUE_MARKER = "GameplayCueNotify"

result = {
    "by_root": defaultdict(list),
    "outside_known_roots": [],
    "total": 0,
    "known_roots": ["/Game/Cues", "/NarrativePro/Pro/Core/Abilities/Cues"],
}


def native_parent(asset):
    """Blueprint assets record their native parent; native assets report their own class."""
    tag = asset.get_tag_value("NativeParentClass")
    if tag:
        return str(tag)
    return str(asset.asset_class_path.asset_name)


for asset in registry.get_all_assets():
    path = str(asset.package_name)
    # Skip engine content: only project-reachable roots matter for the cue scan.
    if not (path.startswith("/Game/") or path.startswith("/NarrativePro/")):
        continue
    parent = native_parent(asset)
    own = str(asset.asset_class_path.asset_name)
    if CUE_MARKER not in parent and CUE_MARKER not in own:
        continue
    result["total"] += 1
    matched_root = None
    for root in result["known_roots"]:
        if path.startswith(root + "/") or path == root:
            matched_root = root
            break
    entry = {"path": path, "parent": parent}
    if matched_root:
        result["by_root"][matched_root].append(entry)
    else:
        result["outside_known_roots"].append(entry)

result["by_root"] = {k: v for k, v in result["by_root"].items()}
summary = {
    "total_cue_notifies": result["total"],
    "counts_by_known_root": {k: len(v) for k, v in result["by_root"].items()},
    "outside_known_roots_count": len(result["outside_known_roots"]),
    "outside_known_roots": result["outside_known_roots"][:40],
    "sample_inside": {k: [e["path"] for e in v[:5]] for k, v in result["by_root"].items()},
}

print("SOV_CUE_REPORT_BEGIN")
print(json.dumps(summary, indent=2, sort_keys=True))
print("SOV_CUE_REPORT_END")
