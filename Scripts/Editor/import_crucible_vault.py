"""Import the owned upper enclosure; no map edits."""
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[2]
name='SM_Aurelion_CrucibleVault'; destination='/Game/Aurelion/Environment/Blender'
source=root/'Art/Source/Aurelion'/(name+'.fbx')
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
if not unreal.EditorAssetLibrary.does_asset_exist(destination+'/'+name):
    task=unreal.AssetImportTask(); task.filename=str(source); task.destination_path=destination
    task.destination_name=name; task.automated=True; task.replace_existing=False; task.save=False; task.factory=unreal.FbxFactory()
    options=unreal.FbxImportUI(); options.import_mesh=True; options.import_as_skeletal=False
    options.import_materials=False; options.import_textures=False; options.automated_import_should_detect_type=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes=True; options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.generate_lightmap_u_vs=True; task.options=options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=unreal.load_asset(destination+'/'+name); assert isinstance(mesh,unreal.StaticMesh)
assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==source.resolve()
spec=json.loads(source.with_suffix('.json').read_text()); extent=mesh.get_bounds().box_extent
dimensions=[extent.x*2,extent.y*2,extent.z*2]
assert all(abs(a-b*100)<1 for a,b in zip(dimensions,spec['dimensions_metres'])),dimensions
materials={'M_Vault_Stone':'/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_IvoryStone',
    'M_Vault_Gold':'/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_gold',
    'M_Vault_Shadow':'/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_dark_trim',
    'M_Vault_Inlay':'/Game/Aurelion/Environment/Blender/M_RecessPanel_WarmInlay'}
for i,slot in enumerate(mesh.get_editor_property('static_materials')):
    material=unreal.load_asset(materials[str(slot.get_editor_property('imported_material_slot_name'))]); assert material
    mesh.set_material(i,material)
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assert sm.get_simple_collision_count(mesh)==0 and sm.get_convex_collision_count(mesh)==0
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
out=root/'Saved/Validation/Aurelion/CrucibleVault-20260913'; out.mkdir(parents=True,exist_ok=True)
(out/'import.json').write_text(json.dumps(dict(mesh=mesh.get_path_name(),dimensions_cm=dimensions,collision='none',resolved_material_slots=4),indent=2),encoding='utf8')
unreal.log('CRUCIBLE_VAULT_IMPORTED')
