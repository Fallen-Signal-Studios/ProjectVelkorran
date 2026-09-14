"""Unsaved same-camera comparison of automatic and fine Nanite position precision."""
import json,os,time
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
names=['Z02ArchFace','Z02SideVault_2m','Z02SideVault_Edge','Z02Perimeter_4m','Z02EndFrieze']
meshes=[unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_'+n) for n in names];assert all(meshes)
previous=[sm.get_nanite_settings(m) for m in meshes]
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-6920,-9220,170),unreal.Rotator(pitch=24,yaw=160))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(85);editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
state=dict(phase=0,start=time.monotonic(),busy=False);rows=[]
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def restore():
    for mesh,settings in zip(meshes,previous):sm.set_nanite_settings(mesh,settings,True)

def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        elapsed=time.monotonic()-state['start']
        if state['phase']==0 and elapsed>25:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/'default-precision.png'),camera);state['phase']=1
        elif state['phase']==1:
            for mesh,old in zip(meshes,previous):
                settings=sm.get_nanite_settings(mesh);settings.set_editor_property('position_precision',6)
                sm.set_nanite_settings(mesh,settings,True)
                rows.append(dict(mesh=mesh.get_path_name(),before=old.export_text(),after=sm.get_nanite_settings(mesh).export_text()))
            state['phase']=2;state['start']=time.monotonic()
        elif state['phase']==2 and elapsed>30:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/'fine-precision.png'),camera);state['phase']=3
        elif state['phase']==3:
            restore()
            assert all(sm.get_nanite_settings(m).export_text()==old.export_text() for m,old in zip(meshes,previous))
            assert (out/'default-precision.png').exists() and (out/'fine-precision.png').exists()
            (out/'comparison.json').write_text(json.dumps(dict(status='captured',meshes=rows,scope='Unsaved same-camera rendering experiment; settings restored in memory; no map or assets saved. Visual assessment required.'),indent=2))
            unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
