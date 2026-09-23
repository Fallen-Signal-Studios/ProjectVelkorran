"""Isolated replay of genuine checkpoint banks, followed by an ordinary-shot probe."""
import hashlib
import importlib
import json
import os
from pathlib import Path
import shutil
import sys
import time
import traceback
from datetime import datetime
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
PROFILE = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']).resolve()
OUT = PROFILE / ('ContactProbe-' + datetime.now().strftime('%H%M%S-%f'))
OUT.mkdir(exist_ok=False)
SOURCE = Path(os.environ.get('SOV_EARNED_COMPANION_SAVE_DIRECTORY', str(
    Path(unreal.Paths.project_dir()) / 'Saved/Validation/Aurelion/CompanionPrimaryRoute-20260919-183439-ce4bd05a/UserData/Saved/SaveGames'))).resolve()
report = dict(status='bootstrapping', method='Unmodified earned checkpoint banks copied only into this isolated test profile; public native load; optional ordinary retry input and controlled companion command; explicit input owner recorded below', banks=[], callbacks=[])
state = dict(phase='bootstrap', started=time.monotonic(), phase_started=time.monotonic(), saves=None, delegate=None, handle=None)

def write():
    (OUT / 'earned-contact-probe.json').write_text(json.dumps(report, indent=2))

def completed(result, header, message):
    report['callbacks'].append(dict(result=str(result), header=header.export_text(), message=str(message)))
    write()

def finish(status, reason):
    report.update(status=status, reason=reason)
    if state.get('retry_input') is not None:
        report['retry_input'] = dict(samples=state['retry_input'].samples,
            frames=state['retry_input'].driver.report['input_frames'])
        state['retry_input'].stop()
        state['retry_input'] = None
    if state['delegate'] is not None:
        state['delegate'].remove_callable(completed)
        state['delegate'] = None
    unreal.unregister_slate_post_tick_callback(state['handle'])
    state['handle'] = None
    state['saves'] = None
    write()

