"""Unsaved base-color isolation of a gallery baffle and its raster fallback."""
from pathlib import Path
import json,os,time,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(sub.get_all_level_actors());assert len(actors)==3140
prior=[(c,c.get_editor_property('visible')) for a in actors for c in a.get_components_by_class(unreal.StaticMeshComponent)]
mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z09Baffle');assert mesh
temporary=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-500,26810,-1500),unreal.Rotator())
tc=temporary.static_mesh_component;tc.set_static_mesh(mesh);tc.set_collision_profile_name('NoCollision');tc.set_visibility(False)
cam=sub.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(-300,25750,-1330),unreal.Rotator(pitch=12,yaw=90))
c=cam.get_component_by_class(unreal.SceneCaptureComponent2D)
c.set_editor_property('texture_target',unreal.RenderingLibrary.create_render_target2d(world,1600,900,unreal.TextureRenderTargetFormat.RTF_RGBA8))
c.set_editor_property('fov_angle',80.0);c.set_editor_property('capture_every_frame',False)
c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_BASE_COLOR)
views=globals().get('SURFACE_VIEWS',['context','isolated','isolated-fallback']);state=dict(index=0,phase=0,next=time.monotonic()+15)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    for comp,visible in prior:comp.set_visibility(visible)
    assert sub.destroy_actor(temporary) and sub.destroy_actor(cam)
    assert len(sub.get_all_level_actors())==3140
def tick(delta):
    if time.monotonic()<state['next']:return
    try:
        if state['index']==len(views):
            restore()
            (out/'surface-isolation.json').write_text(json.dumps(dict(status='captured',views=views,source_mesh=mesh.get_path_name(),actor_count=3140,qualification='Base color diagnostic; no map or asset saves. Isolated mode hides all original static mesh components and uses one temporary baffle at its saved pose.'),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        name=views[state['index']]
        if state['phase']==0:
            close=name=='isolated-close';scale=2 if name=='isolated-2x' else 1
            cam.set_actor_location(unreal.Vector(-300,26350 if close else 25750,-1250 if close else -1330),False,False)
            cam.set_actor_rotation(unreal.Rotator(pitch=0 if close else 12,yaw=90),False)
            c.set_editor_property('texture_target',unreal.RenderingLibrary.create_render_target2d(world,1600*scale,900*scale,unreal.TextureRenderTargetFormat.RTF_RGBA8))
            for comp,visible in prior:comp.set_visibility(visible if name=='context' else False)
            tc.set_visibility(name!='context');tc.set_editor_property('disallow_nanite',name=='isolated-fallback')
            state.update(phase=1,next=time.monotonic()+15)
        elif state['phase']==1:
            c.capture_scene();state.update(phase=2,next=time.monotonic()+1)
        else:
            unreal.RenderingLibrary.export_render_target(world,c.texture_target,str(out),name+'.png')
            state.update(index=state['index']+1,phase=0,next=time.monotonic()+1)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
handle=unreal.register_slate_post_tick_callback(tick)
