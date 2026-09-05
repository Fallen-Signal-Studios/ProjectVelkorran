#!/usr/bin/env python3
"""UE 5.7 validation stages. A diagnostic success never qualifies a candidate.

The Windows PowerShell entry point locates Python and invokes this module. Every
run owns a new directory; engine reports are consumed only from that directory.
No engine, content, route driver or hardware measurements are simulated here.
"""
import argparse
import datetime as dt
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import uuid


class ValidationError(ValueError):
    pass


def load_checker(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def engine_host_supported():
    return os.name == 'nt'


def required_validator_coverage():
    """Known source gaps, not a route-config waiver or a claim of execution.

    Retire an entry only when the native commandlet owns the actual category,
    its negative fixtures run, and RequireCompleteCoverage is updated with it.
    """
    return {'complete': False, 'categories': [
        {'id': 'status_cleanup_declarations', 'required': True, 'implementation': 'missing',
         'execution': 'not_evaluated',
         'reason': 'No generalized native status/ability cleanup declaration validator exists.'},
        {'id': 'prohibited_runtime_class_closure', 'required': True, 'implementation': 'partial',
         'execution': 'not_evaluated',
         'reason': 'Vendor inheritance and legacy paths are checked; exhaustive prohibited runtime type coverage is absent.'}
    ]}


def fingerprint(root):
    """Hash modified/untracked source, config, scripts, shaders and authored assets."""
    paths = []
    for directory in ('Source', 'Plugins', 'Config', 'Scripts', 'Content', 'Shaders', '.github'):
        for path in (root / directory).rglob('*'):
            if path.is_file() and not any(p in ('Binaries', 'Intermediate', '__pycache__', '.git') for p in path.parts):
                if 'Content' in path.relative_to(root).parts or path.suffix.lower() in ('.cpp', '.h', '.inl', '.cs', '.ini', '.uplugin', '.py', '.ps1', '.json', '.usf', '.ush', '.yml', '.yaml'):
                    paths.append(path)
    paths.extend(root.glob('*.uproject'))
    paths.extend(root.glob('*.uplugin'))
    digest = hashlib.sha256()
    for path in sorted(set(paths)):
        digest.update(path.relative_to(root).as_posix().encode('utf-8') + b'\0')
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        digest.update(b'\0')
    return digest.hexdigest()


def process_command(argv, windows=None):
    # cmd.exe expands these characters even when .bat arguments are quoted.
    if (os.name == 'nt' if windows is None else windows) and Path(argv[0]).suffix.lower() in ('.bat', '.cmd'):
        if any(any(c in arg for c in '%!\r\n"') for arg in argv):
            raise ValidationError('Batch arguments cannot contain percent, exclamation, quotes or newlines')
        # list2cmdline quotes only whitespace-bearing arguments. cmd.exe also
        # interprets &|<>^(), so quote EVERY argument and give Popen the complete
        # Windows command line without a second layer of CRT argument escaping.
        def quote(value):
            return '"' + re.sub(r'(\\+)$', r'\1\1', value) + '"'
        return quote(os.environ.get('ComSpec', 'cmd.exe')) + ' /d /v:off /s /c "' + ' '.join(quote(arg) for arg in argv) + '"'
    return argv


def run_process(argv, cwd, log, timeout):
    """Capture one process and terminate its whole process tree on timeout."""
    started = time.monotonic()
    with log.open('wb') as output:
        process = subprocess.Popen(process_command(argv), cwd=cwd, stdout=output, stderr=subprocess.STDOUT,
                                   start_new_session=(os.name != 'nt'))
        try:
            code = process.wait(timeout=timeout)
        except (subprocess.TimeoutExpired, KeyboardInterrupt):
            if os.name == 'nt':
                subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], stdout=output, stderr=subprocess.STDOUT, timeout=30)
            else:
                os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=30)
            code = 124
    return {'exit_code': code, 'duration_seconds': round(time.monotonic() - started, 3)}


def stage_log_errors(path, strict_warnings, report_authority=False):
    # Commandlets can return zero despite logged errors. Preserve full logs and
    # reject Unreal/compiler severity tokens; do not treat ordinary prose as errors.
    pattern = r'(?:\bLog\w*:\s*(?:Error|Fatal):|\bfatal error\s+[A-Z]+\d+|\berror\s+[A-Z]+\d+:)'
    warnings = r'(?:\bLog\w*:\s*Warning:|\bwarning\s+[A-Z]+\d+:)'
    text = path.read_text(encoding='utf-8', errors='replace')
    if report_authority:
        # AddExpectedError tests intentionally emit raw Log*: Error/Warning.
        # Their native automation report reconciles those expectations. Do not
        # defeat that mechanism by rejecting its raw negative-test stimuli.
        if re.search(r'\bLog\w*:\s*Fatal:|\bfatal error[!: ]|\bUnhandled Exception:|\bAssertion failed:', text, re.I):
            raise ValidationError('Fatal/crash output during automation: ' + str(path))
        return
    if re.search(pattern, text, re.I):
        raise ValidationError('Logged engine/compiler errors; inspect ' + str(path))
    if strict_warnings and re.search(warnings, text, re.I):
        raise ValidationError('Candidate warnings require correction; no implicit warning waivers: ' + str(path))


