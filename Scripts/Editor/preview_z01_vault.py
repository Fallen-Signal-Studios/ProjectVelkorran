"""Import the fitted vault and preview it in Z01 without saving the campaign map."""
import json
import os
from pathlib import Path
import time
import shutil
import unreal
persist=bool(globals().get('PERSIST',False))
root=Path(unreal.Paths.project_dir()).resolve()
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original=list(subsystem.get_all_level_actors())
by_label={a.get_actor_label():a for a in original}
source=root/'Art/Source/Aurelion/VaultKit'
def import_module(spec):
    name=spec['asset']
    destination='/Game/Aurelion/Environment/ArchitectureKit/Meshes'
    asset=destination+'/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(asset):
        old=unreal.load_asset(asset)
        assert Path(old.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(name+'.fbx')).resolve()
    task=unreal.AssetImportTask(); task.filename=str(source/(name+'.fbx')); task.destination_path=destination
    task.destination_name=name; task.automated=True; task.save=False; task.replace_existing=True; task.factory=unreal.FbxFactory()
    options=unreal.FbxImportUI(); options.import_mesh=True; options.import_as_skeletal=False
    options.import_materials=False; options.import_textures=False; options.automated_import_should_detect_type=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes=True; options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.generate_lightmap_u_vs=False
    options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task.options=options; unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(asset); assert mesh
    materials={'M_Aurelion_IvoryStone':'Ivory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        key=str(slot.get_editor_property('imported_material_slot_name'))
        material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_'+materials[key])
        assert material; mesh.set_material(i,material)
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert sm.get_simple_collision_count(mesh)==0
    assert sm.get_num_uv_channels(mesh,0)==2
    extent=mesh.get_bounds().box_extent
    expected=spec['nominal_dimensions_m']
    assert all(abs(a-b*100)<1 for a,b in zip((extent.x*2,extent.y*2,extent.z*2),expected))
    mesh.set_editor_property('light_map_coordinate_index',1)
    nanite=sm.get_nanite_settings(mesh); nanite.set_editor_property('enabled',True); sm.set_nanite_settings(mesh,nanite,True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh
meshes={spec['asset']:import_module(spec) for spec in json.loads((source/'manifest.json').read_text())['modules']}
mesh=meshes['SM_Aurelion_KIT_Vault_25x4']
asset=mesh.get_path_name()
before={a.get_path_name():(a.get_actor_transform().export_text(),a.get_actor_enable_collision()) for a in original}
assert not any(a.get_actor_label().startswith(('KIT_Z01_Vault_','KIT_Z01_VaultEnd_')) for a in original), 'Vault already placed; review existing placements before replacing'
component_collision={c.get_path_name():str(c.get_collision_enabled()) for a in original for c in a.get_components_by_class(unreal.PrimitiveComponent)}
for i in range(14):
    a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-7010,-17300+400*i,695))
    a.set_actor_label('KIT_Z01_Vault_'+str(i).zfill(2)); a.set_folder_path('Aurelion/CustomArchitecture/Z01/Vault')
    a.static_mesh_component.set_static_mesh(mesh)
    a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); a.set_actor_enable_collision(False)
for i,y in enumerate((-17480,-11920)):
    a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-7010,y,695),unreal.Rotator(yaw=180 if i==1 else 0))
    a.set_actor_label('KIT_Z01_VaultEnd_'+str(i)); a.set_folder_path('Aurelion/CustomArchitecture/Z01/Vault')
    a.static_mesh_component.set_static_mesh(meshes['SM_Aurelion_KIT_VaultTympanum_25m'])
    a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); a.set_actor_enable_collision(False)
upper=by_label['KIT_Z01_UpperEnclosure'].static_mesh_component
upper.set_visibility(False,False); upper.set_hidden_in_game(True,False)
# The old suspended mechanical runners obscure the proposed architectural ceiling.
# Preview visibility only: their collision and actor transforms remain unchanged.
overhead=by_label['things'].static_mesh_component
overhead.set_visibility(False,False); overhead.set_hidden_in_game(True,False)
assert before=={a.get_path_name():(a.get_actor_transform().export_text(),a.get_actor_enable_collision()) for a in original}
assert component_collision=={c.get_path_name():str(c.get_collision_enabled()) for a in original for c in a.get_components_by_class(unreal.PrimitiveComponent)}
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap')
    assert editor.save_current_level()
(out/'vault-preview.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',module=asset,instances=14,end_panels=2,
    original_actor_count=len(original),retained_upper_shell=False,overhead_runners_hidden=True,
    qualification='Visual fit only; no mission, navigation or performance acceptance'),indent=2))
# Reuse the established entry/wall capture sequence; it contains no map save operations.
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
exec(compile("p=by_label['Z01_Entry_StandIn']"+capture,'z01_vault_capture','exec'))
