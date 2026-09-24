"""Correct the isolated Z08 review composition after first in-engine capture."""
import hashlib
import json
import os
from pathlib import Path
import time
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
mapfile = root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before = hashlib.sha256(mapfile.read_bytes()).hexdigest()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert world.get_name() == 'L_Aurelion_Z08ContainmentPylon'
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
parts = [a for a in actors if a.get_actor_label().startswith('Z08_REVIEW_SM_')]
assert len(parts)==2
for actor in parts:
    actor.set_actor_rotation(unreal.Rotator(yaw=180),False)
camera = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
position = unreal.Vector(550,-900,570)
rotation = unreal.MathLibrary.find_look_at_rotation(position,unreal.Vector(0,0,390))
camera.set_level_viewport_camera_info(position,rotation)
editor.editor_set_game_view(True)
assert editor.save_current_level()
assert hashlib.sha256(mapfile.read_bytes()).hexdigest() == before
(out/'z08-pylon-facing.json').write_text(json.dumps(dict(status='saved_review_map',
    actor_rotations={a.get_actor_label():a.get_actor_rotation().export_text() for a in parts},
    camera=position.export_text(),m12_unchanged=True,m12_sha256=before),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.monotonic(),capture=False)
def tick(delta):
    elapsed=time.monotonic()-state['start']
    if not state['capture'] and elapsed>12:
        unreal.AutomationLibrary.take_high_res_screenshot(1200,1200,
            str(out/'z08-pylon-front-unreal.png'))
        state['capture']=True
    if elapsed>22:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
state['handle']=unreal.register_slate_post_tick_callback(tick)
print('Z08_PYLON_FACING_REVIEW_PASS')
