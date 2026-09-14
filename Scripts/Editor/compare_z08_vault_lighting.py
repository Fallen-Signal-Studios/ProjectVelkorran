"""Unsaved fixed-camera comparison of vault light aim and source size."""
from pathlib import Path
import json,os,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());assert len(actors)==3140
labels={a.get_actor_label():a for a in actors};lights=[]
for side in (-1,1):
    for index in (0,1):
        a=labels[f'ENVL_CrucibleVault_{side}_{index}'];c=a.get_component_by_class(unreal.RectLightComponent)
        lights.append((a,c,side,a.get_actor_rotation(),c.source_width,c.source_height))
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-500,19800,-950),unreal.Rotator(pitch=20,yaw=75))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(90);editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
views=['baseline','inward-wash','inward-compact-source'];state=dict(start=time.monotonic(),phase=0,busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    for a,c,side,rotation,width,height in lights:
        a.set_actor_rotation(rotation,False);c.set_source_width(width);c.set_source_height(height)
def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        phase=state['phase'];elapsed=time.monotonic()-state['start']
        if phase<6 and elapsed>15+phase*12:
            state['phase']+=1
            if phase%2==0:
                restore()
                if phase>=2:
                    for a,c,side,rotation,width,height in lights:
                        a.set_actor_rotation(unreal.Rotator(pitch=25,yaw=0 if side<0 else 180),False)
                        if phase==4:c.set_source_width(120);c.set_source_height(80)
            else:state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/('z08-'+views[phase//2]+'.png')),camera)
        elif phase==6 and elapsed>95:
            assert all((out/('z08-'+v+'.png')).exists() for v in views);restore()
            (out/'vault-light-comparison.json').write_text(json.dumps(dict(status='unsaved_comparison',views=views,qualification='Fixed camera and unchanged intensity; exposure/GI settling may still affect comparisons. No save or runtime qualification.'),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
