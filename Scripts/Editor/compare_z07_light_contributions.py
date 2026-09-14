"""Unsaved same-camera comparisons of external fill and local key shadows."""
from pathlib import Path
import json,os,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=list(subsystem.get_all_level_actors());assert len(actors)==2923
labels={a.get_actor_label():a for a in actors}
names=['ENVL_Z06_Key_02','ENVL_Z08_Key_01','ENVL_Z08_Key_02','ENVL_Z08_Key_03']+[f'Aurelion_Atrium_Pier_Uplight_{i:02}' for i in range(5,9)]
external=[labels[n].get_component_by_class(unreal.LightComponent) for n in names]
local=[labels[f'ENVL_Z07_Key_{i:02}'].get_component_by_class(unreal.RectLightComponent) for i in range(2)]
assert all(c.get_editor_property('visible') and not c.get_editor_property('cast_shadows') for c in external+local)
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-1100,14200,-710),unreal.Rotator(pitch=8,yaw=65))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(90)
editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
views=['baseline','external-off','local-shadows']
state=dict(start=time.monotonic(),phase=0,busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        phase=state['phase'];elapsed=time.monotonic()-state['start']
        if phase<6 and elapsed>15+phase*12:
            state['phase']+=1
            if phase%2==0:
                for c in external:c.set_visibility(phase!=2)
                for c in local:c.set_cast_shadows(phase==4)
            else:state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/('z07-'+views[phase//2]+'.png')),camera)
        elif phase==6 and elapsed>95:
            assert all((out/('z07-'+v+'.png')).exists() for v in views)
            for c in external:c.set_visibility(True)
            for c in local:c.set_cast_shadows(False)
            (out/'light-comparison.json').write_text(json.dumps(dict(status='unsaved_comparison',external_lights=names,views=views,qualification='Same camera, exposure and geometry; no map save or gameplay qualification.'),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
