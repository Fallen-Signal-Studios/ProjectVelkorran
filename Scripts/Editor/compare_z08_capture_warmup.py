"""Unsaved fixed-camera comparison of high-res screenshot render warmup."""
from pathlib import Path
import hashlib,json,os,runpy,time,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);map_path=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
map_hash=hashlib.sha256(map_path.read_bytes()).hexdigest()
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(subsystem.get_all_level_actors());assert len(actors)==3140
snapshot=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))['snapshot_actor_state'];before=snapshot(actors)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
cvars={k:unreal.SystemLibrary.get_console_variable_float_value(k) for k in ('r.HighResScreenshotDelay','r.ScreenPercentage','r.AntiAliasingMethod','r.Nanite','r.Nanite.MaxPixelsPerEdge','r.Nanite.Streaming.StreamingPoolSize','r.Shadow.Virtual.Enable')}
(out/'capture-baseline.json').write_text(json.dumps(cvars,indent=2))
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,18900,-1035),unreal.Rotator(pitch=5,yaw=90));editor.editor_set_game_view(True)
views=[('entry-4',4,(0,18900,-1035),(5,90),90),('entry-32',32,(0,18900,-1035),(5,90),90),('entry-64',64,(0,18900,-1035),(5,90),90),('entry-return-4',4,(0,18900,-1035),(5,90),90),('detail-4',4,(-2330,19100,-1040),(-8,165),65),('detail-64',64,(-2330,19100,-1040),(-8,165),65)]
state=dict(index=0,phase=0,next=time.monotonic()+30,busy=False);records=[]
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    unreal.SystemLibrary.execute_console_command(world,'r.HighResScreenshotDelay '+str(int(cvars['r.HighResScreenshotDelay'])))
def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==len(views):
            restore();editor.eject_pilot_level_actor();assert subsystem.destroy_actor(camera)
            assert snapshot(actors)==before and len(subsystem.get_all_level_actors())==3140
            assert hashlib.sha256(map_path.read_bytes()).hexdigest()==map_hash
            assert all((out/(v[0]+'.png')).is_file() for v in views)
            (out/'capture-warmup-comparison.json').write_text(json.dumps(dict(status='unsaved_comparison',views=records,map_sha256=map_hash,preserved_actor_states=3140,qualification='Screenshot warmup isolation only; no runtime rendering or art acceptance'),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        name,frames,pos,rot,fov=views[state['index']]
        if state['phase']==0:
            unreal.SystemLibrary.execute_console_command(world,'r.HighResScreenshotDelay '+str(frames))
            camera.set_actor_location(unreal.Vector(*pos),False,False);camera.set_actor_rotation(unreal.Rotator(pitch=rot[0],yaw=rot[1]),False);camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov);editor.pilot_level_actor(camera)
            state.update(phase=1,next=time.monotonic()+20)
        else:
            assert unreal.SystemLibrary.get_console_variable_int_value('r.HighResScreenshotDelay')==frames
            records.append(dict(name=name,frames=frames,elapsed=time.monotonic()))
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(name+'.png')),camera)
            state.update(index=state['index']+1,phase=0,next=time.monotonic()+5)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
