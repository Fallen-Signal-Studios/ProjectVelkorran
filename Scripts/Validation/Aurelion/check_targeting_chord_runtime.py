"""Fresh saved mappings and trigger states under the actual weapon-wheel action.

Injects the wheel and targeting actions with their authored mapping triggers,
not hardware keys. This does not qualify gamepad routing, base-action suppression
or target selection end to end.
"""
import json
import os
import time
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
settings = unreal.GameUserSettings.get_game_user_settings()
original = settings.get_settings_snapshot()
settings.complete_accessibility_setup()
report = dict(status='running', qualification=__doc__, samples=[])
context = unreal.load_asset('/Game/Input/IMC_Combat')
rows = list(context.get_editor_property('default_key_mappings').get_editor_property('mappings'))
wheel_actions = {r.action for r in rows if r.action.get_name() == 'IA_WeaponWheel'}
assert len(wheel_actions) == 1
wheel = next(iter(wheel_actions))
triggers = [(r.action.get_name(), t) for r in rows for t in r.triggers if isinstance(t, unreal.InputTriggerChordAction)]
report['serialized_triggers'] = [dict(action=a, trigger=t.get_path_name(), outer=t.get_outer().get_path_name(),
    source=str(t.get_editor_property('chord_action'))) for a,t in triggers]
(out / 'targeting-chord-runtime.json').write_text(json.dumps(report, indent=2))
assert len(triggers) == 9
assert all(t.get_editor_property('chord_action') == wheel and t.get_outer() == context for _, t in triggers)
phase = 'start'
started = time.monotonic()
at = started
index = 0
values = [0., 1., 0.]
events = []
delegate = None
target_rows = [r for r in rows if r.action.get_name() in ('IA_ThreatFocus','IA_Designate','IA_CycleTargetLeft','IA_CycleTargetRight')
               and str(r.key.get_editor_property('key_name')).startswith('Gamepad')]
def semantic(tag, pressed):
    events.append(dict(tag=str(unreal.GameplayTagLibrary.get_tag_name(tag)), pressed=bool(pressed), phase=index))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def tick(delta):
    global phase, at, index, delegate
    try:
        if time.monotonic() - started > 90:
            raise AssertionError('Timed out: ' + phase)
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase == 'start':
            editor.editor_request_begin_play()
            phase = 'ready'
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
        if not pawn or not pawn.is_character_ready() or pawn.is_character_pending_load():
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if delegate is None:
            delegate = pc.on_semantic_input_changed
            delegate.add_callable(semantic)
        inputs = [p for p in unreal.ObjectIterator(unreal.EnhancedPlayerInput) if p.get_outer() == pc]
        assert len(inputs) == 1
        engine = unreal.GameplayStatics.get_game_instance(world).get_outer()
        owners = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)
                  if isinstance(s.get_outer(), unreal.LocalPlayer) and s.get_outer().get_outer() == engine]
        assert len(owners) == 1
        owners[0].inject_input_vector_for_action(wheel, unreal.Vector(values[index], 0., 0.), [], [])
        for row in target_rows:
            owners[0].inject_input_vector_for_action(row.action, unreal.Vector(1.,0.,0.), list(row.modifiers), list(row.triggers))
        if phase == 'ready':
            phase = 'settle'
            at = time.monotonic()
        elif phase == 'settle' and time.monotonic() - at > 2:
            held = bool(pc.get_editor_property('WeaponWheelHeld'))
            current_events = [e for e in events if e['phase'] == index]
            report['samples'].append(dict(wheel_input=values[index], wheel_held=held,
                look_ignored=pc.is_look_input_ignored(), move_ignored=pc.is_move_input_ignored(), events=current_events))
            assert held == bool(values[index]), 'Actual wheel held state did not follow injected action'
            for name in ('ThreatFocus','Designate','CycleTargetLeft','CycleTargetRight'):
                pressed = any(e['pressed'] and e['tag'] == 'Narrative.Input.' + name for e in current_events)
                assert pressed == bool(values[index]), (name, current_events)
            index += 1
            if index == len(values):
                report['status'] = 'passed_controlled_chord_semantics'
                phase = 'end'
            else:
                phase = 'ready'
    except Exception as exc:
        report.update(status='failed', error=str(exc))
        phase = 'end'
    if phase == 'end':
        if delegate is not None:
            delegate.remove_callable(semantic)
        settings.apply_settings_snapshot(original)
        (out / 'targeting-chord-runtime.json').write_text(json.dumps(report, indent=2))
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)

handle = unreal.register_slate_post_tick_callback(tick)
