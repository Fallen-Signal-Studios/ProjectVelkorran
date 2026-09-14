"""Unsaved same-camera comparison of default and full Nanite fallback geometry."""
import json,os,time
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
defaults={'r.Nanite.MaxPixelsPerEdge':unreal.SystemLibrary.get_console_variable_float_value('r.Nanite.MaxPixelsPerEdge'),'r.ShadowQuality':unreal.SystemLibrary.get_console_variable_int_value('r.ShadowQuality')}
scenarios=[('default',{}),('fine-nanite-detail',{'r.Nanite.MaxPixelsPerEdge':.125}),('shadows-disabled',{'r.ShadowQuality':0})]
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-6920,-9220,170),unreal.Rotator(pitch=24,yaw=160))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(85);editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
state=dict(phase=0,start=time.monotonic(),busy=False);rows=[]
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def restore():
    for key,value in defaults.items():unreal.SystemLibrary.execute_console_command(world,f'{key} {value}')

def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        elapsed=time.monotonic()-state['start'];phase=state['phase']
        if phase>=len(scenarios)*2:
            restore()
            (out/'comparison.json').write_text(json.dumps(dict(status='captured',defaults=defaults,scenarios=scenarios,scope='Temporary rendering console settings restored; no map or assets saved. Visual assessment required.'),indent=2))
            unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        elif phase%2==0:
            restore()
            for key,value in scenarios[phase//2][1].items():unreal.SystemLibrary.execute_console_command(world,f'{key} {value}')
            state['start']=time.monotonic();state['phase']+=1
        elif elapsed>25:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(scenarios[phase//2][0]+'.png')),camera);state['phase']+=1
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
