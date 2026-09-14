"""Unsaved static-component ownership isolation for the blue shapes."""
from pathlib import Path
import json,os,time,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=subsystem.get_all_level_actors()
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-2450,19350,-920),unreal.Rotator(pitch=15,yaw=160));camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(75);editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
groups={k:[] for k in ['conduit05','scar05','conduit06','scar06','other_display','remaining']};inventory=[]
for actor in actors:
    label=actor.get_actor_label()
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if isinstance(c,unreal.InstancedStaticMeshComponent) or not c.static_mesh or not c.get_editor_property('visible'):continue
        if not label.startswith(('Z0','Z1')):continue
        key={'Z08__EclipseConduit_05':'conduit05','Z08__EclipseScar_05':'scar05','Z08__EclipseConduit_06':'conduit06','Z08__EclipseScar_06':'scar06'}.get(label)
        if not key:key='other_display' if any(c.get_material(i) and 'NativeDisplay' in c.get_material(i).get_path_name() for i in range(c.get_num_materials())) else 'remaining'
        groups[key].append(c)
        inventory.append(dict(group=key,actor=label,location=str(c.get_world_location()),transform=str(c.get_world_transform()),hidden_in_game=c.get_editor_property('hidden_in_game'),component=c.get_path_name(),mesh=c.static_mesh.get_path_name(),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]))
(out/'static-owner-inventory.json').write_text(json.dumps(inventory,indent=2))
views=[('west',mode) for mode in ['baseline']+list(groups)]
state=dict(index=0,phase='configure',next=time.monotonic()+15,busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    for components in groups.values():
        for c in components:c.set_visibility(True)
def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==len(views):
            restore();assert all((out/(k+'-'+m+'.png')).exists() for k,m in views)
            (out/'blue-owner-isolation.json').write_text(json.dumps(dict(status='captured',saved=False,views=views,group_counts={k:len(v) for k,v in groups.items()}),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        key,mode=views[state['index']]
        if state['phase']=='configure':
            restore()
            for c in groups.get(mode,[]):c.set_visibility(False)
            state.update(phase='capture',next=time.monotonic()+15)
        else:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(key+'-'+mode+'.png')),camera)
            state.update(index=state['index']+1,phase='configure',next=time.monotonic()+5)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
