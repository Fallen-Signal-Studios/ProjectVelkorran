"""Unsaved fixed-camera comparison of two repurposed ceiling-bounce lights."""
from pathlib import Path
import json,os,time,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());assert len(actors)==3140
labels={a.get_actor_label():a for a in actors};lights=[labels['ENVL_Z08_CeilingBounce_'+str(side)] for side in (-1,1)];original=[]
for a in lights:
    c=a.get_component_by_class(unreal.RectLightComponent);color=c.get_light_color()
    original.append(dict(actor=a.get_actor_label(),transform=a.get_actor_transform().export_text(),position=[a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z],linear_color=[color.r,color.g,color.b,color.a],properties={k:str(c.get_editor_property(k)) for k in ('intensity','intensity_units','attenuation_radius','cast_shadows','source_width','source_height','indirect_lighting_intensity','volumetric_scattering_intensity')}))
(out/'deck-light-baseline.json').write_text(json.dumps(original,indent=2));transforms=[a.get_actor_transform() for a in lights]
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(2200,20400,-735),unreal.Rotator(pitch=-5,yaw=-90));editor.editor_set_game_view(True)
views=[(name,mode,pos,rot,fov) for name,pos,rot,fov in [('landing',(2200,20400,-735),(-5,-90),85),('entry',(0,18900,-1035),(5,90),90)] for mode in ('baseline','down-25000','down-60000')]
state=dict(index=0,phase=0,next=time.monotonic()+15,busy=False);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    for a,t,row in zip(lights,transforms,original):
        a.set_actor_transform(t,False,False);c=a.get_component_by_class(unreal.RectLightComponent);c.set_intensity(float(row['properties']['intensity']));c.set_attenuation_radius(float(row['properties']['attenuation_radius']))
def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==len(views):
            restore();assert all((out/(n+'-'+m+'.png')).is_file() for n,m,*_ in views)
            (out/'deck-light-comparison.json').write_text(json.dumps(dict(status='unsaved_comparison',views=[n+'-'+m for n,m,*_ in views],qualification='Lighting candidates only; no map save or gameplay acceptance'),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        name,mode,pos,rot,fov=views[state['index']]
        if state['phase']==0:
            restore()
            if mode!='baseline':
                for a in lights:
                    p=a.get_actor_location();a.set_actor_location(unreal.Vector(p.x,p.y,850),False,False);a.set_actor_rotation(unreal.Rotator(pitch=-90),False)
                    c=a.get_component_by_class(unreal.RectLightComponent);c.set_intensity(float(mode.split('-')[1]));c.set_attenuation_radius(4500)
            camera.set_actor_location(unreal.Vector(*pos),False,False);camera.set_actor_rotation(unreal.Rotator(pitch=rot[0],yaw=rot[1]),False);camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov);editor.pilot_level_actor(camera)
            state.update(phase=1,next=time.monotonic()+15)
        else:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(name+'-'+mode+'.png')),camera);state.update(index=state['index']+1,phase=0,next=time.monotonic()+5)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
