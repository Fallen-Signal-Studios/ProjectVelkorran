"""Fit two authored treatment trolleys; optionally save after checks."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/Z08MedicalKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
runpy.run_path(str(root/'Scripts/Editor/audit_z08_cabinet.py'))
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
old=json.loads((source/'bed-baseline.json').read_text());a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert not json.loads((source/'coplanar-faces.json').read_text())['overlaps']
assert c.static_mesh.get_path_name()==old['mesh'] and c.get_instance_count()==2
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(2)]==[r['transform'] for r in old['instances']]
dest='/Game/Aurelion/Environment/ArchitectureKit';spec=json.loads((source/'manifest.json').read_text())['modules'][0];asset=dest+'/Meshes/'+spec['asset']
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_MedicalUpholstery':'MedicalUpholstery'}.items()}
path=materials['M_Aurelion_MedicalUpholstery'];lib=unreal.MaterialEditingLibrary
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AurelionKit_MedicalUpholstery',dest+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    color=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);color.set_editor_property('constant',unreal.LinearColor(.035,.075,.072,1));assert lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.82);assert lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    metal=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);metal.set_editor_property('r',0);assert lib.connect_material_property(metal,'',unreal.MaterialProperty.MP_METALLIC)
    mat.set_editor_property('used_with_nanite',True);lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
mesh=unreal.load_asset(asset) if globals().get('PERSIST_Z08_BEDS',False) and unreal.EditorAssetLibrary.does_asset_exist(asset) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(spec['asset']+'.fbx')).resolve()
transform=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'))['rail_transform']
c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
for row in old['instances']:
    prior=transform(row);t=unreal.Transform();t.rotation=prior.rotation;t.scale3d=unreal.Vector(1,1,1)
    t.translation=unreal.MathLibrary.transform_location(prior,unreal.Vector(old['mesh_origin'][0],old['mesh_origin'][1],old['mesh_origin'][2]-old['mesh_extent'][2]));c.add_instance(t,world_space=True)
assert helpers['snapshot_actor_state'](actors)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z08_beds.py'))['check_z08_beds'](actors)
persist=bool(globals().get('PERSIST_Z08_BEDS',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-bed-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_actor_states=len(actors)),indent=2))
exec(compile((root/'Scripts/Editor/review_z08_beds.py').read_text(),'medical_capture','exec'),globals())
