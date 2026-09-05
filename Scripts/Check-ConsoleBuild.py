#!/usr/bin/env python3
"""Conservative descriptor preflight, not an Unreal/SDK build or certification test."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path


REQUIRED_NARRATIVE_MODULES = {
    "NarrativePro", "NarrativeArsenal", "NarrativeCommonUI", "NarrativeSaveSystem"
}


def allowed(descriptor: dict, prefix: str, value: str) -> bool:
    allow = descriptor.get(prefix + "AllowList", descriptor.get("Whitelist" + prefix + "s", []))
    deny = descriptor.get(prefix + "DenyList", descriptor.get("Blacklist" + prefix + "s", []))
    explicit_empty = prefix == "Platform" and descriptor.get("HasExplicitPlatforms", False)
    return value not in deny and (value in allow if allow else not explicit_empty)


def discover_plugins(root: Path) -> dict[str, Path]:
    found = {}
    for directory, children, files in os.walk(root):
        children[:] = sorted(name for name in children if name not in {
            "Resources", "Content", "Binaries", "Intermediate", ".git"
        })
        descriptors = sorted(name for name in files if name.endswith(".uplugin"))
        if descriptors:
            children[:] = []  # Do not mistake bundled templates for installed plugins.
        for filename in descriptors:
            path = Path(directory) / filename
            if path.stem in found:
                raise ValueError(f"Duplicate plugin descriptor {path.stem}: {found[path.stem]} / {path}")
            found[path.stem] = path
    return found


def audit(project: Path, platform: str | None, plugin_roots: list[Path]) -> list[dict]:
    findings = []

    def report(severity: str, code: str, subject: str, message: str):
        findings.append(dict(severity=severity, code=code, subject=subject, message=message))

    data = json.loads(project.read_text(encoding="utf-8-sig"))
    registry = {}
    for root in plugin_roots:
        if not root.is_dir():
            raise ValueError(f"Plugin search directory does not exist: {root}")
        for name, path in discover_plugins(root).items():
            if name in registry:
                raise ValueError(f"Duplicate external plugin descriptor {name}")
            registry[name] = path
    # Project plugins override engine plugins, matching the local project source audit.
    registry.update(discover_plugins(project.parent / "Plugins"))
    pending = list(data.get("Plugins", []))
    visited = set()
    found_required = set()
    while pending:
        reference = pending.pop(0)
        name = reference["Name"]
        if not reference.get("Enabled", False) or not allowed(reference, "Target", "Game"):
            continue
        if platform and not allowed(reference, "Platform", platform):
            continue  # An explicitly conditional project feature, e.g. Windows TTS.
        if name in visited:
            continue
        visited.add(name)
        path = registry.get(name)
        if path is None:
            severity = "review" if reference.get("Optional", False) else "blocker"
            report(severity, "plugin-unresolved", name,
                   "Enabled plugin descriptor is absent from the supplied project/engine plugin roots.")
            continue
        descriptor = json.loads(path.read_text(encoding="utf-8-sig"))
        if descriptor.get("IsPluginExtension", False):
            report("review", "platform-extension", name,
                   "Platform extension merging must be checked by the licensed UnrealBuildTool.")
        for owner in (reference, descriptor):
            supported = owner.get("SupportedTargetPlatforms", [])
            if platform and supported and platform not in supported:
                report("blocker", "plugin-platform-filter", name,
                       f"SupportedTargetPlatforms does not contain the supplied token {platform}.")
                break
        for module in descriptor.get("Modules", []):
            module_name = module["Name"]
            if module_name not in REQUIRED_NARRATIVE_MODULES:
                continue
            found_required.add(module_name)
            platforms = module.get("PlatformAllowList", module.get("WhitelistPlatforms", []))
            if module.get("Type") not in {"Runtime", "RuntimeNoCommandlet", "RuntimeAndProgram"} or not allowed(module, "Target", "Game") or (
                platform is not None and not allowed(module, "Platform", platform)
            ):
                report("blocker", "required-module-filter", module_name,
                       "Required runtime module is excluded for the supplied Game target.")
            elif platform is None and (platforms or module.get("HasExplicitPlatforms", False)):
                report("blocker", "required-module-platform-review", module_name,
                       "Console target support is not established; current allow-list: " +
                       (", ".join(platforms) if platforms else "explicit platform extension required"))
        pending.extend(descriptor.get("Plugins", []))
    for module in sorted(REQUIRED_NARRATIVE_MODULES - found_required):
        report("blocker", "required-module-unresolved", module,
               "The project's required Narrative runtime module was not resolved.")
    report("review", "licensed-build-required", "Unreal Engine 5.7",
           "This reads descriptors only. Licensed platform extensions, SDK availability, UHT/UBT, "
           "cooking, staging, signing, device startup and certification remain unverified.")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path,
                        default=Path(__file__).resolve().parents[1] / "ProjectVelkorran.uproject")
    parser.add_argument("--platform-token", help="Exact platform identifier from your licensed UBT install; no token is guessed.")
    parser.add_argument("--plugin-root", action="append", default=[], type=Path,
                        help="Installed Engine/Plugins or external plugin directory; may be repeated.")
    args = parser.parse_args()
    try:
        findings = audit(args.project, args.platform_token, args.plugin_root)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    blocked = any(item["severity"] == "blocker" for item in findings)
    print(json.dumps(dict(scope="source descriptor preflight", target=args.platform_token,
                          descriptor_blocked=blocked, compiled=False, certified=False,
                          findings=findings), indent=2))
    return 1 if blocked else 0


if __name__ == "__main__":
    raise SystemExit(main())
