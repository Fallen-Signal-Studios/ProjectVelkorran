"""Repeated fresh M12 starts and public CP0 checkpoint reloads in one isolated editor process.

Run through run-editor-script.ps1 in its own fresh profile. Each cycle starts real PIE on the M12
wrapper, waits for a ready living protagonist in the active mission with a CP0 checkpoint newer than
the previous cycle's, requests exactly one public SovSaveSubsystem.load_slot(CHECKPOINT, 0), requires
one successful native completion callback, a different world object and an unchanged journal with a
ready living protagonist for STABLE_SECONDS, then ends PIE. No input, resource, damage, transform or
story state is written. SOV_SOAK_CYCLES sets the cycle count; SOV_SOAK_MAX_FPS optionally applies
t.MaxFPS for a frame profile. Frame caps do not qualify target-hardware performance.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

MAP = '/Game/Aurelion/Maps/L_Aurelion_M12'
MISSION = 'M12_FireAndFrost'
BOUNDARY = 'Aurelion.CP0'
READY_SECONDS = 150.
LOAD_SECONDS = 150.
STOP_SECONDS = 30.
STABLE_SECONDS = 3.
OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
CYCLES = max(1, int(os.environ.get('SOV_SOAK_CYCLES', '5')))
MAX_FPS = os.environ.get('SOV_SOAK_MAX_FPS')

report = dict(status='running', scope='Fresh M12 PIE starts plus public CP0 checkpoint reloads; one editor process',
              method='Real PIE begin/end and SovSaveSubsystem.load_slot only; no gameplay input or state writes',
              requested_cycles=CYCLES, max_fps=MAX_FPS, cycles=[], physical_platform_qualification=False)
ctl = dict(handle=None, phase='begin', phase_at=time.monotonic(), cycle=None, last_generation=-1,
           saves=None, delegate=None, callback=None, world_hash=None, journal=None, stable_since=None, written=0.)


def write():
    report['completed_cycles'] = sum(1 for c in report['cycles'] if c.get('status') in ('passed', 'failed'))
    report['passed_cycles'] = sum(1 for c in report['cycles'] if c.get('status') == 'passed')
    starts = [c for c in report['cycles'] if c.get('ready_seconds') is not None]
    loads = [c for c in report['cycles'] if c.get('load_requested')]
    report['traversal_starts_ready'] = len(starts)
    report['reloads_requested'] = len(loads)
    report['reloads_succeeded'] = sum(1 for c in loads if c.get('status') == 'passed')
    report['reload_success_rate'] = report['reloads_succeeded'] / len(loads) if loads else None
    temp = OUT / 'checkpoint-soak.tmp'
    temp.write_text(json.dumps(report, indent=2, default=str), encoding='utf8')
    os.replace(temp, OUT / 'checkpoint-soak.json')


def stage(name):
    ctl['phase'] = name
    ctl['phase_at'] = time.monotonic()


def unbind():
    if ctl['delegate'] is not None:
        try:
            ctl['delegate'].remove_callable(ctl['callback'])
        except Exception:
            pass
    ctl['delegate'] = ctl['callback'] = ctl['saves'] = None


def fail_cycle(reason):
    unbind()
    ctl['cycle'].update(status='failed', reason=reason, elapsed=time.monotonic()-ctl['cycle']['started_monotonic'])
    write()
    stage('end_play')


def finish(reason):
    unbind()
    report['status'] = 'completed' if report['completed_cycles'] == CYCLES else 'failed'
    report['reason'] = reason
    report['qualification'] = ('Counts actual fresh starts and native CP0 reloads in this process only. '
                               'It is not a physical-input, packaged, target-hardware or long-session stability pass.')
    write()
    if ctl['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(ctl['handle'])
        ctl['handle'] = None
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def journal(state):
    return [str(e.beat_id) for e in state.get_journal()]


def player(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not isinstance(pc, unreal.SovPlayerController) or not isinstance(pawn, unreal.SovPlayerCharacterBase):
        return None, None, None
    state = pc.get_campaign_state()
    return pc, pawn, state


def ready(pawn, state):
    mission = state.get_active_mission() if state else None
    return (mission is not None and str(mission.mission_id) == MISSION and state.is_state_valid()
            and pawn.is_character_ready() and pawn.is_alive() and pawn.get_health() > 0.
            and not pawn.is_character_pending_load())


def tick(_delta):
    try:
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        now = time.monotonic()
        phase = ctl['phase']
        cycle = ctl['cycle']
        if now-ctl['written'] > 2.:
            ctl['written'] = now
            write()
        if phase == 'begin':
            if len(report['cycles']) >= CYCLES:
                finish('Requested cycles completed')
                return
            if editor.is_in_play_in_editor():
                return
            ctl['cycle'] = dict(index=len(report['cycles']), status='running', started_monotonic=now)
            report['cycles'].append(ctl['cycle'])
            ctl['world_hash'] = ctl['journal'] = ctl['stable_since'] = None
            editor.editor_request_begin_play()
            stage('await_ready')
            return
        if phase == 'end_play':
            if editor.is_in_play_in_editor():
                editor.editor_request_end_play()
            stage('await_stopped')
            return
        if phase == 'await_stopped':
            if not editor.is_in_play_in_editor():
                stage('begin')
            elif now-ctl['phase_at'] > STOP_SECONDS:
                finish('PIE did not stop within the bounded window')
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if phase == 'await_ready':
            if now-ctl['phase_at'] > READY_SECONDS:
                fail_cycle('Fresh start did not reach a ready mission with a new CP0 checkpoint')
                return
            if world is None or 'L_Aurelion_M12' not in world.get_name():
                return
            if MAX_FPS and not cycle.get('max_fps_applied'):
                unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS ' + str(int(MAX_FPS)))
                cycle['max_fps_applied'] = True
            pc, pawn, state = player(world)
            if pawn is None or not ready(pawn, state):
                return
            instance = unreal.GameplayStatics.get_game_instance(world)
            owners = [s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == instance]
            if len(owners) != 1 or owners[0].is_load_pending() or owners[0].is_awaiting_failure_decision():
                return
            headers = [h for h in owners[0].list_slots()
                       if h.kind == unreal.SovSaveSlotKind.CHECKPOINT and h.slot_index == 0 and str(h.boundary_id) == BOUNDARY]
            if len(headers) != 1 or int(headers[0].generation) <= ctl['last_generation']:
                return
            ctl['last_generation'] = int(headers[0].generation)
            cycle.update(ready_seconds=now-cycle['started_monotonic'], checkpoint_generation=int(headers[0].generation),
                         protagonist=pawn.get_class().get_path_name(), journal=journal(state))
            ctl['saves'], ctl['world_hash'], ctl['journal'] = owners[0], hash(world), journal(state)
            def completed(result, saved, message):
                cycle.setdefault('callbacks', []).append(dict(result=str(result), success=result == unreal.SovSaveResult.SUCCESS,
                    boundary=str(saved.boundary_id), generation=int(saved.generation), message=str(message),
                    elapsed=time.monotonic()-cycle['started_monotonic']))
            ctl['callback'], ctl['delegate'] = completed, owners[0].on_load_completed
            ctl['delegate'].add_callable(completed)
            stage('request_load')
            return
        if phase == 'request_load':
            returned, message = ctl['saves'].load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
            cycle.update(load_requested=True, request_result=str(returned), request_message=str(message),
                         load_requested_seconds=now-cycle['started_monotonic'])
            if returned != unreal.SovSaveResult.LOAD_STARTED:
                fail_cycle('Public checkpoint load was not started: ' + str(message))
                return
            stage('await_load')
            return
        if phase == 'await_load':
            if now-ctl['phase_at'] > LOAD_SECONDS:
                fail_cycle('Checkpoint load did not complete and settle within the bounded window')
                return
            callbacks = cycle.get('callbacks', [])
            if len(callbacks) > 1:
                fail_cycle('More than one native load completion callback')
                return
            if callbacks and not callbacks[0]['success']:
                fail_cycle('Native load failed: ' + callbacks[0]['message'])
                return
            if not callbacks or ctl['saves'].is_load_pending() or world is None:
                return
            # A fresh start can land a newer CP0 for the same boundary between the header read and the load.
            # Loading that newer same-boundary checkpoint is correct; an older or different one is not.
            if callbacks[0]['boundary'] != BOUNDARY or callbacks[0]['generation'] < cycle['checkpoint_generation']:
                fail_cycle('Native callback reported a different or older checkpoint')
                return
            cycle['callback_generation_advanced'] = callbacks[0]['generation'] > cycle['checkpoint_generation']
            ctl['last_generation'] = max(ctl['last_generation'], callbacks[0]['generation'])
            if hash(world) == ctl['world_hash']:
                return
            pc, pawn, state = player(world)
            if pawn is None or not ready(pawn, state):
                ctl['stable_since'] = None
                return
            if journal(state) != ctl['journal']:
                fail_cycle('Reload changed the journal')
                return
            ctl['stable_since'] = ctl['stable_since'] or now
            if now-ctl['stable_since'] < STABLE_SECONDS:
                return
            unbind()
            cycle.update(status='passed', load_seconds=now-cycle['started_monotonic']-cycle['load_requested_seconds'],
                         elapsed=now-cycle['started_monotonic'])
            write()
            stage('end_play')
    except Exception:
        report['error'] = traceback.format_exc()
        if ctl['cycle'] is not None and ctl['cycle'].get('status') == 'running':
            fail_cycle('Observer exception: ' + report['error'].strip().splitlines()[-1])
        else:
            finish('Observer exception before a cycle started')


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
try:
    settings = unreal.GameUserSettings.get_game_user_settings()
    assert isinstance(settings, unreal.SovGameUserSettings)
    assert settings.complete_accessibility_setup(), 'Cannot accept unchanged standard settings in isolated profile'
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert not editor.is_in_play_in_editor()
    assert editor.load_level(MAP)
    assert editor.get_viewport_config_keys(), 'No real PIE viewport'
    write()
    ctl['handle'] = unreal.register_slate_post_tick_callback(tick)
except Exception:
    report['error'] = traceback.format_exc()
    finish('Startup failed')
