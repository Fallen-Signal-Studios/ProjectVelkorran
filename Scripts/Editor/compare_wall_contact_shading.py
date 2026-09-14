"""Unsaved constant-material and shadow comparisons at fixed climb-wall cameras."""
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
material=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_WallComparison_Unsaved','/Game/Developers/Diagnostics',unreal.Material,unreal.MaterialFactoryNew())
assert material;material.set_editor_property('used_with_nanite',True)
lib=unreal.MaterialEditingLibrary
color=lib.create_material_expression(material,unreal.MaterialExpressionConstant3Vector);color.set_editor_property('constant',unreal.LinearColor(.5,.5,.5,1));assert lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,1),(unreal.MaterialProperty.MP_METALLIC,0),(unreal.MaterialProperty.MP_SPECULAR,0)]:
    node=lib.create_material_expression(material,unreal.MaterialExpressionConstant);node.set_editor_property('r',value);assert lib.connect_material_property(node,'',prop)
lib.recompile_material(material)
camera=subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(1770,21830,-1020),unreal.Rotator(yaw=165));editor.editor_set_game_view(True);editor.pilot_level_actor(camera)
shadow_var='r.ShadowQuality';shadow_before=unreal.SystemLibrary.get_console_variable_int_value(shadow_var)
views=[(key,mode) for key in ['e4','e3'] for mode in ['authored','constant','constant-no-shadows']]
state=dict(index=0,phase='configure',next=time.monotonic()+15,busy=False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def restore():
    for k,c in surfaces.items():c.set_editor_property('override_materials',original[k])
    unreal.SystemLibrary.execute_console_command(world,shadow_var+' '+str(shadow_before))
def tick(delta):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        if state.get('task') and not state['task'].is_task_done():return
        if state['index']==len(views):
            restore();assert all((out/(k+'-'+m+'.png')).exists() for k,m in views)
            (out/'wall-shading-comparison.json').write_text(json.dumps(dict(status='captured',saved=False,views=views,shadow_before=shadow_before),indent=2))
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        key,mode=views[state['index']]
        if state['phase']=='configure':
            restore()
            if mode!='authored':
                c=surfaces[key]
                for i in range(c.get_num_materials()):c.set_material(i,material)
            if mode=='constant-no-shadows':unreal.SystemLibrary.execute_console_command(world,shadow_var+' 0')
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
