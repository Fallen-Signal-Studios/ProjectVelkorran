"""Controlled CP2 posture/movement/wield review; not combat or physical-input qualification."""
import hashlib
import json
import os
import shutil
import time
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = Path(unreal.Paths.project_dir()) / 'Saved/Validation/Aurelion/SeleneShoulderHandoff-20260920-043213-8124ab93'
evidence = json.loads((source / 'E1Continuation/e1-input-continuation.json').read_text())
assert evidence['status'] == 'passed'
assert 'BP_SovSelene_C' in evidence['samples'][-1]['pawn']
assert any(e['beat'] == 'HandoffToSelene' for e in evidence['samples'][-1]['journal'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
settings = unreal.SovGameUserSettings.get_game_user_settings()
assert settings.complete_accessibility_setup()
report = dict(status='running', scope=__doc__, source=str(source), banks=[], callbacks=[], samples=[], presentation_errors=[])
state = dict(phase='bootstrap', started=time.monotonic(), at=time.monotonic(), busy=False, index=0)
cases = ('restored', 'bypass', 'crouch', 'crouch_bypass', 'walk', 'WI_Verity', 'WI_Staccato', 'restowed')


def write():
    (out / 'selene-posture-play.json').write_text(json.dumps(report, indent=2))


def completed(result, header, message):
    report['callbacks'].append(dict(result=str(result), header=header.export_text(), message=str(message)))
    write()


def stop(error=None):
    report['status'] = 'failed' if error or report['presentation_errors'] else 'passed_requires_visual_review'
    if error:
        report['error'] = error
    if state.get('delegate'):
        state['delegate'].remove_callable(completed)
        state['delegate'] = None
    write()
    level.editor_request_end_play()
    state['phase'] = 'stopping'


def tick(delta):
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
        assert now - state['started'] < 300, 'Checkpoint HUD review timed out'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():
            return
        if state['phase'] == 'bootstrap':
            instance = unreal.GameplayStatics.get_game_instance(world)
            saves = next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == instance)
            if saves.is_load_pending():
                return
            banks = sorted((source / 'UserData/Saved/SaveGames').glob('*_2_0_*.sav'))
            assert len(banks) == 2
            destination = (out / 'UserData/Saved/SaveGames').resolve()
            assert destination.is_relative_to(out.resolve())
            destination.mkdir(parents=True, exist_ok=True)
            for bank in banks:
                target = destination / bank.name
                if target.exists():
                    shutil.copy2(target, out / (bank.name + '.bootstrap'))
                digest = hashlib.sha256(bank.read_bytes()).hexdigest()
                shutil.copy2(bank, target)
                assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
                report['banks'].append(dict(source=str(bank), sha256=digest))
            state.update(saves=saves, old_world=hash(world), delegate=saves.on_load_completed)
            state['delegate'].add_callable(completed)
            result, message = saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
            assert result == unreal.SovSaveResult.LOAD_STARTED, str(message)
            state['phase'] = 'load'
            write()
            return
        if state['phase'] == 'load':
            if hash(world) == state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:
                return
            assert len(report['callbacks']) == 1 and 'SUCCESS' in report['callbacks'][0]['result']
            if pawn.is_character_pending_load():
                return
            assert pawn.get_class().get_name() == 'BP_SovSelene_C', pawn.get_class().get_name()
            state.update(phase='settle', at=now)
            return
        case = cases[state['index']]
        if state['phase'] == 'settle' and case == 'WI_Verity':
            if now - state['at'] > 1 and not state.get('attack_sent'):
                attack = unreal.GameplayTag()
                assert attack.import_text('(TagName="Narrative.Input.Attack")')
                asc = pawn.get_narrative_ability_system_component()
                asc.ability_input_tag_pressed(attack)
                asc.ability_input_tag_released(attack)
                state['attack_sent'] = True
            montage = pawn.get_editor_property('mesh').get_anim_instance().get_current_active_montage()
            if montage:
                path = montage.get_path_name()
                if path not in report.setdefault('verity_montages', []):
                    report['verity_montages'].append(path)
        if state['phase'] == 'settle' and case == 'walk':
            pawn.add_movement_input(state['walk_direction'], 0.35, False)
        if state['phase'] == 'settle' and now - state['at'] > (1.5 if case == 'walk' else 4):
            mesh = pawn.get_editor_property('mesh')
            anim = mesh.get_anim_instance()
            assert anim and anim.get_class().get_name() == 'ABP_Biped_C'
            preview = unreal.SovBlueprintAuthoringLibrary.preview_selene_feminine_posture(anim, 'bypass' not in case)
            assert preview.succeeded, preview.report
            values = dict(field.split('=') for field in preview.report.split())
            expected = 0.0 if 'bypass' in case or case.startswith('WI_') else 1.0
            assert abs(float(values['alpha']) - expected) < .01, (case, preview.report)
            if case == 'WI_Verity':
                assert any('Verity' in path for path in report.get('verity_montages', [])), 'Verity attack montage was not observed'
            speed = pawn.get_velocity().length()
            if case == 'walk':
                assert speed > 5, 'Walking input produced no locomotion'
            if case.startswith('crouch'):
                assert pawn.get_editor_property('is_crouched'), 'Crouch request did not enter crouch'
            wielded = pawn.get_wielded_weapons()
            if state.get('item'):
                assert state['item'] in wielded
            if case == 'restowed':
                assert not wielded
            report['samples'].append(dict(case=case, anim_class=anim.get_class().get_path_name(),
                posture=preview.report, speed=speed, crouched=pawn.get_editor_property('is_crouched'),
                wielded=[w.get_class().get_path_name() for w in wielded],
                bones={str(b):mesh.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_COMPONENT).export_text()
                    for b in ('root', 'pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')}))
            task = unreal.AssetExportTask()
            task.object = anim
            task.filename = str(out / ('anim-' + case + '.copy'))
            task.automated = True
            task.prompt = False
            unreal.Exporter.run_asset_export_task(task)
            unreal.SystemLibrary.execute_console_command(world, 'Shot showui -nosuffix filename=' +
                str(out / ('selene-posture-' + case + '.png')))
            state.update(phase='capture', at=now)
            write()
            return
        if state['phase'] == 'capture' and now - state['at'] > 3:
            state['index'] += 1
            if state['index'] == len(cases):
                stop()
                return
            name = cases[state['index']]
            if not name.startswith('crouch'):
                next(getattr(pawn, n) for n in ('un_crouch', 'uncrouch') if hasattr(pawn, n))(False)
            state['item'] = None
            anim = pawn.get_editor_property('mesh').get_anim_instance()
            preview = unreal.SovBlueprintAuthoringLibrary.preview_selene_feminine_posture(anim, 'bypass' not in name)
            assert preview.succeeded, preview.report
            if name in ('walk', 'crouch', 'crouch_bypass', 'bypass', 'restowed'):
                if name == 'walk':
                    state['walk_direction'] = pawn.get_actor_right_vector()
                if name.startswith('crouch'):
                    pawn.crouch(False)
                if name == 'restowed':
                    pawn.set_wield_state(unreal.WeaponWieldState())
                state.update(phase='settle', at=now)
                return
            inventory = pawn.get_component_by_class(unreal.NarrativeInventoryComponent)
            item = inventory.find_item_of_class(unreal.load_class(None, '/Game/Items/Weapons/' + name + '.' + name + '_C'), False)
            assert item, name + ' is absent from restored inventory'
            slot = str(unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property('current_slot')))
            assert slot not in ('', 'None')
            equip = unreal.GameplayTagContainer()
            assert equip.import_text('(GameplayTags=((TagName="' + slot + '")))')
            hands = unreal.GameplayTagContainer()
            assert hands.import_text('(GameplayTags=((TagName="Narrative.Equipment.WieldSlot.Mainhand")))')
            wield = unreal.WeaponWieldState()
            wield.set_editor_property('equip_slots', equip)
            wield.set_editor_property('equip_weapons', [item])
            wield.set_editor_property('wield_slots', hands)
            pawn.set_wield_state(wield)
            state.update(item=item, phase='settle', at=now)
    except Exception:
        stop(traceback.format_exc())
    finally:
        state['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
write()
level.editor_request_begin_play()