def validate_config(config):
    for key in ('missions', 'maps'):
        values = config.get(key)
        if not isinstance(values, list) or not values or any(not isinstance(v, str) or not re.fullmatch(r'/Game/[A-Za-z0-9_/.]+', v) for v in values):
            raise ValidationError(f'Config {key} must contain real /Game asset paths')
    additional = config.get('additional_assets', [])
    if not isinstance(additional, list) or any(not isinstance(v, str) or not re.fullmatch(r'/[A-Za-z0-9_/.]+', v) for v in additional):
        raise ValidationError('additional_assets must contain real package/object paths')
    route = config.get('route_command')
    if not isinstance(route, list) or not route or any(not isinstance(v, str) or not v for v in route):
        raise ValidationError('Candidate config requires an actual packaged route driver argv in route_command')
    policy = config.get('performance_policy')
    load_checker('Check-PerformanceReport').validate_policy(policy)


def validate_route(report, run_id, source_hash, package_hash):
    if not isinstance(report, dict) or report.get('run_id') != run_id or report.get('source_sha256') != source_hash or report.get('package_sha256') != package_hash:
        raise ValidationError('Packaged route evidence does not identify this run/source/package')
    if report.get('null_rhi') is not False or report.get('status') != 'passed':
        raise ValidationError('Packaged route needs a successful real-RHI execution')
    required = {'M01_Mantle', 'M02_OneDegree'}
    completed = report.get('completed_missions')
    if not isinstance(completed, list) or not required.issubset(completed):
        raise ValidationError('Packaged route did not finish both opening missions')
    for key in ('checkpoint_restored', 'fatal_recovery_completed', 'protagonist_handoff_completed', 'save_reload_completed'):
        if report.get(key) is not True:
            raise ValidationError('Packaged route missing real assertion: ' + key)


def artifact_inventory(root):
    files = sorted(p for p in root.rglob('*') if p.is_file())
    if not files or not any(p.suffix.lower() == '.exe' for p in files):
        raise ValidationError('Packaging did not produce a Windows executable')
    inventory = {}
    for path in files:
        if path.is_symlink():
            raise ValidationError('Packaged artifacts must not be mutable symbolic links')
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        inventory[path.relative_to(root).as_posix()] = digest.hexdigest()
    return inventory


