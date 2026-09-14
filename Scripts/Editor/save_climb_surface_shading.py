"""Persist reviewed material and light-color corrections with backups."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();assert len(actors)==3140
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
fit=json.loads((root/'Art/Source/Aurelion/Z08WallKit/climb-light-fit.json').read_text());row=fit['light']
a=next(a for a in actors if a.get_actor_label()==row['actor']);c=a.get_component_by_class(unreal.LightComponent)
assert c.get_world_transform().export_text()==row['transform'];assert {k:str(c.get_editor_property(k)) for k in row['properties']}==row['properties']
color=c.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b,color.a),row['linear_color']))<.0001
mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z06ClimbPanel');sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);settings=sm.get_nanite_settings(mesh)
assert not settings.get_editor_property('explicit_tangents')
shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap')
shutil.copy2(root/'Content/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z06ClimbPanel.uasset',out/'SM_Aurelion_KIT_Z06ClimbPanel-before.uasset')
a.modify();c.modify();c.set_light_color(unreal.LinearColor(*fit['selected_linear_color']))
base='/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingIvory'
path='/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ClimbIvory'
assert not unreal.EditorAssetLibrary.does_asset_exist(path)
material=unreal.EditorAssetLibrary.duplicate_asset(base,path);assert material
lib=unreal.MaterialEditingLibrary;normal=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate) and abs(normal.get_editor_property('const_alpha')-.12)<.0001
normal.set_editor_property('const_alpha',.03);lib.recompile_material(material);assert unreal.EditorAssetLibrary.save_loaded_asset(material)
for i,slot in enumerate(mesh.get_editor_property('static_materials')):
    if str(slot.get_editor_property('imported_material_slot_name'))=='M_Aurelion_IvoryStone':mesh.set_material(i,material)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
assert helpers['snapshot_actor_state'](actors)==before
check=runpy.run_path(str(root/'Scripts/Editor/check_climb_surface_shading.py'))['check_climb_surface_shading'](actors)
assert editor.save_current_level()
(out/'climb-shading-save.json').write_text(json.dumps(dict(status='saved',preserved_actor_states=len(actors),settings=check),indent=2))
