"""Capture three fixed M13 route views without saving the temporary camera."""
from pathlib import Path
import json
import os
import time
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Aurelion_M13'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
views = [('chamber', (0, 33600, -1570)), ('lift', (0, 37500, -1570)), ('gallery', (0, 41900, 170))]
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(*views[0][1]), unreal.Rotator(yaw=90))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(80)
level.editor_set_game_view(True)
level.pilot_level_actor(camera)
state = dict(index=0, stage=0, next=time.monotonic()+12, busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def finish():
    level.eject_pilot_level_actor()
    actors.destroy_actor(camera)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(delta):
    if state['busy'] or time.monotonic() < state['next']:
        return
    state['busy'] = True
    try:
        if state.get('task') and not state['task'].is_task_done():
            return
        if state['index'] >= len(views):
            assert all((out / (name+'.png')).is_file() for name, _ in views)
            (out / 'route-views.json').write_text(json.dumps(dict(
                views=[name for name, _ in views],
                qualification='Fixed editor game-view captures; no runtime mission or GPU-performance acceptance.'
            ), indent=2))
            finish()
            return
        name, position = views[state['index']]
        if state['stage'] == 0:
            camera.set_actor_location(unreal.Vector(*position), False, False)
            state.update(stage=1, next=time.monotonic()+10)
        else:
            state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, str(out / (name+'.png')), camera)
            state.update(index=state['index']+1, stage=0, next=time.monotonic()+2)
    except Exception:
        finish()
        raise
    finally:
        state['busy'] = False

handle = unreal.register_slate_post_tick_callback(tick)
