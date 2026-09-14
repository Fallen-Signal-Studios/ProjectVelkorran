"""Replace the diagnosed kit normal input while retaining base color and roughness."""
import json,os,shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);save_normal=bool(globals().get('SAVE_STONE_NORMAL',False));lib=unreal.MaterialEditingLibrary
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
source=root/'Art/Source/Aurelion/StoneNormalKit/T_AurelionKit_StoneFine_N.png';destination='/Game/Aurelion/Environment/ArchitectureKit/Textures'
task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=destination;task.destination_name=source.stem;task.automated=True;task.replace_existing=True;task.save=False;task.factory=unreal.TextureFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);texture=unreal.load_asset(destination+'/'+source.stem);assert isinstance(texture,unreal.Texture2D)
texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);texture.set_editor_property('srgb',False);texture.set_editor_property('flip_green_channel',True);assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
exec(compile((root/'Scripts/Editor/inspect_aurelion_surface_palette.py').read_text().split('rows = {}')[0],'palette_graph_helper','exec'))
rows=[]
for name,alpha in [('Ivory',.20),('PavingIvory',.12)]:
    path='/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_'+name;material=unreal.load_asset(path);assert material
    normal=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL);assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate) and abs(normal.get_editor_property('const_alpha')-alpha)<.001
    node=lib.get_inputs_for_material_expression(material,normal)[1];old=node.get_editor_property('texture');assert old.get_name()=='T_KB3D_UTP_ConcreteMix_normal_jpg'
    props=[unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_ROUGHNESS];before=[graph(material,lib.get_material_property_input_node(material,p)) for p in props]
    if save_normal:shutil.copy2(root/('Content/'+path.removeprefix('/Game/')+'.uasset'),out/(material.get_name()+'-before.uasset'))
    material.modify();node.modify();node.set_editor_property('texture',texture);node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL);lib.recompile_material(material)
    assert before==[graph(material,lib.get_material_property_input_node(material,p)) for p in props]
    if save_normal:assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    rows.append(dict(material=path,previous_normal=old.get_path_name(),new_normal=texture.get_path_name(),retained_strength=alpha,base_and_roughness_unchanged=True))
(out/'kit-stone-normal-fit.json').write_text(json.dumps(dict(status='saved' if save_normal else 'unsaved_preview',materials=rows,normal_compression=True,srgb=False,flip_green=True,qualification='Normal input replacement only; diagonal shadow artifacts and final surface art remain unresolved.'),indent=2))
exec(compile((root/'Scripts/Editor/preview_z06_gate_housing.py').read_text(),'preview_z06_gate_housing','exec'),globals())
