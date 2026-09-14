"""Fit one authored medical sideboard; optionally save after checks."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/Z08CabinetKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140

helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
old=json.loads((source/'cabinet-baseline.json').read_text());a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert not json.loads((source/'coplanar-faces.json').read_text())['overlaps']
assert c.static_mesh.get_path_name()==old['mesh'] and c.get_instance_count()==1
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(1)]==[r['transform'] for r in old['instances']]
dest='/Game/Aurelion/Environment/ArchitectureKit';spec=json.loads((source/'manifest.json').read_text())['modules'][0];asset=dest+'/Meshes/'+spec['asset']
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_MedicalUpholstery':'MedicalUpholstery'}.items()}
path=materials['M_Aurelion_MedicalUpholstery'];lib=unreal.MaterialEditingLibrary
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AurelionKit_MedicalUpholstery',dest+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    color=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);color.set_editor_property('constant',unreal.LinearColor(.035,.075,.072,1));assert lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.82);assert lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    metal=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);metal.set_editor_property('r',0);assert lib.connect_material_property(metal,'',unreal.MaterialProperty.MP_METALLIC)
    mat.set_editor_property('used_with_nanite',True);lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
mat=unreal.load_asset(path);mat.set_editor_property('used_with_instanced_static_meshes',True);lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
mesh=unreal.load_asset(asset) if globals().get('PERSIST_Z08_CABINET',False) and unreal.EditorAssetLibrary.does_asset_exist(asset) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(spec['asset']+'.fbx')).resolve()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
placement=json.loads((source/'placement.json').read_text())
# A slightly inset stationary sweep rejects existing physical obstructions without touching the floor.
def hit(raw):
    if raw is None:return dict(blocked=False,actor=None)
    values=[v for v in raw if isinstance(v,unreal.HitResult)] if isinstance(raw,tuple) else [raw]
    assert len(values)==1 and isinstance(values[0],unreal.HitResult)
    p=values[0].to_tuple()
    return dict(blocked=bool(p[0]),actor=p[9].get_actor_label() if p[9] else None,impact=p[5].export_text())
clearance=hit(unreal.SystemLibrary.box_trace_single(world,unreal.Vector(-2600,22780,-1160),unreal.Vector(-2600,22780,-1159.99),unreal.Vector(59,49,39),unreal.Rotator(),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[a],unreal.DrawDebugTrace.NONE,True))
(out/'placement-clearance.json').write_text(json.dumps(clearance,indent=2));assert not clearance['blocked'],clearance
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
if sm.get_simple_collision_count(mesh)==0:assert sm.add_simple_collisions(mesh,unreal.ScriptingCollisionShapeType.BOX)>=0
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
t=unreal.Transform(location=unreal.Vector(*placement['location_cm']),rotation=unreal.Rotator(yaw=placement['yaw_degrees']),scale=unreal.Vector(1,1,1));c.add_instance(t,world_space=True)
c.set_collision_profile_name('BlockAll');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
after=helpers['snapshot_actor_state'](actors)
assert {k:v for k,v in after.items() if k!=a.get_path_name()}=={k:v for k,v in before.items() if k!=a.get_path_name()}
result=runpy.run_path(str(root/'Scripts/Editor/check_z08_cabinet.py'))['check_z08_cabinet'](actors)
unreal.SovAurelionNavigationLibrary.build_navigation(world,unreal.Vector(0,13000,-500),unreal.Vector(14000,38000,3000))
import time
nav_started=time.monotonic();unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def finish_fit(delta):
    if time.monotonic()-nav_started<15:return
    if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
        if time.monotonic()-nav_started>=240:
            unreal.unregister_slate_post_tick_callback(nav_handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            raise RuntimeError('Navigation rebuild timed out')
        return
    unreal.unregister_slate_post_tick_callback(nav_handle)
    try:
        result['access']=runpy.run_path(str(root/'Scripts/Editor/check_z08_cabinet.py'))['check_cabinet_access'](actors)
        paths=result['access']['paths'];(out/'placement-navigation.json').write_text(json.dumps(paths,indent=2))
        persist=bool(globals().get('PERSIST_Z08_CABINET',False))
        if persist:
            shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
        (out/'z08-cabinet-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_actor_states=len(actors)-1,clearance=clearance,paths=paths),indent=2))
        exec(compile((root/'Scripts/Editor/review_z08_cabinet.py').read_text(),'medical_capture','exec'),globals())
    except Exception:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
nav_handle=unreal.register_slate_post_tick_callback(finish_fit)
