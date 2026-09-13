"""Import reviewed Blender source art into owned assets; does not place actors."""
import json
import os
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
name = 'SM_Aurelion_RecessPanel_2m'
destination = '/Game/Aurelion/Environment/Blender'
existing_mesh = unreal.load_asset(destination + '/' + name) if unreal.EditorAssetLibrary.does_asset_exist(destination + '/' + name) else None
if existing_mesh:
    assert Path(existing_mesh.get_editor_property('asset_import_data').get_first_filename()).resolve() == (root / 'Art/Source/Aurelion' / (name + '.fbx')).resolve()
task = unreal.AssetImportTask()
task.filename = str(root / 'Art/Source/Aurelion' / (name + '.fbx'))
task.destination_path = destination
task.destination_name = name
task.automated = True
task.replace_existing = False
task.save = False
task.factory = unreal.FbxFactory()
options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.auto_generate_collision = False
options.static_mesh_import_data.generate_lightmap_u_vs = True
task.options = options
if not existing_mesh:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(destination + '/' + name)
assert isinstance(mesh, unreal.StaticMesh), task.imported_object_paths
extent = mesh.get_bounds().box_extent
assert all(abs(a-b) < .2 for a,b in zip((extent.x,extent.y,extent.z),(100,20,150))), extent
materials = {
    'M_Aurelion_IvoryStone': '/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_IvoryStone',
    'M_Aurelion_AncientGold': '/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_gold',
    'M_Aurelion_ChannelShadow': '/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_dark_trim',
}
light_path = destination + '/M_RecessPanel_WarmInlay'
material = unreal.load_asset(light_path) if unreal.EditorAssetLibrary.does_asset_exist(light_path) else None
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_RecessPanel_WarmInlay', destination, unreal.Material, unreal.MaterialFactoryNew())
    node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant', unreal.LinearColor(2, 1.28, .44, 1))
    assert unreal.MaterialEditingLibrary.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
materials['M_Aurelion_WarmInformation'] = light_path
slots = []
for i, slot in enumerate(mesh.get_editor_property('static_materials')):
    key = str(slot.get_editor_property('imported_material_slot_name'))
    assert key in materials, key
    resolved = unreal.load_asset(materials[key])
    assert resolved
    mesh.set_material(i, resolved)
    slots.append(dict(slot=key, material=resolved.get_path_name()))
assert len(slots) == 4
subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
collision_count = subsystem.get_simple_collision_count(mesh)
convex_count = subsystem.get_convex_collision_count(mesh)
assert collision_count == 0 and convex_count == 1, (collision_count, convex_count)
assert unreal.EditorAssetLibrary.save_loaded_asset(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
report = dict(status='imported_verified_not_placed', mesh=mesh.get_path_name(),
    dimensions_cm=[extent.x*2,extent.y*2,extent.z*2], simple_collision_count=collision_count, convex_collision_count=convex_count,
    slots=slots, limitations='No map instances changed; placement, lighting and navigation acceptance pending')
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'recess-panel-import.json'
out.write_text(json.dumps(report, indent=2), encoding='utf8')
unreal.log('RECESS_PANEL_IMPORTED ' + str(out))