def tick(delta):
    try:
        if time.monotonic()-state['phase_started'] > 210:
            finish('timeout', state['phase'] + ' exceeded its separate observation bound; see readiness samples')
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
        now = time.monotonic()
        if now-state.get('last_readiness', 0.) >= 2.:
            state['last_readiness'] = now
            report.setdefault('readiness', []).append(dict(phase=state['phase'], elapsed=now-state['started'],
                world=world.get_name() if world else None, pawn=pawn.get_class().get_name() if pawn else None,
                ready=pawn.is_character_ready() if isinstance(pawn, unreal.SovPlayerCharacterBase) else None,
                pending=pawn.is_character_pending_load() if isinstance(pawn, unreal.SovPlayerCharacterBase) else None,
                load_pending=state['saves'].is_load_pending() if state['saves'] else None))
            write()
        if not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready() or pawn.is_character_pending_load():
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        mission = pc.get_campaign_state()
        if not mission.is_state_valid():
            return
        if state['phase'] == 'bootstrap':
            instance = unreal.GameplayStatics.get_game_instance(world)
            owners = [s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == instance]
            if len(owners) != 1 or owners[0].is_load_pending() or owners[0].is_awaiting_failure_decision():
                return
            saves = owners[0]
            if not any(h.kind == unreal.SovSaveSlotKind.CHECKPOINT for h in saves.list_slots()):
                return
            destination = (PROFILE / 'UserData/Saved/SaveGames').resolve()
            assert destination.is_relative_to(PROFILE) and destination.is_dir()
            banks = sorted(SOURCE.glob('*_2_0_*.sav'))
            assert len(banks) == 2
            for source in banks:
                target = destination / source.name
                backup = OUT / (source.name + '.bootstrap')
                assert not backup.exists()
                if target.exists():
                    assert target.is_file()
                    shutil.copy2(target, backup)
                digest = hashlib.sha256(source.read_bytes()).hexdigest()
                shutil.copy2(source, target)
                assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
                report['banks'].append(dict(source=str(source), target=str(target), sha256=digest))
            headers = [h for h in saves.list_slots() if h.kind == unreal.SovSaveSlotKind.CHECKPOINT and h.slot_index == 0]
            # ListSlots intentionally caches summaries; an external copy does not
            # refresh them. LoadSlot reads the banks and its callback is authoritative.
            report['cached_headers_before_load'] = [h.export_text() for h in headers]
            state['world_hash'] = hash(world)
            state['saves'] = saves
            state['delegate'] = saves.on_load_completed
            state['delegate'].add_callable(completed)
            result, message = saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
            report.update(request_result=str(result), request_message=str(message), status='loading')
            assert result == unreal.SovSaveResult.LOAD_STARTED
            state['phase'] = 'load'
            state['phase_started'] = time.monotonic()
            write()
        elif hash(world) != state['world_hash'] and not state['saves'].is_load_pending() and report['callbacks']:
            assert len(report['callbacks']) == 1 and 'SUCCESS' in report['callbacks'][0]['result']
            boundary = os.environ.get('SOV_CONTACT_BOUNDARY', 'M12_E4_QuarantineCrucibleB')
            assert boundary in ('M12_E4_QuarantineCrucibleA', 'M12_E4_QuarantineCrucibleB'), 'Unsupported contact checkpoint'
            assert ('BoundaryId="' + boundary + '"') in report['callbacks'][0]['header']
            expected = unreal.SovSeleneCharacter if boundary == 'M12_E4_QuarantineCrucibleA' else unreal.SovTarrikCharacter
            assert isinstance(pawn, expected) and pawn.is_alive()
            if os.environ.get('SOV_CONTACT_FOLLOW_E4A') == '1' or os.environ.get('SOV_CONTACT_RETRY_ENTRY') == '1':
                objectives = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovCampaignEncounterObjective)
                    if a.encounter_director and str(a.encounter_director.encounter_id) == boundary]
                assert len(objectives) == 1
                objective = objectives[0]
                director = objective.encounter_director
                report['native_entry'] = dict(state=str(director.get_encounter_state()),
                    player=pawn.get_actor_location().export_text(),
                    overlap=objective.start_volume.is_overlapping_component(pawn.get_editor_property('capsule_component')),
                    retry=objective.get_editor_property('retry_initial_entry_while_overlapping'),
                    last_error=str(objective.get_editor_property('last_error')))
                report.setdefault('native_entry_samples', []).append(dict(report['native_entry']))
                state.setdefault('entry_wait_started', time.monotonic())
                write()
                if director.get_encounter_state() != unreal.SovEncounterState.ACTIVE:
                    # Arena-entry load deliberately parks participants in Failed.
                    # Use the same authored interaction the player must use;
                    # automatic initial-entry polling never retries an attempt.
                    if state.get('retry_input') is None:
                        assert director.get_encounter_state() == unreal.SovEncounterState.FAILED
                        from aurelion_retry_input import RetryInput
                        state['retry_input'] = RetryInput(world, director, OUT / 'RetryInput')
                    state['retry_input'].step(world, pc, pawn)
                    return
                if state.get('retry_input') is not None:
                    assert state['retry_input'].step(world, pc, pawn)
                    report['retry_input'] = dict(samples=state['retry_input'].samples,
                        frames=state['retry_input'].driver.report['input_frames'])
                    state['retry_input'].stop()
                    state['retry_input'] = None
            import observe_companion_after_player_shot as contact
            assert contact._RUN is None or contact._RUN.handle is None, 'A contact observer already owns this session'
            importlib.reload(contact)
            follow_route = os.environ.get('SOV_CONTACT_FOLLOW_E4A') == '1'
            command_only = os.environ.get('SOV_CONTACT_COMMAND_ONLY') == '1'
            passive_only = os.environ.get('SOV_CONTACT_PASSIVE_ONLY') == '1'
            contact.start(OUT, passive=follow_route or command_only or passive_only)
            if follow_route:
                import continue_aurelion_e4a_input as route
                route.start(OUT / 'E4A')
                report['input_owner'] = 'Existing normal E4A route driver; contact observer is passive'
            elif passive_only:
                report['input_owner'] = 'No player or companion combat input; passive observation of authored autonomous behavior'
            else:
                companion = contact._RUN.companion.get_companion_component()
                admitted = companion.request_command(pawn, unreal.SovCompanionCommand.FOCUS_TARGET, contact._RUN.target)
                report['controlled_focus_request'] = str(admitted)
                report['command_only'] = command_only
                report['input_owner'] = 'No player combat input; passive observation after public companion command' if command_only else 'Existing ordinary firearm trigger observer'
            report['journal'] = [str(e.beat_id) for e in mission.get_journal()]
            finish('probe_started', 'Real checkpoint restored; contact observation started with the recorded input owner')
    except Exception:
        finish('failed', traceback.format_exc())

settings = unreal.GameUserSettings.get_game_user_settings()
assert isinstance(settings, unreal.SovGameUserSettings)
report['settings'] = settings.get_settings_snapshot().export_text()
assert settings.complete_accessibility_setup()
write()
state['handle'] = unreal.register_slate_post_tick_callback(tick)
if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
