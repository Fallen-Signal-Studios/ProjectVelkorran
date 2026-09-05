#!/usr/bin/env python3
"""Host regression tests of orchestration and hostile capture evidence, not UE execution."""
import argparse
import copy
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1]

def module(name):
    spec = importlib.util.spec_from_file_location(name, SCRIPTS / (name + '.py'))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

runner = module('Validate-Unreal')
performance = module('Check-PerformanceReport')
POLICY = {'reviewed_by': 'Host test fixture only', 'target_fps': 60, 'minimum_frames': 400, 'minimum_combat_frames': 100,
          'minimum_frames_per_reload': 100, 'sustained_spike_frames': 2,
          'minimum_promotion_events': 1,
          'maximum_reload_growth_bytes': 100, 'maximum_promotion_latency_ms': 10}


def capture(directory, run_id='run', source='source', package='package', mutate=None):
    rows = [{'frame': i, 'frame_ms': 16, 'memory_bytes': 1000, 'reload_index': i // 100,
             'combat': 1, 'pso_pending': 0, 'promotion_latency_ms': 1 if i == 0 else 0,
             'promotion_events': 1 if i == 0 else 0} for i in range(400)]
    if mutate:
        mutate(rows)
    path = directory / 'frames.csv'
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    report = {'run_id': run_id, 'source_sha256': source, 'package_sha256': package,
              'platform': 'Win64', 'device': 'Host fixture - no hardware result', 'rendering_mode': 'test fixture',
              'driver': 'test', 'capture_tool': 'host fixture', 'null_rhi': False, 'cold_cache': True,
              'target_fps': 60, 'samples_csv': 'frames.csv', 'samples_sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    manifest = directory / 'performance.json'
    manifest.write_text(json.dumps(report))
    return manifest


class PerformanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def verify(self, mutate=None, report_mutation=None):
        path = capture(self.root, mutate=mutate)
        if report_mutation:
            report = json.loads(path.read_text())
            report_mutation(report)
            path.write_text(json.dumps(report))
        return performance.verify(path, POLICY, 'run', 'source', 'package')

    def test_valid_measurement_contract(self):
        self.assertEqual(self.verify()['p99_frame_ms'], 16)

    def test_injected_sustained_spike(self):
        with self.assertRaisesRegex(performance.PerformanceError, 'Sustained'):
            self.verify(lambda rows: [rows[i].update(frame_ms=51) for i in (0, 1)])

    def test_p99_budget(self):
        with self.assertRaisesRegex(performance.PerformanceError, '99%'):
            self.verify(lambda rows: [rows[i].update(frame_ms=20) for i in range(5)])

    def test_three_reload_leak(self):
        with self.assertRaisesRegex(performance.PerformanceError, 'memory growth'):
            self.verify(lambda rows: [row.update(memory_bytes=1200) for row in rows[300:]])

    def test_delayed_promotion(self):
        with self.assertRaisesRegex(performance.PerformanceError, 'Promotion'):
            self.verify(lambda rows: rows[0].update(promotion_latency_ms=11))

    def test_psos_pending(self):
        with self.assertRaisesRegex(performance.PerformanceError, 'shader/PSO'):
            self.verify(lambda rows: rows[0].update(pso_pending=1))

    def test_no_promotion_workload_cannot_pass_latency(self):
        with self.assertRaisesRegex(performance.PerformanceError, 'promotion workload'):
            self.verify(lambda rows: rows[0].update(promotion_latency_ms=0, promotion_events=0))

    def test_idle_samples_cannot_hide_unrepresentative_or_slow_combat(self):
        with self.assertRaisesRegex(performance.PerformanceError, 'combat workload'):
            self.verify(lambda rows: [row.update(combat=0) for row in rows[1:]])
        def slow_combat(rows):
            for row in rows[100:]:
                row['combat'] = 0
            for row in rows[:2]:
                row['frame_ms'] = 20
        with self.assertRaisesRegex(performance.PerformanceError, '99% of combat'):
            self.verify(slow_combat)

    def test_missing_duplicate_nonfinite_negative_samples(self):
        for key, value in [('frame', 3), ('frame_ms', 'nan'), ('memory_bytes', -1), ('reload_index', 3), ('combat', 'true')]:
            with self.subTest(key=key), self.assertRaises(performance.PerformanceError):
                self.verify(lambda rows: rows[0].update({key: value}))

    def test_capture_identity_and_no_null_rhi(self):
        for key, value in [('run_id', 'stale'), ('source_sha256', 'stale'), ('package_sha256', 'stale'),
                           ('null_rhi', True), ('cold_cache', False), ('target_fps', 30), ('samples_sha256', 'wrong'),
                           ('samples_csv', '../old.csv'), ('device', '')]:
            with self.subTest(key=key), self.assertRaises(performance.PerformanceError):
                self.verify(report_mutation=lambda report: report.update({key: value}))

    def test_no_combat_and_insufficient_reloads(self):
        with self.assertRaisesRegex(performance.PerformanceError, 'no combat'):
            self.verify(lambda rows: [row.update(combat=0) for row in rows])
        with self.assertRaisesRegex(performance.PerformanceError, 'insufficient'):
            self.verify(lambda rows: [row.update(reload_index=0) for row in rows])

    def test_reviewed_policy_is_required(self):
        for key in POLICY:
            policy = dict(POLICY)
            del policy[key]
            with self.subTest(key=key), self.assertRaises(performance.PerformanceError):
                performance.validate_policy(policy)


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project = self.root / 'ProjectVelkorran.uproject'
        self.project.write_text(json.dumps({'EngineAssociation': '5.7'}))
        native = self.root / 'Source/Tests.cpp'
        native.parent.mkdir()
        native.write_text('IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest, "ProjectVelkorran.Fixture", flags)')
        self.engine = self.root / 'EngineRoot'
        version = self.engine / 'Engine/Build/Build.version'
        version.parent.mkdir(parents=True)
        version.write_text(json.dumps({'MajorVersion': 5, 'MinorVersion': 7, 'Changelist': 123}))
        for name in ('NarrativePro', 'ZenDyn'):
            plugin = self.root / 'Plugins' / name / (name + '.uplugin')
            plugin.parent.mkdir(parents=True)
            plugin.write_text('{}')
        self.config = self.root / 'route-config.json'
        self.config.write_text(json.dumps({'missions': ['/Game/M01.M01', '/Game/M02.M02'],
            'maps': ['/Game/M01', '/Game/M02'], 'route_command': ['HOST_TEST_ROUTE', '{run_id}', '{source_sha256}', '{package_sha256}', '{output}'],
            'performance_policy': POLICY}))
        self.args = argparse.Namespace(project=self.project, output=self.root / 'Runs', mode='candidate',
            skip_build=False, build_only=False, filter='ProjectVelkorran', config=self.config,
            engine_root=self.engine, build_timeout=30, automation_timeout=30, route_timeout=30)
        self.fail_stage = None
        self.empty_report = False
        self.change_source = False
        self.expected_automation_error = False

    def fake_process(self, argv, cwd, log, timeout):
        """Mock only engine process I/O to test the runner's aggregation decisions."""
        log.write_text('Host orchestration fixture; this is not an Unreal result.\n')
        if log.stem == self.fail_stage:
            return {'exit_code': 9, 'duration_seconds': .001}
        if any(arg.startswith('-ReportExportPath=') for arg in argv):
            if self.expected_automation_error:
                log.write_text('LogSovMission: Error: [WORLD.ENTRY_START] expected negative-test stimulus\n')
            report = Path(next(arg.split('=', 1)[1] for arg in argv if arg.startswith('-ReportExportPath='))) / 'index.json'
            report.parent.mkdir()
            report.write_text(json.dumps({} if self.empty_report else {'succeeded': 1, 'succeededWithWarnings': 0, 'failed': 0, 'notRun': 0, 'inProcess': 0,
                'tests': [{'fullTestPath': 'ProjectVelkorran.Fixture', 'state': 'Success', 'errors': 0}]}))
        if 'BuildCookRun' in argv:
            package = Path(next(arg.split('=', 1)[1] for arg in argv if arg.startswith('-archivedirectory=')))
            package.mkdir()
            (package / 'Fixture.exe').write_bytes(b'HOST FIXTURE - NOT AN EXECUTABLE')
        if argv[0] == 'HOST_TEST_ROUTE':
            _, run_id, source, package, output = argv
            out = Path(output)
            (out / 'route.json').write_text(json.dumps({'run_id': run_id, 'source_sha256': source, 'package_sha256': package,
                'status': 'passed', 'null_rhi': False, 'completed_missions': ['M01_Mantle', 'M02_OneDegree'],
                'checkpoint_restored': True, 'fatal_recovery_completed': True,
                'protagonist_handoff_completed': True, 'save_reload_completed': True}))
            capture(out, run_id, source, package)
            if self.change_source:
                (self.root / 'Source/Tests.cpp').write_text('changed during run')
        return {'exit_code': 0, 'duration_seconds': .001}

    def execute(self):
        with patch.object(runner, 'engine_host_supported', return_value=True), patch.object(runner, 'run_process', side_effect=self.fake_process):
            return runner.execute(self.args)

    def summary(self):
        path = max(self.args.output.glob('*/summary.json'), key=lambda item: item.stat().st_mtime_ns)
        return json.loads(path.read_text())

    def test_all_implemented_stages_cannot_hide_missing_required_categories(self):
        self.assertEqual(self.execute(), 2)
        manifest = self.summary()
        self.assertEqual(manifest['status'], 'candidate_blocked')
        self.assertFalse(manifest['required_validator_coverage']['complete'])
        self.assertEqual({item['id'] for item in manifest['required_validator_coverage']['categories']},
                         {'status_cleanup_declarations', 'prohibited_runtime_class_closure'})
        stages = {stage['name']: stage for stage in manifest['stages']}
        self.assertTrue({'editor_unity', 'editor_nonunity', 'game_development', 'game_shipping', 'campaign_worlds',
                         'blueprints', 'asset_validation', 'cook_package', 'packaged_route'}.issubset(stages))
        self.assertIn('-DisableUnity', stages['editor_nonunity']['argv'])
        self.assertIn('-ForceUnity', stages['editor_unity']['argv'])
        self.assertIn('-RequireCompleteCoverage', stages['campaign_worlds']['argv'])
        self.assertEqual(stages['required_validator_coverage']['status'], 'blocked')
        self.assertTrue(all(stage['status'] == 'passed' for name, stage in stages.items() if name != 'required_validator_coverage'))

    def test_native_expected_errors_use_report_accounting(self):
        self.expected_automation_error = True
        self.assertEqual(self.execute(), 2)
        self.assertEqual(self.summary()['status'], 'candidate_blocked')
        self.assertEqual(next(stage for stage in self.summary()['stages'] if stage['name'] == 'automation_report')['status'], 'passed')
        log = self.root / 'fatal.log'
        log.write_text('LogWindows: Error: Fatal error!')
        with self.assertRaises(runner.ValidationError):
            runner.stage_log_errors(log, True, report_authority=True)

    def test_each_process_stage_failure_rejects_candidate(self):
        for stage in ('editor_unity_clean', 'editor_unity', 'editor_nonunity', 'game_development', 'game_shipping',
                      'automation', 'campaign_worlds', 'blueprints', 'asset_validation', 'cook_package', 'packaged_route'):
            with self.subTest(stage=stage):
                self.fail_stage = stage
                self.assertEqual(self.execute(), 2)
                self.assertEqual(self.summary()['status'], 'failed')

    def test_stale_build_focused_and_build_only_never_candidate(self):
        for key, value in [('skip_build', True), ('build_only', True), ('filter', 'ProjectVelkorran.Fixture')]:
            with self.subTest(key=key):
                prior = getattr(self.args, key)
                setattr(self.args, key, value)
                self.assertEqual(self.execute(), 2)
                setattr(self.args, key, prior)

    def test_diagnostic_skip_is_explicit(self):
        self.args.mode = 'diagnostic'
        self.args.skip_build = True
        self.args.config = None
        self.assertEqual(self.execute(), 0)
        manifest = self.summary()
        self.assertEqual(manifest['status'], 'diagnostic_passed')
        self.assertTrue(any(stage['status'] == 'skipped' for stage in manifest['stages']))
        self.assertFalse(manifest['required_validator_coverage']['complete'])

    def test_full_diagnostic_execution_keeps_missing_categories_visible(self):
        self.args.mode = 'diagnostic'
        self.assertEqual(self.execute(), 0)
        manifest = self.summary()
        self.assertEqual(manifest['status'], 'diagnostic_passed')
        self.assertFalse(manifest['required_validator_coverage']['complete'])
        stages = {stage['name']: stage for stage in manifest['stages']}
        self.assertEqual(stages['performance']['status'], 'passed')
        self.assertEqual(stages['required_validator_coverage']['status'], 'incomplete')
        self.assertNotIn('-RequireCompleteCoverage', stages['campaign_worlds']['argv'])

    def test_missing_route_config_rejected(self):
        self.args.config = None
        self.assertEqual(self.execute(), 2)

    def test_empty_report_and_mutated_source_rejected(self):
        self.empty_report = True
        self.assertEqual(self.execute(), 2)
        self.empty_report = False
        self.change_source = True
        self.assertEqual(self.execute(), 2)

    def test_unique_run_directories(self):
        self.args.mode = 'diagnostic'
        self.args.build_only = True
        self.args.config = None
        self.assertEqual(self.execute(), 0)
        self.assertEqual(self.execute(), 0)
        self.assertEqual(len(list(self.args.output.glob('*/summary.json'))), 2)

    def test_logged_errors_and_candidate_warnings_fail(self):
        log = self.root / 'errors.log'
        for text in ('LogTemp: Error: broken', 'x.cpp: error C2039: no member', 'LogTemp: Warning: incomplete'):
            log.write_text(text)
            with self.assertRaises(runner.ValidationError):
                runner.stage_log_errors(log, True)
        log.write_text('LogTemp: Warning: diagnostic only')
        runner.stage_log_errors(log, False)

    def test_real_host_process_exit_timeout_and_literal_arguments(self):
        log = self.root / 'child.log'
        result = runner.run_process([sys.executable, '-c', 'import sys; print(sys.argv[1]); sys.exit(7)', 'literal $() & value'], self.root, log, 5)
        self.assertEqual(result['exit_code'], 7)
        self.assertIn('literal $() & value', log.read_text())
        result = runner.run_process([sys.executable, '-c', 'import time; time.sleep(10)'], self.root, log, .1)
        self.assertEqual(result['exit_code'], 124)

    def test_batch_arguments_quote_metacharacters_and_reject_expansion(self):
        command = runner.process_command(['Build.bat', '-Project=C:\\A&B\\Project.uproject', 'Development'], windows=True)
        self.assertIn('"-Project=C:\\A&B\\Project.uproject"', command)
        for value in ('%PATH%', '!PATH!', 'embedded"quote', 'line\nbreak'):
            with self.subTest(value=value), self.assertRaises(runner.ValidationError):
                runner.process_command(['Build.bat', value], windows=True)

    def test_package_inputs_cannot_change_after_fingerprinting(self):
        package = self.root / 'Package'
        package.mkdir()
        executable = package / 'Game.exe'
        executable.write_bytes(b'host fixture original')
        inventory = runner.artifact_inventory(package)
        saved = package / 'Game/Saved/Logs'
        saved.mkdir(parents=True)
        (saved / 'Game.log').write_text('permitted new runtime log')
        runner.verify_package_unchanged(package, inventory)
        executable.write_bytes(b'host fixture replacement')
        with self.assertRaisesRegex(runner.ValidationError, 'changed'):
            runner.verify_package_unchanged(package, inventory)
        executable.write_bytes(b'host fixture original')
        (saved / 'Injected.dll').write_bytes(b'not a log')
        with self.assertRaisesRegex(runner.ValidationError, 'untracked'):
            runner.verify_package_unchanged(package, inventory)


if __name__ == '__main__':
    unittest.main()
