"""Unsaved isolation of the violet key and stone normal strength."""
from pathlib import Path
import json,os,time,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=subsystem.get_all_level_actors()
surfaces={}
for key,mesh in [('e4','SM_Aurelion_KIT_Z08WallAssembly'),('e3','SM_Aurelion_KIT_Z06ClimbPanel')]:
    matches=[c for a in actors for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name()==mesh and c.get_editor_property('visible')]
    assert len(matches)==1;surfaces[key]=matches[0]
original={k:list(c.get_editor_property('override_materials')) for k,c in surfaces.items()}
light=next(a for a in actors if a.get_actor_label()=='ENVL_Z08_Key_03').get_component_by_class(unreal.LightComponent)
color_before=light.get_light_color();intensity_before=light.intensity
stone=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingIvory')
lib=unreal.MaterialEditingLibrary;normal=lib.get_material_property_input_node(stone,unreal.MaterialProperty.MP_NORMAL)
assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate);normal_before=normal.get_editor_property('const_alpha')
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(1770,21830,-1020),unreal.Rotator(yaw=165));editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
shadow_var='r.ShadowQuality';shadow_before=unreal.SystemLibrary.get_console_variable_int_value(shadow_var)
views=[('e4','violet-off'),('e4','neutral-key'),('e3','normal-zero'),('e3','normal-003')]
state=dict(index=0,phase='configure',next=time.monotonic()+15,busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    light.set_light_color(color_before);light.set_intensity(intensity_before)
    normal.set_editor_property('const_alpha',normal_before);lib.recompile_material(stone)
def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==len(views):
            restore();assert all((out/(k+'-'+m+'.png')).exists() for k,m in views)
            (out/'wall-input-comparison.json').write_text(json.dumps(dict(status='captured',saved=False,views=views,shadow_before=shadow_before,normal_before=normal_before,light_intensity=intensity_before),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        key,mode=views[state['index']]
        if state['phase']=='configure':
            restore()
            if mode=='violet-off':light.set_intensity(0)
            if mode=='neutral-key':light.set_light_color(unreal.LinearColor(.887923,.913099,1.,1.))
            if mode.startswith('normal-'):
                normal.set_editor_property('const_alpha',0. if mode=='normal-zero' else .03);lib.recompile_material(stone)
            pos,rot,fov=(unreal.Vector(1770,21830,-1020),unreal.Rotator(yaw=165),80) if key=='e4' else (unreal.Vector(-900,9370,-440),unreal.Rotator(yaw=170),85)
            camera.set_actor_location(pos,False,False);camera.set_actor_rotation(rot,False);camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov);editor.pilot_level_actor(camera)
            state.update(phase='capture',next=time.monotonic()+15)
        else:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/(key+'-'+mode+'.png')),camera)
            state.update(index=state['index']+1,phase='configure',next=time.monotonic()+5)
    except Exception:
        restore();unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
