"""Review the saved custom kit from assembly, player-height and detail cameras."""
import json
import os
from pathlib import Path
import time
import unreal
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert editor.load_level('/Game/Aurelion/ArtReview/L_Aurelion_ArchitectureKit')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rows=[]
for a in actors.get_all_level_actors():
    if a.get_actor_label().startswith('KIT_REVIEW_') and isinstance(a,unreal.StaticMeshActor):
        a.set_actor_rotation(unreal.Rotator(yaw=180),False)
        rows.append(dict(label=a.get_actor_label(),rotation=a.get_actor_rotation().export_text()))
existing=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.CameraActor) and a.get_actor_label()=='KIT_REVIEW_Camera']
assert len(existing)<=1
camera=existing[0] if existing else actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(850,-1400,620))
camera.set_actor_location(unreal.Vector(850,-1400,620),False,False)
camera.set_actor_label('KIT_REVIEW_Camera')
component=camera.get_component_by_class(unreal.CameraComponent)
component.set_field_of_view(50)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,330)),False)
assert editor.save_current_level()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'r.HighResScreenshotDelay 32')
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for a in actors.get_all_level_actors():
    if a.get_actor_label().startswith('KIT_REVIEW_') and isinstance(a,unreal.StaticMeshActor):
        assert sm.get_num_uv_channels(a.static_mesh_component.static_mesh,0)==2
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
(out/'review-state.json').write_text(json.dumps(dict(assembly_orientation=rows,scope='Saved review map only'),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.monotonic(),phase=0)
views=[('assembly',(850,-1400,620),(0,0,330),50),
       ('player',(-140,-650,165),(0,0,340),70),
       ('detail',(-160,-280,510),(-180,0,525),55)]
def tick(delta):
    if state.get('busy'):
        return
    state['busy']=True
    try:
        elapsed=time.monotonic()-state['start']
        phase=state['phase']
        if state.get('task') and not state['task'].is_task_done():
            return
        if phase<6 and elapsed>15+phase*10:
            index=phase//2; name,pos,target,fov=views[index]
            if phase%2==0:
                camera.set_actor_location(unreal.Vector(*pos),False,False)
                camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*pos),unreal.Vector(*target)),False)
                component.set_field_of_view(fov)
                editor.pilot_level_actor(camera)
            else:
                state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,1000,str(out/('kit-'+name+'.png')),camera)
            state['phase']+=1
        elif phase==6 and elapsed>80:
            assert all((out/('kit-'+v[0]+'.png')).exists() for v in views), 'Capture set incomplete'
            (out/'capture-complete.json').write_text(json.dumps(dict(views=[v[0] for v in views],
                imported_uv_channels=2,scope='Editor art review; no gameplay qualification'),indent=2))
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise
    finally:
        state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
