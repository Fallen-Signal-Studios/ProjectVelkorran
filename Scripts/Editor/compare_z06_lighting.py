"""Unsaved fixed-camera comparison of current and reduced local room lighting."""
import json,os,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());labels={a.get_actor_label():a for a in actors};assert len(actors)==2797
baseline=json.loads((root/'Art/Source/Aurelion/Z06WallFit/lighting-baseline.json').read_text())['lights'];lights=[]
for row in baseline:
    c=labels[row['actor']].get_component_by_class(unreal.RectLightComponent)
    assert c.get_world_transform().export_text()==row['transform'] and c.get_editor_property('intensity')==row['intensity'] and c.get_editor_property('attenuation_radius')==row['radius']
    assert c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
    lights.append(c)
p=labels['Z06_Entry_StandIn'].get_actor_location()
views=[('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90)),('detail',unreal.Vector(-600,8000,-435),unreal.Rotator(pitch=10,yaw=-145))]
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,views[0][1],views[0][2]);camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(90)
editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
state=dict(index=0,start=time.monotonic(),busy=False,configured=False);unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def restore():
    for c,row in zip(lights,baseline):c.set_intensity(row['intensity']);c.set_attenuation_radius(row['radius'])

def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==4:
            restore()
            assert all(c.get_editor_property('intensity')==r['intensity'] and c.get_editor_property('attenuation_radius')==r['radius'] for c,r in zip(lights,baseline))
            assert all((out/f'{setting}-{view[0]}.png').exists() for setting in ('baseline','reduced') for view in views)
            (out/'comparison.json').write_text(json.dumps(dict(status='captured',baseline=baseline,candidate=dict(lumens=600,radius_cm=3000),restored=True,qualification='Unsaved fixed-camera comparison; moving effects may differ. No gameplay or performance acceptance.'),indent=2))
            unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        index=state['index'];name,pos,rot=views[index%2]
        if not state['configured']:
            for c,r in zip(lights,baseline):c.set_intensity(r['intensity'] if index<2 else 600);c.set_attenuation_radius(r['radius'] if index<2 else 3000)
            camera.set_actor_location(pos,False,False);camera.set_actor_rotation(rot,False);editor.pilot_level_actor(camera)
            state['configured']=True;state['start']=time.monotonic();return
        if time.monotonic()-state['start']>25:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/f'{"baseline" if index<2 else "reduced"}-{name}.png'),camera)
            state['index']+=1;state['configured']=False
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(state['handle']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
