"""Three fixed cameras for mounted Eclipse overlays, no saves."""
from pathlib import Path
import os,time,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-2450,19350,-920),unreal.Rotator(pitch=15,yaw=160));editor.editor_set_game_view(True)
views=[('west-context',(-2450,19350,-920),(15,160),75),('west-detail',(-3300,19300,-825),(0,180),60),('east-context',(2450,21550,-920),(15,20),75)]
state=dict(index=0,phase=0,next=time.monotonic()+15,busy=False);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==len(views):
            assert all((out/(row[0]+'.png')).is_file() for row in views)
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        name,pos,rot,fov=views[state['index']]
        if state['phase']==0:
            camera.set_actor_location(unreal.Vector(*pos),False,False);camera.set_actor_rotation(unreal.Rotator(pitch=rot[0],yaw=rot[1]),False);camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov);editor.pilot_level_actor(camera)
            state.update(phase=1,next=time.monotonic()+15)
        else:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(name+'.png')),camera);state.update(index=state['index']+1,phase=0,next=time.monotonic()+5)
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
