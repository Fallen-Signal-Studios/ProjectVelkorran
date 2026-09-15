"""Base-color and final-color captures with smaller local fill emitters; no saves."""
from pathlib import Path
import time,json,unreal
root=Path(unreal.Paths.project_dir());SKIP_CARRIER_CAPTURE=True;REIMPORT_CARRIER_HULL=False
exec(compile((root/'Scripts/Editor/preview_carrier_refinement.py').read_text(),'carrier_capture_setup','exec'),globals())
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
visuals=[]
for a in actors:
    if a.get_actor_label() in carrier_labels:
        c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        visuals.append((c,c.get_editor_property('visible')));c.set_visibility('_Stable' in a.get_actor_label())
fills=[(c,c.source_width,c.source_height) for c in owner.get_components_by_class(unreal.RectLightComponent)]
cam=sub.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(6500,10000,2000),unreal.Rotator(pitch=-12,yaw=-55))
c=cam.get_component_by_class(unreal.SceneCaptureComponent2D)
c.set_editor_property('texture_target',unreal.RenderingLibrary.create_render_target2d(world,1600,900,unreal.TextureRenderTargetFormat.RTF_RGBA8))
c.set_editor_property('fov_angle',75.0);c.set_editor_property('capture_every_frame',True)
views=[('base-color',unreal.SceneCaptureSource.SCS_BASE_COLOR,1),('small-emitter',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,.1)]
state=dict(index=0,phase=0,next=time.monotonic()+15)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    for comp,v in visuals:comp.set_visibility(v)
    for light,w,h in fills:light.set_source_width(w);light.set_source_height(h)
    assert sub.destroy_actor(cam)
    assert len(sub.get_all_level_actors())==3140
def tick(delta):
    if time.monotonic()<state['next']:return
    try:
        if state['index']==len(views):
            restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        name,source,scale=views[state['index']]
        if state['phase']==0:
            c.set_editor_property('capture_source',source)
            for light,w,h in fills:light.set_source_width(w*scale);light.set_source_height(h*scale)
            state.update(phase=1,next=time.monotonic()+15)
        elif state['phase']==1:
            c.capture_scene()
            state.update(phase=2,next=time.monotonic()+1)
        else:
            unreal.RenderingLibrary.export_render_target(world,c.texture_target,str(out),name+'.png')
            state.update(index=state['index']+1,phase=0,next=time.monotonic()+1)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
handle=unreal.register_slate_post_tick_callback(tick)
