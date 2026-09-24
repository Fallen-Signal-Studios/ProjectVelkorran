"""Light the isolated Z08 review map for an assessable front/player-scale capture."""
import hashlib
import json
import os
from pathlib import Path
import time
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
mapfile=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before=hashlib.sha256(mapfile.read_bytes()).hexdigest()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert world.get_name()=='L_Aurelion_Z08ContainmentPylon'
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert len([a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('Z08_REVIEW_SM_')])==2
for index,(position,intensity) in enumerate((((0,-520,530),85000),((420,-220,360),42000))):
    point=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*position))
    point.set_actor_label('Z08_REVIEW_FrontFill_'+str(index))
    light=point.get_component_by_class(unreal.PointLightComponent)
    light.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    light.set_intensity(intensity)
    light.set_attenuation_radius(1800)
position=unreal.Vector(370,-650,500)
rotation=unreal.MathLibrary.find_look_at_rotation(position,unreal.Vector(0,0,390))
camera=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
camera.set_level_viewport_camera_info(position,rotation)
editor.editor_set_game_view(True)
assert editor.save_current_level()
assert hashlib.sha256(mapfile.read_bytes()).hexdigest()==before
(out/'z08-pylon-lit-review.json').write_text(json.dumps(dict(status='saved_lit_review',
    camera=position.export_text(),fill_lights=2,m12_unchanged=True,m12_sha256=before),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.monotonic(),capture=False)
def tick(delta):
    elapsed=time.monotonic()-state['start']
    if not state['capture'] and elapsed>15:
        unreal.AutomationLibrary.take_high_res_screenshot(1200,1200,
            str(out/'z08-pylon-lit-unreal.png'))
        state['capture']=True
    if elapsed>25:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
state['handle']=unreal.register_slate_post_tick_callback(tick)
print('Z08_PYLON_LIT_REVIEW_PASS')
