"""Exercise early cue activation, cancellation and reuse in an unsaved SIE scene."""
import json
import os
import time
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
cue = unreal.load_asset('/Game/Cues/Aurelion/GC_AurelionLethalFloor')
cue_defaults = unreal.get_default_object(cue.generated_class())
assert cue_defaults.get_editor_property('auto_destroy_on_remove')
assert cue_defaults.get_editor_property('auto_destroy_delay') == 0.0
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert editor.load_level('/Game/Aurelion/ArtReview/Chaos/L_Aurelion_ChaosPrototype')
classes = [unreal.load_asset('/Game/Aurelion/Enemies/BP_Aurelion' + r).generated_class()
           for r in ('Elite', 'Weaver')]
for i, (role, cls) in enumerate(zip(('Elite', 'Weaver'), classes)):
    for group in ('held', 'cancelled'):
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(0 if group == 'held' else 800, i * 600, 160))
        assert actor
        actor.set_editor_property('authored_placed_definition', unreal.load_asset('/Game/Aurelion/Enemies/NPC_Aurelion' + role))
        actor.set_actor_label('FloorStartup_' + role + '_' + group)

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(start=time.monotonic(), stage=0, at=0, rows=[])

def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    editor.editor_request_end_play()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(delta):
    try:
        assert time.monotonic() - state['start'] < 90, 'Startup probe timed out'
        game = worlds.get_game_world()
        if not game:
            return
        now = unreal.GameplayStatics.get_time_seconds(game)
        targets = [a for cls in classes for a in unreal.GameplayStatics.get_all_actors_of_class(game, cls)
                   if a.get_actor_label().startswith('FloorStartup_')]
        if len(targets) != 4:
            return
        floors = [a.get_component_by_class(unreal.SovLethalFloorComponent) for a in targets]
        assert all(floors)
        if state['stage'] == 0:
            for actor, floor in zip(targets, floors):
                assert not actor.get_character_visual(), 'Probe missed the pre-visual startup window'
                floor.set_floor_held(True)
                if actor.get_actor_label().endswith('cancelled'):
                    floor.set_floor_held(False)
                state['rows'].append(dict(actor=actor.get_actor_label(), activated_before_visual=True, time=now))
            state.update(stage=1, at=now)
            return
        if now - state['at'] < 2:
            return
        if not all(a.get_character_visual() and a.get_character_visual().get_all_meshes() for a in targets):
            return
        if state['stage'] == 1 and 'ready_at' not in state:
            state['ready_at'] = now
            return
        if state['stage'] == 1 and now - state['ready_at'] < 0.2:
            return
        for actor, floor in zip(targets, floors):
            overlays = [m.get_overlay_material() for m in actor.get_character_visual().get_all_meshes()]
            expect_active = state['stage'] == 3 or (state['stage'] == 1 and actor.get_actor_label().endswith('held'))
            assert floor.is_floor_held() == expect_active
            if expect_active:
                assert overlays and all(overlays), actor.get_path_name() + ': missing overlay'
                assert all('M_AurelionPhaseLattice' in m.get_editor_property('parent').get_path_name() for m in overlays)
            else:
                assert not any(overlays), actor.get_path_name() + ': cancelled overlay appeared or survived removal'
            state['rows'].append(dict(actor=actor.get_actor_label(), stage=state['stage'], active_meshes=sum(bool(m) for m in overlays)))
        if state['stage'] in (1, 3):
            for floor in floors:
                floor.set_floor_held(False)
        elif state['stage'] == 2:
            for floor in floors:
                floor.set_floor_held(True)
        else:
            (out / 'lethal-cue-startup.json').write_text(json.dumps(dict(status='startup_cancel_reuse_pass', rows=state['rows'],
                qualification='Isolated SIE lifecycle only; no visual, damage, poise or campaign acceptance.'), indent=2))
            unreal.log('LETHAL_CUE_STARTUP_CANCEL_REUSE_PASS')
            finish()
            return
        state.update(stage=state['stage'] + 1, at=now)
    except Exception as error:
        (out / 'lethal-cue-startup.json').write_text(json.dumps(dict(status='failed', error=str(error), rows=state['rows']), indent=2))
        finish()
        raise

handle = unreal.register_slate_post_tick_callback(tick)
editor.editor_play_simulate()
