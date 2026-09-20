"""Unsaved same-camera shadow comparison and a read-only chamber light census."""
import json,os,time,unreal
from pathlib import Path
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rows=[]
for actor in actors.get_all_level_actors():
    for c in actor.get_components_by_class(unreal.LightComponent):
        p=c.get_world_location()
        if not isinstance(c,unreal.DirectionalLightComponent) and not (abs(p.x)<8000 and 29000<p.y<40500):continue
        row=dict(actor=actor.get_actor_label(),component=c.get_name(),type=c.get_class().get_name(),position=p.export_text(),properties={})
        for key in ('intensity','intensity_units','attenuation_radius','source_radius','soft_source_radius','source_angle','source_width','source_height','cast_shadows','shadow_resolution_scale','contact_shadow_length','inner_cone_angle','outer_cone_angle'):
            try:row['properties'][key]=str(c.get_editor_property(key))
            except Exception:pass
        rows.append(row)
variables=['r.Shadow.Virtual.Enable','r.ShadowQuality','r.Shadow.Virtual.ResolutionLodBiasLocal','r.Shadow.Virtual.SMRT.RayCountLocal','r.AntiAliasingMethod','r.ScreenPercentage','r.HighResScreenshotDelay']
variables+=globals().get('EXTRA_COMPARISON_VARIABLES',[])
values={n:unreal.SystemLibrary.get_console_variable_float_value(n) for n in variables}
(out/'lighting-census.json').write_text(json.dumps(dict(lights=rows,cvars=values),indent=2))
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(*globals().get('COMPARISON_POSITION',(0,33600,-1570))),unreal.Rotator(yaw=globals().get('COMPARISON_YAW',90)))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(80)
level.editor_set_game_view(True);level.pilot_level_actor(camera)
cases=globals().get('SHADOW_COMPARISON_CASES',[('baseline',{}),('higher-shadow-resolution',{'r.Shadow.Virtual.ResolutionLodBiasLocal':-2}),('shadows-disabled',{'r.ShadowQuality':0})])
controlled=set().union(*(overrides.keys() for _,overrides in cases))
state=dict(index=0,next=time.monotonic()+12,stage=0,busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def finish():
    for name,value in values.items():
        if name in controlled:unreal.SystemLibrary.execute_console_command(world,name+' '+str(value))
    level.eject_pilot_level_actor();actors.destroy_actor(camera)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==len(cases):
            assert all((out/(name+'.png')).exists() for name,_ in cases)
            (out/'comparison.json').write_text(json.dumps(dict(status='captured',scope=__doc__,map_saved=False),indent=2))
            finish();return
        name,overrides=cases[state['index']]
        if state['stage']==0:
            for var in controlled:
                unreal.SystemLibrary.execute_console_command(world,var+' '+str(overrides.get(var,values[var])))
            state.update(stage=1,next=time.monotonic()+10)
        else:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(name+'.png')),camera)
            state.update(index=state['index']+1,stage=0,next=time.monotonic()+2)
    except Exception:
        finish();raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
