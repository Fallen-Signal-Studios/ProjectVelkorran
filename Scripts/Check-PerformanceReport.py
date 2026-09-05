#!/usr/bin/env python3
"""Validate real route capture samples against explicit, reviewed performance policy.

Input is a normalized CSV exported by the route capture driver, not diagnostic
combat events. No measurements are synthesized and no budget is silently chosen.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import sys


class PerformanceError(ValueError):
    pass


def number(value, name, minimum=0):
    if isinstance(value, bool):
        raise PerformanceError(name + ' must be a finite number')
    try:
        result = float(value)
    except (TypeError, ValueError):
        raise PerformanceError(name + ' must be a finite number') from None
    if not math.isfinite(result) or result < minimum:
        raise PerformanceError(name + ' is outside the permitted range')
    return result


def integer(value, name, minimum=0):
    result = number(value, name, minimum)
    if not result.is_integer():
        raise PerformanceError(name + ' must be an integer')
    return int(result)


def validate_policy(policy):
    if not isinstance(policy, dict) or not isinstance(policy.get('reviewed_by'), str) or not policy['reviewed_by'].strip():
        raise PerformanceError('Performance policy needs an accountable reviewed_by')
    for key in ('target_fps', 'minimum_frames', 'minimum_combat_frames', 'minimum_frames_per_reload', 'sustained_spike_frames', 'minimum_promotion_events',
                'maximum_reload_growth_bytes', 'maximum_promotion_latency_ms'):
        if type(policy.get(key)) not in (int, float):
            raise PerformanceError('Performance policy must use numeric values: ' + key)
    if number(policy.get('target_fps'), 'target_fps', 1) not in (30, 60, 120):
        raise PerformanceError('target_fps must identify the actual 30/60/120 mode')
    for key in ('minimum_frames', 'minimum_combat_frames', 'minimum_frames_per_reload', 'sustained_spike_frames', 'minimum_promotion_events'):
        integer(policy.get(key), key, 1)
    for key in ('maximum_reload_growth_bytes', 'maximum_promotion_latency_ms'):
        number(policy.get(key), key)
    return policy


def verify(report_path, policy, run_id, source_hash, package_hash):
    validate_policy(policy)
    report = json.loads(report_path.read_text(encoding='utf-8-sig'))
    if not isinstance(report, dict):
        raise PerformanceError('Capture manifest must be an object')
    for key, expected in (('run_id', run_id), ('source_sha256', source_hash), ('package_sha256', package_hash)):
        if report.get(key) != expected:
            raise PerformanceError('Capture identity mismatch: ' + key)
    for key in ('platform', 'device', 'rendering_mode', 'driver', 'capture_tool'):
        if not isinstance(report.get(key), str) or not report[key].strip():
            raise PerformanceError('Capture metadata missing: ' + key)
    if report.get('null_rhi') is not False or report.get('cold_cache') is not True:
        raise PerformanceError('Qualification requires a real-RHI cold-cache capture')
    if report.get('target_fps') != policy['target_fps']:
        raise PerformanceError('Capture FPS does not match the reviewed mode')
    relative = Path(report.get('samples_csv', ''))
    if relative.is_absolute() or '..' in relative.parts or not relative.name:
        raise PerformanceError('Capture CSV must be a relative file in this fresh run')
    path = report_path.parent / relative
    if not path.resolve().is_relative_to(report_path.parent.resolve()):
        raise PerformanceError('Capture CSV escapes the run directory')
    if hashlib.sha256(path.read_bytes()).hexdigest() != report.get('samples_sha256'):
        raise PerformanceError('Capture CSV hash mismatch')
    with path.open(encoding='utf-8-sig', newline='') as stream:
        reader = csv.DictReader(stream)
        required = {'frame', 'frame_ms', 'memory_bytes', 'reload_index', 'combat', 'pso_pending', 'promotion_latency_ms', 'promotion_events'}
        if not required.issubset(reader.fieldnames or []):
            raise PerformanceError('Capture is missing required raw sample columns')
        rows = list(reader)
    if len(rows) < policy['minimum_frames']:
        raise PerformanceError('Capture is too short for the reviewed workload')
    times, combat_times, reload_memory = [], [], {index: [] for index in range(4)}
    spike_run = longest_spike = combat_frames = 0
    previous_frame, previous_reload = None, 0
    worst_promotion = 0.0
    promotion_events = 0
    for row in rows:
        frame = integer(row['frame'], 'frame')
        if previous_frame is not None and frame != previous_frame + 1:
            raise PerformanceError('Frames are missing, duplicated or reordered')
        previous_frame = frame
        duration = number(row['frame_ms'], 'frame_ms', 0.000001)
        memory = integer(row['memory_bytes'], 'memory_bytes', 1)
        reload_index = integer(row['reload_index'], 'reload_index')
        if reload_index not in reload_memory or reload_index < previous_reload or reload_index > previous_reload + 1:
            raise PerformanceError('Reload capture must progress monotonically through baseline and three reloads')
        previous_reload = reload_index
        if row['combat'] not in ('0', '1'):
            raise PerformanceError('combat must be 0 or 1')
        combat = row['combat'] == '1'
        pending = integer(row['pso_pending'], 'pso_pending')
        promotion = number(row['promotion_latency_ms'], 'promotion_latency_ms')
        events = integer(row['promotion_events'], 'promotion_events')
        if not events and promotion:
            raise PerformanceError('Promotion latency sample has no corresponding completed event')
        promotion_events += events
        times.append(duration)
        reload_memory[reload_index].append(memory)
        combat_frames += int(combat)
        if combat:
            combat_times.append(duration)
        spike_run = spike_run + 1 if combat and duration > 50 else 0
        longest_spike = max(longest_spike, spike_run)
        worst_promotion = max(worst_promotion, promotion)
        if combat and pending:
            raise PerformanceError('Combat entered before required shader/PSO work completed')
    if not combat_frames:
        raise PerformanceError('Capture contains no combat workload')
    if combat_frames < policy['minimum_combat_frames']:
        raise PerformanceError('Capture lacks the reviewed combat workload')
    if promotion_events < policy['minimum_promotion_events']:
        raise PerformanceError('Capture lacks the reviewed promotion workload')
    for index, samples in reload_memory.items():
        if len(samples) < policy['minimum_frames_per_reload']:
            raise PerformanceError(f'Reload {index} has insufficient settled samples')
    frame_budget = 1000.0 / policy['target_fps']
    within = sum(value <= frame_budget for value in times)
    if within * 100 < len(times) * 99:
        raise PerformanceError('Fewer than 99% of measured frames meet the target frame budget')
    combat_within = sum(value <= frame_budget for value in combat_times)
    if combat_within * 100 < combat_frames * 99:
        raise PerformanceError('Fewer than 99% of combat frames meet the target frame budget')
    if longest_spike >= policy['sustained_spike_frames']:
        raise PerformanceError('Sustained combat frame spike above 50 ms')
    # Compare worst residency in each complete settled window; route driver owns
    # consistent settling intervals. Loading peaks belong in separate raw traces.
    baseline = max(reload_memory[0])
    growth = max(max(values) - baseline for index, values in reload_memory.items() if index)
    if growth > policy['maximum_reload_growth_bytes']:
        raise PerformanceError('Three-reload memory growth exceeds the reviewed tolerance')
    if worst_promotion > policy['maximum_promotion_latency_ms']:
        raise PerformanceError('Promotion latency exceeds the reviewed warning/attacker timing budget')
    ordered = sorted(times)
    return {'status': 'passed', 'frames': len(times), 'combat_frames': combat_frames,
            'p99_frame_ms': ordered[math.ceil(len(ordered) * .99) - 1],
            'fraction_within_budget': within / len(times), 'maximum_frame_ms': max(times),
            'combat_fraction_within_budget': combat_within / combat_frames,
            'longest_combat_spike_frames': longest_spike, 'maximum_reload_growth_bytes': growth,
            'maximum_promotion_latency_ms': worst_promotion, 'promotion_events': promotion_events, 'platform': report['platform'],
            'device': report['device'], 'rendering_mode': report['rendering_mode'], 'policy': policy}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--policy', type=Path, required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--source-sha256', required=True)
    parser.add_argument('--package-sha256', required=True)
    args = parser.parse_args()
    try:
        result = verify(args.report, json.loads(args.policy.read_text()), args.run_id, args.source_sha256, args.package_sha256)
    except (PerformanceError, OSError, ValueError, TypeError, KeyError) as error:
        print(json.dumps({'status': 'failed', 'error': str(error)}))
        return 2
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
