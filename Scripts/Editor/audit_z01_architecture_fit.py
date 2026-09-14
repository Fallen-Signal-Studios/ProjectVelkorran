"""Current Z01 geometry/instance census and entry view; no map save or gameplay changes."""
import json
import os
from pathlib import Path
import time
import unreal
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
def inside(p): return -8450<=p.x<=-5550 and -17650<=p.y<=-11750 and -100<=p.z<=1400
rows=[]
for a in actors.get_all_level_actors():
    label=a.get_actor_label()
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        mesh=c.static_mesh
        if not mesh: continue
        instances=[]
        if isinstance(c,unreal.InstancedStaticMeshComponent):
            for i in range(c.get_instance_count()):
                t=c.get_instance_transform(i,world_space=True)
                if inside(t.translation): instances.append(dict(index=i,transform=t.export_text()))
            if not instances: continue
        elif not (inside(c.get_world_location()) or label.startswith('Z01_')): continue
        origin,extent=a.get_actor_bounds(False)
        rows.append(dict(actor=label,path=a.get_path_name(),component=c.get_name(),mesh=mesh.get_path_name(),
            transform=c.get_world_transform().export_text(),bounds_origin=origin.export_text(),bounds_extent=extent.export_text(),
            hidden=a.get_editor_property('hidden'),visible=c.get_editor_property('visible'),
            hidden_in_game=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_enabled()),
            materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],
            instances=instances))
(out/'z01-current.json').write_text(json.dumps(dict(scope='Current authored Z01 census',components=rows),indent=2))
marks=[a for a in actors.get_all_level_actors() if a.get_actor_label()=='Z01_Entry_StandIn']
assert len(marks)==1
p=marks[0].get_actor_location(); eye=unreal.Vector(p.x,p.y,p.z+165)
camera=actors.spawn_actor_from_class(unreal.CameraActor,eye,unreal.Rotator(yaw=90))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(90)
editor.pilot_level_actor(camera)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.monotonic(),phase=0,busy=False)
def tick(delta):
    if state['busy']: return
    state['busy']=True
    try:
        if state['phase']==0 and time.monotonic()-state['start']>20:
            state['phase']=1
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/'z01-before.png'),camera)
        elif state['phase']==1 and state.get('task') and state['task'].is_task_done():
            assert (out/'z01-before.png').exists()
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise
    finally: state['busy']=False
state['handle']=unreal.register_slate_post_tick_callback(tick)
