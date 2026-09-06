#!/usr/bin/env python3
"""Run UE 5.7 build, native automation, campaign preflight and optional packaging.

Python 3.9+, stdlib only. A passing run is evidence for these gates, never proof
of a packaged playthrough, platform certification or a shipping candidate.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import sys
import time
import uuid


FILTER = "ProjectVelkorran"


def helper(filename):
    spec = importlib.util.spec_from_file_location(filename, Path(__file__).with_name(filename))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_records(paths):
    records = []
    for path in sorted(set(paths), key=str):
        if path.is_symlink() or getattr(path.lstat(), "st_file_attributes", 0) & 0x400:
            raise ValueError(f"Linked/reparse input cannot be attested: {path}")
        if path.is_file():
            records.append({"path": str(path), "size": path.stat().st_size, "sha256": sha256(path)})
    return records


def tree_paths(root):
    return [root, *root.rglob("*")] if root.exists() or root.is_symlink() else []


def platform_tools(engine, target):
    batch = engine / "Engine/Build/BatchFiles"
    if target == "Mac":
        return {"build": batch / "Mac/Build.sh", "uat": batch / "RunUAT.sh",
                "editor": engine / "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"}
    return {"build": batch / "Build.bat", "uat": batch / "RunUAT.bat",
            "editor": engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"}


def applies(item, target):
    for key in ("PlatformAllowList", "SupportedTargetPlatforms"):
        if item.get(key) and target not in item[key]:
            return False
    return target not in item.get("PlatformDenyList", [])


def plugins_for(project, descriptor, engine, target):
    roots = [project.parent / "Plugins"]
    roots += [(project.parent / item).resolve() for item in descriptor.get("AdditionalPluginDirectories", [])]
    roots.append(engine / "Engine/Plugins")
    available = {}
    for root in roots:
        for path in sorted(root.rglob("*.uplugin")) if root.is_dir() else []:
            available.setdefault(path.stem, path)
    pending = [item["Name"] for item in descriptor.get("Plugins", [])
               if item.get("Enabled") and applies(item, target)]
    found, errors = {}, []
    while pending:
        name = pending.pop()
        if name in found:
            continue
        path = available.get(name)
        if path is None:
            errors.append(f"Enabled plugin is unavailable for {target}: {name}")
            continue
        data = read_json(path)
        if not isinstance(data, dict):
            raise ValueError(f"Plugin descriptor must be a JSON object: {path}")
        found[name] = path
        if not applies(data, target):
            errors.append(f"Enabled plugin descriptor excludes {target}: {name}")
        pending.extend(item["Name"] for item in data.get("Plugins", [])
                       if item.get("Enabled") and not item.get("Optional") and applies(item, target))
    return found, errors


def asset_path(value, object_path=True):
    expression = r"/Game/(?:[A-Za-z0-9_]+/)*([A-Za-z0-9_]+)"
    match = re.fullmatch(expression + (r"\.\1" if object_path else ""), value)
    if not match:
        raise ValueError(f"Expected full /Game/ {'object' if object_path else 'map package'} path: {value}")
    return value


def supplied_assets(args):
    missions = read_json(args.mission_manifest) if args.mission_manifest else []
    if not isinstance(missions, list) or any(not isinstance(item, str) for item in missions):
        raise ValueError("Mission manifest must be a JSON array of full /Game/.../Asset.Asset paths")
    for values, is_object in ((missions, True), (args.additional_asset, True), (args.map, False)):
        for value in values:
            asset_path(value, is_object)
        if len({value.casefold() for value in values}) != len(values):
            raise ValueError("Duplicate supplied asset/map paths")
    if not args.build_only and not missions:
        raise ValueError("--mission-manifest with real authored mission assets is required")
    if args.package and (args.build_only or not args.map):
        raise ValueError("--package requires a mission manifest and explicit --map entries, without --build-only")
    return missions


def preflight(args, host):
    errors, version, plugins = [], None, {}
    expected_host = "Darwin" if args.platform == "Mac" else "Windows"
    if host != expected_host:
        errors.append(f"{args.platform} execution requires {expected_host}; current host is {host}")
    project = read_json(args.project)
    if not isinstance(project, dict):
        raise ValueError("Project descriptor must be a JSON object")
    if args.project.name != "ProjectVelkorran.uproject" or project.get("EngineAssociation") != "5.7":
        errors.append("ProjectVelkorran.uproject with EngineAssociation 5.7 is required")
    version_path = args.engine_root / "Engine/Build/Build.version"
    if version_path.is_file():
        version = read_json(version_path)
        if not isinstance(version, dict):
            raise ValueError("Engine Build.version must be a JSON object")
        if (version.get("MajorVersion"), version.get("MinorVersion")) != (5, 7):
            errors.append("Supplied installation must be Unreal Engine 5.7")
    else:
        errors.append(f"Missing UE installation version: {version_path}")
    required = ["build"] + ([] if args.build_only else ["editor"]) + (["uat"] if args.package else [])
    for key in required:
        path = platform_tools(args.engine_root, args.platform)[key]
        if not path.is_file():
            errors.append(f"Missing {key} executable: {path}")
    plugins, plugin_errors = plugins_for(args.project, project, args.engine_root, args.platform)
    errors.extend(plugin_errors)
    if not args.build_only:
        for label, root in [("Project", args.project.parent / "Content"),
                            ("NarrativePro", plugins.get("NarrativePro", Path("/unavailable")).parent / "Content")]:
            if not any(path.suffix.lower() in (".uasset", ".umap") for path in root.rglob("*") if path.is_file()):
                errors.append(f"Restore actual {label} content before running engine gates: {root}")
    return {"errors": errors, "engine_version": version,
            "plugin_descriptors": {name: str(path) for name, path in sorted(plugins.items())}}


def input_snapshot(args, preflight_result):
    files = tree_paths(args.project.parent / "Content")
    files += tree_paths(args.project.parent / "Config")
    version = args.engine_root / "Engine/Build/Build.version"
    if version.exists():
        files.append(version)
    if args.mission_manifest:
        files.append(args.mission_manifest)
    for value in preflight_result["plugin_descriptors"].values():
        descriptor = Path(value)
        files.append(descriptor)
        for directory in ("Content", "Config", "Source"):
            files += tree_paths(descriptor.parent / directory)
    return file_records(files)


def stage_plan(args, run_dir, missions):
    tools = platform_tools(args.engine_root, args.platform)
    def script(path):
        return ["/bin/bash", str(path)] if args.platform == "Mac" else [str(path)]
    stages = []
    for target in ("ProjectVelkorranEditor", "ProjectVelkorran"):
        argv = script(tools["build"]) + [target, args.platform, "Development", f"-Project={args.project}", "-WaitMutex"]
        if args.non_unity:
            argv.append("-DisableUnity")
        stages.append({"name": "editor-build" if target.endswith("Editor") else "game-build", "argv": argv})
    common = [str(tools["editor"]), str(args.project), "-unattended", "-nop4", "-NullRHI",
              "-nosplash", "-stdout", "-FullStdOutLogOutput"]
    if not args.build_only:
        stages.append({"name": "native-automation", "argv": common + [f"-ExecCmds=Automation RunTests {FILTER}",
                       "-TestExit=Automation Test Queue Empty", f"-ReportExportPath={run_dir / 'AutomationReport'}",
                       f"-AbsLog={run_dir / 'native-editor.log'}"]})
        campaign = common + ["-run=SovValidateCampaign", "-ShippingValidation", "-Missions=" + ",".join(missions),
                             f"-AbsLog={run_dir / 'campaign-editor.log'}"]
        if args.additional_asset:
            campaign.append("-AdditionalAssets=" + ",".join(args.additional_asset))
        stages.append({"name": "campaign-preflight", "argv": campaign})
    if args.package:
        stages.append({"name": "package", "argv": script(tools["uat"]) + ["BuildCookRun", f"-project={args.project}",
                       "-target=ProjectVelkorran", f"-platform={args.platform}", "-clientconfig=Development", "-unattended",
                       "-nop4", "-utf8output", "-build", "-cook", "-stage", "-pak", "-package", "-archive",
                       "-map=" + "+".join(args.map), f"-archivedirectory={run_dir / 'Archive'}",
                       f"-stagingdirectory={run_dir / 'Staged'}"]})
    return [{**stage, "status": "not_run", "exit_code": None, "log": str(run_dir / (stage["name"] + ".log"))}
            for stage in stages]


def process_command(argv, windows):
    if windows and argv[0].lower().endswith(".bat"):
        # cmd.exe expands even inside quotes. Fail explicitly for unsafe path
        # characters; ordinary spaces and parentheses remain supported.
        if any(re.search(r'[%!^&|<>"\r\n]', value) for value in argv):
            raise ValueError("Windows batch arguments cannot contain shell expansion/metacharacters")
        command = " ".join('"' + re.sub(r"(\\+)$", r"\1\1", value) + '"' for value in argv)
        return subprocess.list2cmdline([os.environ.get("ComSpec", "cmd.exe")]) + ' /d /v:off /s /c "' + command + '"'
    return argv


def execute(argv, cwd, log, timeout):
    windows = os.name == "nt"
    with log.open("wb") as output:
        child = subprocess.Popen(process_command(argv, windows), cwd=cwd, stdout=output,
                                 stderr=subprocess.STDOUT, start_new_session=not windows)
        try:
            return {"exit_code": child.wait(timeout=timeout), "timed_out": False}
        except subprocess.TimeoutExpired:
            cleanup_error = None
            if windows:
                try:
                    subprocess.run(["taskkill", "/PID", str(child.pid), "/T", "/F"], stdout=output,
                                   stderr=subprocess.STDOUT, timeout=30, check=False)
                except (OSError, subprocess.SubprocessError) as error:
                    cleanup_error = str(error)
                finally:
                    if child.poll() is None:
                        child.kill()
            else:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            child.wait(timeout=30)
            return {"exit_code": 124, "timed_out": True, "cleanup_error": cleanup_error}


def package_inventory(args, run_dir):
    archive = run_dir / "Archive"
    for root in (run_dir, archive):
        if root.is_symlink() or (root.exists() and getattr(root.lstat(), "st_file_attributes", 0) & 0x400):
            raise ValueError(f"Package evidence root cannot be linked/reparse: {root}")
    # Anchor links to the physical fresh run, never to a possibly replaced Archive.
    archive_root = run_dir.resolve(strict=True) / "Archive"
    records = []
    for path in sorted(tree_paths(archive)):
        if path.is_symlink():
            # Mac app frameworks legitimately contain links. Attest their target
            # strings, while preventing an archive from depending on outside files.
            try:
                resolved = path.resolve(strict=True)
            except (OSError, RuntimeError) as error:
                raise ValueError(f"Archived symlink cannot be resolved: {path}") from error
            if not resolved.is_relative_to(archive_root):
                raise ValueError(f"Archived symlink escapes the archive: {path}")
            records.append({"path": str(path), "symlink_target": os.readlink(path)})
        elif getattr(path.lstat(), "st_file_attributes", 0) & 0x400:
            raise ValueError(f"Unsupported archived reparse point: {path}")
        elif path.is_file():
            records.extend(file_records([path]))
    if not records:
        raise ValueError("UAT exited successfully but produced no archived package")
    if any("projectvelkorrantests" in row["path"].casefold() for row in records):
        raise ValueError("Editor test module found in archived artifact names")
    paths = [Path(row["path"]) for row in records]
    files = {path for path in paths if path.is_file() and path.stat().st_size > 0}
    if args.platform == "Win64":
        executable = any(path.name == "ProjectVelkorran.exe" for path in files)
    else:
        executable = any(path.name == "ProjectVelkorran" and path.parent.name == "MacOS" for path in files)
    toc_files = [path for path in paths if path.suffix == ".utoc"]
    if any(path not in files or path.with_suffix(".ucas") not in files for path in toc_files):
        raise ValueError("Archived IoStore containers require nonempty regular .utoc and matching .ucas files")
    if not executable or not (toc_files or any(path.suffix == ".pak" for path in files)):
        raise ValueError("Archive lacks the expected game executable and cooked container")
    write_json(run_dir / "archive-inventory.json", records)
    return {"files": len(records), "manifest": "archive-inventory.json",
            "fixture_exclusion": "Only artifact names checked; reflected types/container contents remain unverified"}


def campaign_completion(stage, run_dir, mission_count):
    logs = [Path(stage["log"]), run_dir / "campaign-editor.log"]
    text = "\n".join(path.read_text(encoding="utf-8-sig", errors="replace") for path in logs if path.is_file())
    markers = re.findall(r"Native mission preflight: (\d+) assets, (\d+) errors\.", text)
    if not markers or any((int(count), int(errors)) != (mission_count, 0) for count, errors in markers):
        raise ValueError("Campaign commandlet did not record complete successful validation of the supplied mission set")
    return {"mission_count": mission_count, "errors": 0, "completion_marker_verified": True}


def run(args, executor=execute, host=None):
    args.project = args.project.resolve()
    args.engine_root = args.engine_root.resolve()
    if args.mission_manifest:
        args.mission_manifest = args.mission_manifest.resolve()
    output = (args.output or args.project.parent / "Saved/CampaignQualification").resolve()
    run_dir = output / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ-") + uuid.uuid4().hex[:12])
    run_dir.mkdir(parents=True, exist_ok=False)
    summary = {"schema_version": 1, "status": "failed", "project": str(args.project), "platform": args.platform,
               "execution": "real_processes" if executor is execute else "test_double_engine_unexecuted",
               "engine_processes_started": False,
               "dry_run": args.dry_run, "source_integrity": "not_checked", "stages": [],
               "builds_passed": False, "native_tests_passed": False, "campaign_preflight_passed": False,
               "package_produced": False, "shipping_candidate_qualified": False, "packaged_playthrough_verified": False,
               "limits": "Engine installation identity uses Build.version, not a full SDK/binary hash. Native automation uses NullRHI. "
                         "Plugin input capture covers explicitly enabled plugins and their required enabled dependencies, not default-enabled engine plugins. "
                         "Commandlet scope excludes map actors, World Partition, Blueprint compilation and playthroughs."}
    source_before = inputs_before = None
    source_tool = helper("Capture-SourceManifest.py")
    persist = lambda: write_json(run_dir / "summary.json", summary)
    try:
        missions = supplied_assets(args)
        summary["missions"], summary["maps"] = missions, args.map
        summary["stages"] = stage_plan(args, run_dir, missions)
        summary["preflight"] = preflight(args, host or platform.system())
        source_before = source_tool.capture(args.project.parent, FILTER)
        write_json(run_dir / "source-before.json", source_before)
        summary["source_manifest"] = "source-before.json"
        summary["native_test_count"] = source_before["native_test_count"]
        inputs_before = input_snapshot(args, summary["preflight"])
        write_json(run_dir / "content-before.json", inputs_before)
        if summary["preflight"]["errors"]:
            raise ValueError("; ".join(summary["preflight"]["errors"]))
        if args.dry_run:
            summary["status"] = "planned_only"
        else:
            for stage in summary["stages"]:
                stage["status"] = "running"
                persist()
                started = time.monotonic()
                try:
                    if executor is execute:
                        summary["engine_processes_started"] = True
                    result = executor(stage["argv"], args.project.parent, Path(stage["log"]), args.timeout)
                    stage.update(result)
                    stage["duration_seconds"] = round(time.monotonic() - started, 3)
                    if result.get("timed_out") or result.get("exit_code") != 0:
                        raise ValueError(f"{stage['name']} failed: exit={result.get('exit_code')}, timeout={result.get('timed_out')}")
                    if stage["name"] == "native-automation":
                        report = run_dir / "AutomationReport/index.json"
                        stage["coverage"] = helper("Check-UnrealReport.py").verify(read_json(report), source_before["native_tests"], FILTER)
                        stage["report_sha256"] = sha256(report)
                    if stage["name"] == "campaign-preflight":
                        stage["validation"] = campaign_completion(stage, run_dir, len(missions))
                    if stage["name"] == "package":
                        stage["archive"] = package_inventory(args, run_dir)
                    stage["status"] = "passed"
                except (OSError, ValueError, subprocess.SubprocessError) as error:
                    stage.update(status="failed", error=str(error))
                    raise
                finally:
                    persist()
            summary["status"] = "passed" if executor is execute else "test_double_completed"
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        summary["error"] = str(error)
    finally:
        if source_before is not None and inputs_before is not None:
            try:
                source_after = source_tool.capture(args.project.parent, FILTER)
                write_json(run_dir / "source-after.json", source_after)
                inputs_after = input_snapshot(args, summary["preflight"])
                write_json(run_dir / "content-after.json", inputs_after)
                source_tool.verify(source_before, source_after)
                if inputs_before != inputs_after:
                    raise ValueError("Content/config/plugin inputs changed during qualification")
                summary["source_integrity"] = "unchanged"
            except (OSError, ValueError, KeyError, TypeError) as error:
                summary.update(status="failed", source_integrity="failed", error=str(error))
        if summary["status"] == "passed" and summary["source_integrity"] == "unchanged":
            passed = {stage["name"] for stage in summary["stages"] if stage["status"] == "passed"}
            summary.update(builds_passed={"editor-build", "game-build"} <= passed,
                           native_tests_passed="native-automation" in passed,
                           campaign_preflight_passed="campaign-preflight" in passed, package_produced="package" in passed)
        persist()
    return (0 if summary["status"] in ("passed", "planned_only", "test_double_completed") else 2), run_dir, summary


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--engine-root", type=Path, required=True)
    result.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1] / "ProjectVelkorran.uproject")
    result.add_argument("--platform", choices=("Mac", "Win64"), default="Mac" if platform.system() == "Darwin" else "Win64")
    result.add_argument("--mission-manifest", type=Path)
    result.add_argument("--additional-asset", action="append", default=[])
    result.add_argument("--map", action="append", default=[])
    result.add_argument("--output", type=Path)
    result.add_argument("--timeout", type=int, default=3600, help="Maximum seconds per process stage (30 to 86400)")
    result.add_argument("--non-unity", action="store_true")
    result.add_argument("--package", action="store_true")
    result.add_argument("--build-only", action="store_true")
    result.add_argument("--dry-run", action="store_true")
    return result


def main():
    cli = parser()
    args = cli.parse_args()
    if not 30 <= args.timeout <= 86400:
        cli.error("--timeout must be between 30 and 86400 seconds")
    code, directory, summary = run(args)
    print(f"{summary['status']}: {directory / 'summary.json'}")
    if summary.get("error"):
        print(summary["error"], file=sys.stderr)
    return code


if __name__ == "__main__":
    sys.exit(main())
