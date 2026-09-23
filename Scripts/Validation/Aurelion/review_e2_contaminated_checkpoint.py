"""Publicly restore an earned E2 checkpoint and observe ordinary enemy behavior.

The passive drone observer is started by the entry wrapper. This script copies
the two unmodified checkpoint banks into an isolated user profile, uses the
public save API and normal retry input, then leaves the player stationary for
30 seconds. It never writes actor combat state or progression directly.
"""
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
sys.path.insert(0, str(project / 'Scripts/Validation/Aurelion'))
from aurelion_retry_input import RetryInput

ENCOUNTER = 'M12_E2_RelayOverlook'

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = Path(os.environ.get('SOV_AURELION_E2_SOURCE', str(project /
    'Saved/Validation/Aurelion/E2EnforcerFacingReplay-20260923-030228-c85d6e38'))).resolve()
prior = json.loads((source / 'E1Continuation/route-follow-on/continue_aurelion_e2_input/'
                    'e2-input-continuation.json').read_text(encoding='utf-8'))
assert prior['status'] == 'passed', 'Source must be an actually earned E2 route'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.SovGameUserSettings.get_game_user_settings().complete_accessibility_setup()
report = dict(status='running', source=str(source), banks=[], callbacks=[],
              scope=__doc__, retry_uses_normal_input=True, passive_samples=[])
state = dict(phase='bootstrap', start=time.monotonic(), busy=False)


def write():
    (out / 'e2-checkpoint-review.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


def loaded(result, header, message):
    report['callbacks'].append(dict(result=str(result), header=header.export_text(), message=str(message)))
    write()


def finish(error=None):
    report['status'] = 'failed' if error else 'passed'
    if error:
        report['error'] = error
    if state.get('delegate'):
        state['delegate'].remove_callable(loaded)
        state['delegate'] = None
    if state.get('retry'):
        state['retry'].stop()
        state['retry'] = None
    write()
    level.editor_request_end_play()
    state.update(phase='stopping', stopped_at=time.monotonic())


def tick(_delta):
    if state['busy']:
        return
    state['busy'] = True
    try:
        now = time.monotonic()
        if state['phase'] == 'stopping':
            if not level.is_in_play_in_editor() or now-state['stopped_at'] > 15.:
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert now-state['start'] < 420., 'E2 checkpoint replay deadline'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
        if not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready() or pawn.is_character_pending_load():
            return
        if state['phase'] == 'bootstrap':
            instance = unreal.GameplayStatics.get_game_instance(world)
            saves = next(item for item in unreal.ObjectIterator(unreal.SovSaveSubsystem)
                         if item.get_outer() == instance)
            if saves.is_load_pending():
                return
            banks = sorted((source / 'UserData/Saved/SaveGames').glob('*_2_0_*.sav'))
            assert len(banks) == 2, 'Earned source needs exactly two checkpoint banks'
            dest = (out / 'UserData/Saved/SaveGames').resolve()
            assert dest.is_relative_to(out.resolve())
            dest.mkdir(parents=True, exist_ok=True)
            for bank in banks:
                target = dest / bank.name
                digest = hashlib.sha256(bank.read_bytes()).hexdigest()
                shutil.copy2(bank, target)
                assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
                report['banks'].append(dict(name=bank.name, sha256=digest))
            state.update(saves=saves, delegate=saves.on_load_completed,
                         old_world=hash(world), phase='load')
            state['delegate'].add_callable(loaded)
            result, message = saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
            assert result == unreal.SovSaveResult.LOAD_STARTED, str(message)
            write()
            return
        if state['phase'] == 'load':
            if hash(world) == state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:
                return
            assert 'SUCCESS' in report['callbacks'][-1]['result'], report['callbacks']
            assert isinstance(pawn, unreal.SovSeleneCharacter), 'Checkpoint did not restore Selene'
            directors = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.SovEncounterDirector) if str(actor.encounter_id) == ENCOUNTER]
            assert len(directors) == 1, 'Exact E2 director missing after load'
            director = directors[0]
            report['restored_encounter_state'] = str(director.get_encounter_state())
            state['director'] = director
            campaign = unreal.GameplayStatics.get_player_controller(world, 0).get_campaign_state()
            report['restored_journal'] = [str(item.beat_id) for item in campaign.get_journal()]
            if director.get_encounter_state() == unreal.SovEncounterState.FAILED:
                state.update(retry=RetryInput(world, director, out / 'RetryInput'), phase='retry')
                write()
                return
            assert director.get_encounter_state() == unreal.SovEncounterState.ACTIVE, \
                'E2 was neither failed nor active after checkpoint load'
            state.update(phase='passive', passive_at=now, next_passive=now)
            write()
            return
        if state['phase'] == 'retry':
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if state['retry'].step(world, pc, pawn):
                report['retry_input_frames'] = state['retry'].driver.report['input_frames'].copy()
                state['retry'].stop()
                state['retry'] = None
                assert state['director'].get_encounter_state() == unreal.SovEncounterState.ACTIVE, \
                    'Normal retry did not activate E2'
                state.update(phase='passive', passive_at=now, next_passive=now)
                write()
            return
        if state['phase'] == 'passive':
            if now >= state['next_passive']:
                state['next_passive'] = now + 1.
                report['passive_samples'].append(dict(elapsed=round(now-state['passive_at'], 3),
                    encounter_state=str(state['director'].get_encounter_state()),
                    player_health=pawn.get_health()))
                write()
            if now-state['passive_at'] >= 30.:
                finish(None if state['director'].get_encounter_state() == unreal.SovEncounterState.ACTIVE
                       else 'E2 did not remain active during passive observation')
            return
    except Exception:
        finish(traceback.format_exc())
    finally:
        state['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
write()
level.editor_request_begin_play()
