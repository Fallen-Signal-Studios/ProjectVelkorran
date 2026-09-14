"""Unsaved category isolation for the west-wall blue shapes."""
from pathlib import Path
import json,os,time,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=subsystem.get_all_level_actors()
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-2450,19350,-920),unreal.Rotator(pitch=15,yaw=160));camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(75);editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
groups={'baseline':[],'editor':['CompositeEditorPrimitives','ModeWidgets','BillboardSprites'],'particles':['Particles'],'skeletal':['SkeletalMeshes'],'static':['StaticMeshes'],'text':['TextRender']}
original={name:unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.'+name) for names in groups.values() for name in names}
views=[('west',mode) for mode in groups]
state=dict(index=0,phase='configure',next=time.monotonic()+15,busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    for name,value in original.items():unreal.SystemLibrary.execute_console_command(world,'ShowFlag.'+name+' '+str(value))
def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==len(views):
            restore();assert all((out/(k+'-'+m+'.png')).exists() for k,m in views)
            (out/'blue-render-isolation.json').write_text(json.dumps(dict(status='captured',saved=False,views=views,original_show_flags=original),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        key,mode=views[state['index']]
        if state['phase']=='configure':
            restore()
            for name in groups[mode]:
                unreal.SystemLibrary.execute_console_command(world,'ShowFlag.'+name+' 0')
                assert unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.'+name)==0
            state.update(phase='capture',next=time.monotonic()+15)
        else:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(key+'-'+mode+'.png')),camera)
            state.update(index=state['index']+1,phase='configure',next=time.monotonic()+5)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
