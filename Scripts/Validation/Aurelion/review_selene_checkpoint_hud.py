"""Review Selene HUD after publicly loading an unmodified, previously earned save.

This is checkpoint restoration plus controlled Narrative wield-API coverage,
not a new E1 victory, physical-input test or live handoff acceptance.
"""
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
cases = ('restored', 'WI_Verity', 'WI_Staccato')


def write():
    (out / 'selene-checkpoint-hud.json').write_text(json.dumps(report, indent=2))


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
        if state['phase'] == 'settle' and now - state['at'] > 6:
            surfaces = [w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface)
                        if w.get_world() == world and w.is_in_viewport()]
            assert len(surfaces) == 1
            surface = surfaces[0]
            view = surface.get_holographic_hud_view()
            assert view.valid and 'Selene' in str(unreal.GameplayTagLibrary.get_tag_name(view.protagonist))
            assert view.palette.accent.g > view.palette.accent.r and view.palette.accent.b > view.palette.accent.r
            assert abs(view.health.current - pawn.get_health()) < .1
            for field, resource in [('HealthBar', view.health), ('ShieldBar', view.shield)]:
                assert abs(surface.get_editor_property(field).get_editor_property('percent') - resource.fraction) < .001
            visible = surface.get_editor_property('AmmoRegion').get_visibility() != unreal.SlateVisibility.COLLAPSED
            assert visible == view.has_ammo
            item = state.get('item')
            if item:
                assert item in pawn.get_wielded_weapons()
            if cases[state['index']] == 'WI_Verity':
                if visible:
                    report['presentation_errors'].append('Verity displays a firearm magazine')
            if cases[state['index']] == 'WI_Staccato':
                assert visible and view.ammo_in_clip == item.get_ammo_in_clip()
                assert view.ammo_reserve == item.get_spare_ammo()
            if visible:
                assert str(surface.get_editor_property('AmmoClip').get_text()) == str(view.ammo_in_clip)
                assert str(surface.get_editor_property('AmmoReserve').get_text()) == str(view.ammo_reserve)
            pixels = unreal.WidgetLayoutLibrary.get_viewport_size(world)
            report['samples'].append(dict(case=cases[state['index']], pawn=pawn.get_class().get_name(),
                health=view.health.export_text(), shield=view.shield.export_text(), palette=view.palette.export_text(),
                ammo_visible=visible, clip=view.ammo_in_clip, reserve=view.ammo_reserve,
                wielded=[w.get_class().get_path_name() for w in pawn.get_wielded_weapons()],
                viewport_pixels=[pixels.x, pixels.y], pips=[p.export_text() for p in view.pips]))
            unreal.SystemLibrary.execute_console_command(world, 'Shot showui -nosuffix filename=' +
                str(out / ('selene-' + cases[state['index']] + '.png')))
            state.update(phase='capture', at=now)
            write()
            return
        if state['phase'] == 'capture' and now - state['at'] > 3:
            state['index'] += 1
            if state['index'] == len(cases):
                stop()
                return
            name = cases[state['index']]
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