def artifact_hash(root):
    return hashlib.sha256(json.dumps(artifact_inventory(root), sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def verify_package_unchanged(root, original):
    current = artifact_inventory(root)
    for name, digest in original.items():
        if current.get(name) != digest:
            raise ValidationError('Packaged route changed or removed its input artifact: ' + name)
    for name in current.keys() - original.keys():
        path = Path(name)
        if 'Saved' not in path.parts or path.suffix.lower() not in ('.log', '.csv', '.utrace', '.json', '.sav', '.tmp', '.ini', '.upipelinecache'):
            raise ValidationError('Packaged route introduced an untracked package payload: ' + name)


class Run:
    def __init__(self, project, output):
        self.project = project
        self.root = project.parent
        self.id = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ-') + uuid.uuid4().hex[:12]
        self.directory = output / self.id
        self.directory.mkdir(parents=True, exist_ok=False)
        self.manifest = {'schema_version': 1, 'run_id': self.id, 'project': str(project),
                         'source_sha256': fingerprint(self.root), 'status': 'running', 'stages': [],
                         'required_validator_coverage': required_validator_coverage()}
        self.save()

    def save(self):
        target = self.directory / 'summary.json'
        temporary = target.with_suffix('.tmp')
        temporary.write_text(json.dumps(self.manifest, indent=2) + '\n', encoding='utf-8')
        temporary.replace(target)

    def stage(self, name, argv, timeout, strict=False, report_authority=False):
        record = {'name': name, 'status': 'running', 'argv': argv, 'log': str(self.directory / (name + '.log'))}
        self.manifest['stages'].append(record)
        self.save()
        print('Starting ' + name + '; log: ' + record['log'], flush=True)
        try:
            record.update(run_process(argv, self.root, Path(record['log']), timeout))
            if record['exit_code']:
                raise ValidationError(f'{name} exited {record["exit_code"]}')
            stage_log_errors(Path(record['log']), strict, report_authority)
            record['status'] = 'passed'
        except Exception as error:
            record['status'] = 'failed'
            record['error'] = str(error)
            raise
        finally:
            self.save()

    def check(self, name, validate):
        record = {'name': name, 'status': 'running'}
        self.manifest['stages'].append(record)
        self.save()
        try:
            result = validate()
            record['status'] = 'passed'
            return result
        except Exception as error:
            record.update(status='failed', error=str(error))
            raise
        finally:
            self.save()


def execute(args):
    project = args.project.resolve()
    if not project.is_file() or project.name != 'ProjectVelkorran.uproject':
        raise ValidationError('Provide ProjectVelkorran.uproject')
    run = Run(project, (args.output or project.parent / 'Saved' / 'ValidationRuns').resolve())
    try:
        candidate = args.mode == 'candidate'
        if args.build_only and args.skip_build:
            raise ValidationError('--build-only and --skip-build cannot be combined')
        if candidate and (args.skip_build or args.build_only or args.filter != 'ProjectVelkorran'):
            raise ValidationError('Candidate mode requires full build/stages/test inventory')
        if not engine_host_supported():
            raise ValidationError('Engine execution requires Windows; host tests run independently')
        config = json.loads(args.config.read_text(encoding='utf-8-sig')) if args.config else None
        config_hash = hashlib.sha256(args.config.read_bytes()).hexdigest() if args.config else None
        if candidate:
            validate_config(config or {})
        root = args.engine_root.resolve()
        version = json.loads((root / 'Engine/Build/Build.version').read_text(encoding='utf-8-sig'))
        if version.get('MajorVersion') != 5 or version.get('MinorVersion') != 7:
            raise ValidationError('UE 5.7 is required')
        descriptor = json.loads(project.read_text(encoding='utf-8-sig'))
        if descriptor.get('EngineAssociation') != '5.7':
            raise ValidationError('Project EngineAssociation must be 5.7')
        plugin_roots = [project.parent / 'Plugins', root / 'Engine/Plugins']
        plugin_roots += [(project.parent / p).resolve() for p in descriptor.get('AdditionalPluginDirectories', [])]
        plugins = {}
        for name in ('NarrativePro', 'ZenDyn'):
            found = [p for directory in plugin_roots for p in directory.rglob(name + '.uplugin')]
            if not found:
                raise ValidationError('Required plugin unavailable: ' + name)
            if len(found) > 1:
                raise ValidationError('Ambiguous plugin installations: ' + name)
            plugins[name] = {'descriptor': str(found[0]), 'sha256': hashlib.sha256(found[0].read_bytes()).hexdigest(),
                             'implementation_sha256': fingerprint(found[0].parent),
                             'version': json.loads(found[0].read_text(encoding='utf-8-sig')).get('VersionName')}
        run.manifest.update(mode=args.mode, engine_root=str(root), engine_version=version, plugins=plugins, config_sha256=config_hash,
                            platform='Win64', configuration='Shipping' if candidate else 'Development')
        run.save()
        build = str(root / 'Engine/Build/BatchFiles/Build.bat')
        editor = str(root / 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
        if not args.skip_build:
            for name, target, configuration, extra in (
                ('editor_unity', 'ProjectVelkorranEditor', 'Development', ['-ForceUnity']),
                ('editor_nonunity', 'ProjectVelkorranEditor', 'Development', ['-DisableUnity']),
                ('game_development', 'ProjectVelkorran', 'Development', []),
                ('game_shipping', 'ProjectVelkorran', 'Shipping', [])):
                if candidate:
                    run.stage(name + '_clean', [build, target, 'Win64', configuration, '-Project=' + str(project), '-WaitMutex', '-Clean'], args.build_timeout, candidate)
                run.stage(name, [build, target, 'Win64', configuration, '-Project=' + str(project), '-WaitMutex', '-NoHotReloadFromIDE', '-NoUBTMakefiles'] + extra, args.build_timeout, candidate)
        else:
            run.manifest['stages'].append({'name': 'build', 'status': 'skipped', 'reason': 'diagnostic --skip-build'})
        if not args.build_only:
            report_directory = run.directory / 'Automation'
            base = [editor, str(project), '-unattended', '-nop4', '-nosplash', '-stdout', '-FullStdOutLogOutput', '-NullRHI']
            run.stage('automation', base + ['-ExecCmds=Automation RunTests ' + args.filter, '-TestExit=Automation Test Queue Empty', '-ReportExportPath=' + str(report_directory)], args.automation_timeout, candidate, report_authority=True)
            checker = load_checker('Check-UnrealReport')
            def check_automation():
                coverage = checker.verify(json.loads((report_directory / 'index.json').read_text(encoding='utf-8-sig')), checker.source_tests(project.parent, args.filter), args.filter)
                if candidate and coverage['warnings']:
                    raise ValidationError('Candidate automation has warnings')
                return coverage
            coverage = run.check('automation_report', check_automation)
            run.manifest['automation_coverage'] = coverage
            if config:
                mission_args = ['-run=SovValidateCampaign', '-Missions=' + ','.join(config['missions']), '-ShippingValidation', '-ValidateWorlds']
                if candidate:
                    mission_args.append('-RequireCompleteCoverage')
                if config.get('additional_assets'):
                    mission_args += ['-AdditionalAssets=' + ','.join(config['additional_assets'])]
                run.stage('campaign_worlds', base + mission_args, args.automation_timeout, candidate)
                run.stage('blueprints', base + ['-run=CompileAllBlueprints'], args.build_timeout, candidate)
                run.stage('asset_validation', base + ['-run=DataValidation'], args.build_timeout, candidate)
                package = run.directory / 'Package'
                run.stage('cook_package', [str(root / 'Engine/Build/BatchFiles/RunUAT.bat'), 'BuildCookRun', '-project=' + str(project), '-nop4', '-platform=Win64', '-clientconfig=Shipping', '-build', '-cook', '-stage', '-pak', '-archive', '-archivedirectory=' + str(package), '-map=' + '+'.join(config['maps'])], args.build_timeout, candidate)
                package_hash = artifact_hash(package)
                package_inventory = artifact_inventory(package)
                run.manifest['package_sha256'] = package_hash
                run.manifest['package_files'] = package_inventory
                substitutions = {'run_id': run.id, 'source_sha256': run.manifest['source_sha256'], 'package_sha256': package_hash,
                                 'output': str(run.directory), 'package': str(package), 'project': str(project)}
                route_command = [arg.format_map(substitutions) for arg in config['route_command']]
                run.stage('packaged_route', route_command, args.route_timeout, candidate)
                run.check('package_integrity', lambda: verify_package_unchanged(package, package_inventory))
                run.check('route_assertions', lambda: validate_route(json.loads((run.directory / 'route.json').read_text(encoding='utf-8-sig')), run.id, run.manifest['source_sha256'], package_hash))
                performance = load_checker('Check-PerformanceReport')
                run.manifest['performance'] = run.check('performance', lambda: performance.verify(run.directory / 'performance.json', config['performance_policy'], run.id, run.manifest['source_sha256'], package_hash))
            else:
                for stage in ('campaign_worlds', 'blueprints', 'asset_validation', 'cook_package', 'packaged_route', 'performance'):
                    run.manifest['stages'].append({'name': stage, 'status': 'skipped', 'reason': 'No authored campaign/route config; diagnostic run only'})
        if fingerprint(project.parent) != run.manifest['source_sha256']:
            raise ValidationError('Source/config/scripts changed during validation; discard this candidate')
        if args.config and hashlib.sha256(args.config.read_bytes()).hexdigest() != config_hash:
            raise ValidationError('Route/performance policy changed during validation')
        for plugin in plugins.values():
            if fingerprint(Path(plugin['descriptor']).parent) != plugin['implementation_sha256']:
                raise ValidationError('Plugin source/config/content changed during validation')
        coverage = run.manifest['required_validator_coverage']
        if not coverage['complete']:
            run.manifest['stages'].append({'name': 'required_validator_coverage',
                'status': 'blocked' if candidate else 'incomplete',
                'categories': [item['id'] for item in coverage['categories']],
                'reason': 'Required native validator categories remain unimplemented or partial.'})
            if candidate:
                run.manifest['status'] = 'candidate_blocked'
                run.manifest['error'] = 'Implemented stages passed, but incomplete required validator categories prevent qualification.'
                print(run.manifest['error'], file=sys.stderr)
                return 2
        run.manifest['status'] = 'candidate_passed' if candidate else 'diagnostic_passed'
        return 0
    except Exception as error:
        run.manifest['status'] = 'failed'
        run.manifest['error'] = str(error)
        print(str(error), file=sys.stderr)
        return 2
    finally:
        run.save()
        print('Validation manifest: ' + str(run.directory / 'summary.json'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine-root', type=Path, required=True)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1] / 'ProjectVelkorran.uproject')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--mode', choices=('diagnostic', 'candidate'), default='diagnostic')
    parser.add_argument('--config', type=Path)
    parser.add_argument('--filter', default='ProjectVelkorran')
    parser.add_argument('--automation-timeout', type=int, default=1200)
    parser.add_argument('--build-timeout', type=int, default=7200)
    parser.add_argument('--route-timeout', type=int, default=7200)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--skip-build', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'ProjectVelkorran(?:\.[A-Za-z0-9_]+)*', args.filter):
        parser.error('Unsafe/invalid test filter')
    if min(args.automation_timeout, args.build_timeout, args.route_timeout) < 30:
        parser.error('Timeouts must be at least 30 seconds')
    try:
        return execute(args)
    except (ValidationError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
