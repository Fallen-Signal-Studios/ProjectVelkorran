"""Publicly reload low-ammo E3, then select Velkorran through the ordinary weapon wheel."""
import hashlib
import json
import os
import shutil
import sys
import time
import traceback
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(unreal.Paths.project_dir()) / 'Scripts/Validation/Aurelion'))
from aurelion_wheel_input import Selector
from aurelion_retry_input import RetryInput


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = Path(os.environ['SOV_AURELION_E3_AMMO_SOURCE'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
# Each validation run uses an isolated first-boot profile. Accept the unchanged
# default accessibility settings before PIE so its setup menu does not pause combat.
assert unreal.SovGameUserSettings.get_game_user_settings().complete_accessibility_setup()
report = dict(status='running', source=str(source), banks=[], callbacks=[])
state = dict(phase='bootstrap', started=time.monotonic(), busy=False)


def write():
    (out / 'e3-ammo-fallback.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


def loaded(result, header, message):
    report['callbacks'].append(dict(result=str(result), header=header.export_text(), message=str(message)))
    write()


def finish(error=None):
    report.update(status='failed' if error else 'passed', error=error)
    if state.get('delegate'):
        state['delegate'].remove_callable(loaded)
        state['delegate'] = None
    if state.get('retry'):
        state['retry'].stop()
        state['retry'] = None
    if state.get('input') and state.get('wheel_action'):
        state['input'].inject_input_vector_for_action(state['wheel_action'], unreal.Vector(), [], [])
    write()
    level.editor_request_end_play()
    state['phase'] = 'stopping'


def tick(_delta):
    if state['busy']:
        return
    state['busy'] = True
    try:
        if state['phase'] == 'stopping':
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert time.monotonic() - state['started'] < 180, 'E3 checkpoint inspection timeout'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():
            return
        if state['phase'] == 'retry':
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            interaction = pc.get_interaction_component()
            report['retry_progress'] = dict(
                elapsed=round(time.monotonic() - state['started'], 3),
                remaining=interaction.get_editor_property('remaining_interact_time'),
                viewed=interaction.get_editor_property('viewed_interactable').get_path_name()
                    if interaction.get_editor_property('viewed_interactable') else None,
                last_result=str(state['retry'].actor.get_editor_property('last_result')),
                pending=state['retry'].actor.is_request_pending(),
                frames=state['retry'].driver.report['input_frames'].copy(),
                samples=state['retry'].samples[-4:])
            report['retry_context'] = dict(
                interaction_time=state['retry'].actor.interactable.get_editor_property('interaction_time'),
                move_ignored=pc.is_move_input_ignored(),
                look_ignored=pc.is_look_input_ignored(),
                transition=str(pc.get_campaign_transition_state()),
                paused=unreal.GameplayStatics.is_game_paused(world),
                tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(pawn).export_text())
            if time.monotonic() - state.get('last_hold_sample', 0.) >= .15:
                state['last_hold_sample'] = time.monotonic()
                report.setdefault('hold_samples', []).append(dict(
                    elapsed=report['retry_progress']['elapsed'],
                    remaining=report['retry_progress']['remaining'],
                    viewed=report['retry_progress']['viewed'],
                    last_result=report['retry_progress']['last_result'],
                    pending=report['retry_progress']['pending'],
                    frames=report['retry_progress']['frames'].get('IA_Interact', 0),
                    hold_started=state['retry'].hold_started is not None))
            write()
            if not state['retry'].step(world, pc, pawn):
                return
            report['retry'] = dict(samples=state['retry'].samples,
                input_frames=state['retry'].driver.report['input_frames'].copy())
            state['retry'].stop()
            state['retry'] = None
            inventory = pawn.get_component_by_class(unreal.NarrativeInventoryComponent)
            sword_class = unreal.load_class(None, '/Game/Items/Weapons/WI_Velkorran.WI_Velkorran_C')
            sword = inventory.find_item_of_class(sword_class, False)
            assert sword is not None, 'Retry did not retain Velkorran'
            engine = unreal.GameplayStatics.get_game_instance(world).get_outer()
            owners = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)
                if isinstance(s.get_outer(), unreal.LocalPlayer) and s.get_outer().get_outer() == engine]
            assert len(owners) == 1
            wheel_action = unreal.load_asset('/NarrativePro/Pro/Core/Data/Input/IA_WeaponWheel')
            assert wheel_action
            state.update(phase='wheel', sword=sword, input=owners[0], wheel_action=wheel_action,
                selector=Selector('/Game/Items/Weapons/WI_Velkorran.WI_Velkorran_C', manual_mainhand=True))
            write()
            return
        if state['phase'] == 'bootstrap':
            instance = unreal.GameplayStatics.get_game_instance(world)
            saves = next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == instance)
            if saves.is_load_pending():
                return
            banks = sorted((source / 'UserData/Saved/SaveGames').glob('*_2_0_*.sav'))
            assert len(banks) == 2, 'Expected earned checkpoint bank pair'
            destination = (out / 'UserData/Saved/SaveGames').resolve()
            assert destination.is_relative_to(out.resolve())
            destination.mkdir(parents=True, exist_ok=True)
            for bank in banks:
                target = destination / bank.name
                digest = hashlib.sha256(bank.read_bytes()).hexdigest()
                shutil.copy2(bank, target)
                assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
                report['banks'].append(dict(name=bank.name, sha256=digest))
            state.update(saves=saves, delegate=saves.on_load_completed, old_world=hash(world), phase='load')
            state['delegate'].add_callable(loaded)
            result, message = saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
            assert result == unreal.SovSaveResult.LOAD_STARTED, str(message)
            write()
            return
        if state['phase'] == 'wheel':
            held, result = state['selector'].step(world)
            state['input'].inject_input_vector_for_action(state['wheel_action'], unreal.Vector(float(held), 0., 0.), [], [])
            report['wheel'] = result
            if state['selector'].done:
                assert result['status'] == 'passed', result.get('reason', 'Weapon-wheel selection failed')
                sword = state['sword']
                asc = pawn.get_narrative_ability_system_component()
                tag = unreal.GameplayTag()
                heavy_tag = unreal.GameplayTag()
                assert tag.import_text('(TagName="Narrative.Input.Attack")')
                assert heavy_tag.import_text('(TagName="Narrative.Input.Attack.Heavy")')
                report['after_wheel'] = dict(sword_wielded=sword.is_wielded(),
                    wielded=[weapon.get_path_name() for weapon in pawn.get_wielded_weapons() if weapon],
                    light_grants=list(unreal.SovMeleeValidationLibrary.granted_ability_classes_for_input(asc, tag)),
                    heavy_grants=list(unreal.SovMeleeValidationLibrary.granted_ability_classes_for_input(asc, heavy_tag)))
                assert sword.is_wielded(), 'Owned Velkorran did not become the wielded melee fallback'
                assert 'GA_Tarrik_MeleeLight_C' in report['after_wheel']['light_grants']
                finish()
            else:
                write()
            return
        if hash(world) == state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:
            return
        assert 'SUCCESS' in report['callbacks'][-1]['result'], report['callbacks']
        assert isinstance(pawn, unreal.SovTarrikCharacter)
        inventory = pawn.get_component_by_class(unreal.NarrativeInventoryComponent)
        assert inventory
        sword_class = unreal.load_class(None, '/Game/Items/Weapons/WI_Velkorran.WI_Velkorran_C')
        sword = inventory.find_item_of_class(sword_class, False)
        asc = pawn.get_narrative_ability_system_component()
        tag = unreal.GameplayTag()
        heavy_tag = unreal.GameplayTag()
        assert tag.import_text('(TagName="Narrative.Input.Attack")')
        assert heavy_tag.import_text('(TagName="Narrative.Input.Attack.Heavy")')
        report['player'] = dict(world=world.get_path_name(), pawn=pawn.get_path_name(),
            alive=pawn.is_alive(), ready=pawn.is_character_ready(),
            sword_owned=sword is not None,
            sword_slot=sword.get_editor_property('current_slot').export_text() if sword else None,
            sword_wielded=sword.is_wielded() if sword else None,
            wielded=[weapon.get_path_name() for weapon in pawn.get_wielded_weapons() if weapon],
            light_grants=list(unreal.SovMeleeValidationLibrary.granted_ability_classes_for_input(asc, tag)),
            heavy_grants=list(unreal.SovMeleeValidationLibrary.granted_ability_classes_for_input(asc, heavy_tag)))
        assert sword is not None, 'The E3 checkpoint did not retain Velkorran'
        directors = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector)
            if str(actor.encounter_id) == 'M12_E3_SharedBreach']
        assert len(directors) == 1
        assert directors[0].get_encounter_state() == unreal.SovEncounterState.FAILED
        state.update(phase='retry', retry=RetryInput(world, directors[0], out / 'AuthoredRetry'))
        write()
    except Exception:
        finish(traceback.format_exc())
    finally:
        state['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
write()
level.editor_request_begin_play()
