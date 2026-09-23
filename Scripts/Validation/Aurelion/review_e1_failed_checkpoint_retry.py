"""Publicly load an earned failed E1 checkpoint and use its physical retry hold."""
import hashlib
import json
import os
import shutil
import sys
import time
import traceback
from pathlib import Path

import unreal


project = Path(unreal.Paths.project_dir())
sys.path.insert(0, str(project/'Scripts/Validation/Aurelion'))
from aurelion_retry_input import RetryInput

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = Path(os.environ['SOV_AURELION_E1_FAILED_SOURCE'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.SovGameUserSettings.get_game_user_settings().complete_accessibility_setup()
report = dict(status='running', source=str(source), banks=[], callbacks=[], scope=__doc__)
state = dict(phase='bootstrap', started=time.monotonic(), busy=False)


def write():
    (out/'e1-failed-retry.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


def loaded(result, header, message):
    report['callbacks'].append(dict(result=str(result), header=header.export_text(), message=str(message)))
    write()


def optional(call):
    try:
        return call()
    except Exception as error:
        return 'unavailable: '+str(error)


def finish(error=None):
    report.update(status='failed' if error else 'passed', error=error)
    if state.get('retry'):
        state['retry'].stop()
        state['retry'] = None
    if state.get('delegate'):
        state['delegate'].remove_callable(loaded)
        state['delegate'] = None
    write()
    level.editor_request_end_play()
    state['phase'] = 'stopping'


def tick(_delta):
    if state['busy']:
        return
    state['busy'] = True
    try:
        now = time.monotonic()
        if state['phase'] == 'stopping':
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert now-state['started'] < 220, 'Earned E1 retry exceeded its deadline'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not isinstance(pc, unreal.SovPlayerController) or not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():
            return
        if state['phase'] == 'bootstrap':
            instance = unreal.GameplayStatics.get_game_instance(world)
            saves = next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == instance)
            if saves.is_load_pending():
                return
            banks = sorted((source/'UserData/Saved/SaveGames').glob('*_2_0_*.sav'))
            assert len(banks) == 2, 'Expected the earned E1 checkpoint bank pair'
            target_dir = (out/'UserData/Saved/SaveGames').resolve()
            assert target_dir.is_relative_to(out.resolve())
            target_dir.mkdir(parents=True, exist_ok=True)
            for bank in banks:
                target = target_dir/bank.name
                digest = hashlib.sha256(bank.read_bytes()).hexdigest()
                shutil.copy2(bank, target)
                assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
                report['banks'].append(dict(name=bank.name, sha256=digest))
            state.update(saves=saves, old_world=hash(world), delegate=saves.on_load_completed, phase='load')
            state['delegate'].add_callable(loaded)
            result, message = saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
            assert result == unreal.SovSaveResult.LOAD_STARTED, str(message)
            write()
            return
        if state['phase'] == 'load':
            if hash(world) == state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:
                return
            assert 'SUCCESS' in report['callbacks'][-1]['result'], report['callbacks']
            directors = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector)
                         if str(actor.encounter_id) == 'M12_E1_PressureHall']
            assert len(directors) == 1
            director = directors[0]
            report['restored_state'] = str(director.get_encounter_state())
            assert director.get_encounter_state() == unreal.SovEncounterState.FAILED
            state.update(retry=RetryInput(world, director, out/'AuthoredRetry'),
                phase='input_probe', probe_frames=0,
                initial_yaw=pc.get_control_rotation().yaw)
            terminal = state['retry'].actor
            report['restored_retry'] = dict(player=pawn.get_actor_location().export_text(),
                terminal=terminal.get_actor_location().export_text(),
                terminal_path=terminal.get_path_name(),
                interaction_range=terminal.interactable.get_editor_property('interaction_distance'),
                interaction_time=terminal.interactable.get_editor_property('interaction_time'),
                last_result=optional(lambda: str(terminal.get_editor_property('last_result'))),
                move_ignored=pc.is_move_input_ignored(), look_ignored=pc.is_look_input_ignored(),
                interact_keys=[key.export_text() for key in state['retry'].driver.owner.query_keys_mapped_to_action(
                    state['retry'].driver.actions['IA_Interact'])],
                move_keys=[key.export_text() for key in state['retry'].driver.owner.query_keys_mapped_to_action(
                    state['retry'].driver.actions['IA_Move'])],
                controller_input=optional(lambda: pc.get_editor_property('input_component').get_path_name()),
                controller_input_enabled=optional(lambda: pc.is_input_enabled()),
                owned_character=optional(lambda: pc.get_editor_property('owned_character').get_path_name()),
                controlled_character=pawn.get_path_name(),
                input_diagnostic=optional(lambda: pc.get_gameplay_input_diagnostic()),
                mapping=optional(lambda: pc.get_editor_property('default_mapping_context').get_path_name()),
                cinematic=optional(lambda: pc.get_editor_property('cinematic_mode')),
                paused=unreal.GameplayStatics.is_game_paused(world),
                transition=str(pc.get_campaign_transition_state()))
            unreal.SystemLibrary.execute_console_command(world,
                'Shot showui -nosuffix filename='+str(out/'failed-retry-before.png'))
            write()
            return
        if state['phase'] == 'input_probe':
            if state['probe_frames'] < 15:
                state['probe_frames'] += 1
                state['retry'].driver.inject(look=(2., 0.))
                return
            state['retry'].driver.inject()
            report['look_input_probe'] = dict(initial_yaw=state['initial_yaw'],
                final_yaw=pc.get_control_rotation().yaw,
                input_frames=state['retry'].driver.report['input_frames'].get('IA_Look', 0))
            if abs(report['look_input_probe']['final_yaw']-state['initial_yaw']) < .1:
                finish('Fifteen ordinary IA_Look frames did not turn the restored controller')
                return
            state['phase'] = 'retry'
            write()
            return
        if state['phase'] == 'retry':
            retry = state['retry']
            if retry.hold_started is not None and now-state.get('last_hold_sample', 0) >= .2:
                state['last_hold_sample'] = now
                component = pc.get_interaction_component()
                report.setdefault('hold_samples', []).append(dict(
                    elapsed=now-state['started'],
                    held=optional(lambda: component.get_editor_property('b_interact_held')),
                    remaining=component.get_editor_property('remaining_interact_time'),
                    viewed=optional(lambda: component.get_editor_property('viewed_interactable').get_path_name()),
                    pending=retry.actor.is_request_pending(),
                    last_result=optional(lambda: str(retry.actor.get_editor_property('last_result')))))
            if now-state.get('last_retry_report', 0) >= 1:
                state['last_retry_report'] = now
                report['retry_progress'] = dict(elapsed=now-state['started'],
                    player=pawn.get_actor_location().export_text(),
                    phase=retry.driver.phase, waypoints=retry.driver.waypoints,
                    path=retry.driver.report.get('last_route_path'),
                    last_interaction=retry.driver.report.get('last_interaction'),
                    samples=retry.samples[-4:],
                    interact_frames=retry.driver.report['input_frames'].get('IA_Interact', 0),
                    hold_started=retry.hold_started is not None,
                    request_pending=retry.actor.is_request_pending(),
                    viewed=pc.get_interaction_component().get_editor_property('viewed_interactable').get_path_name()
                        if pc.get_interaction_component().get_editor_property('viewed_interactable') else None,
                    remaining=pc.get_interaction_component().get_editor_property('remaining_interact_time'),
                    input_diagnostic=optional(lambda: pc.get_gameplay_input_diagnostic()),
                    move_ignored=pc.is_move_input_ignored(), look_ignored=pc.is_look_input_ignored(),
                    tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(pawn).export_text())
                write()
            if retry.step(world, pc, pawn):
                report.update(retry_samples=retry.samples,
                    input_frames=retry.driver.report['input_frames'].copy(),
                    new_attempt=retry.director.get_attempt_id().export_text())
                retry.stop()
                state['retry'] = None
                state.update(phase='settle', at=now)
                write()
            return
        assert state['phase'] == 'settle'
        assert pawn.is_alive() and pawn.is_character_ready()
        assert now-state['at'] < 10
        if now-state['at'] >= 4:
            report['ready_seconds'] = now-state['at']
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        state['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
write()
level.editor_request_begin_play()
