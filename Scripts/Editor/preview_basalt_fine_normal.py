"""Compare the remaining concrete normal on basalt with the custom fine-stone map."""
from pathlib import Path
import json,os,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);lib=unreal.MaterialEditingLibrary
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==3140
path='/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingBasalt';material=unreal.load_asset(path)
normal=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate) and abs(normal.get_editor_property('const_alpha')-.12)<.001
node=lib.get_inputs_for_material_expression(material,normal)[1];old=node.get_editor_property('texture');assert old.get_name()=='T_KB3D_UTP_ConcreteMix_normal_jpg'
texture=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Textures/T_AurelionKit_StoneFine_N');assert texture
assert texture.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_NORMALMAP and not texture.get_editor_property('srgb') and texture.get_editor_property('flip_green_channel')
exec(compile((root/'Scripts/Editor/inspect_aurelion_surface_palette.py').read_text().split('rows = {}')[0],'palette_graph_helper','exec'))
props=[unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_ROUGHNESS];before=[graph(material,lib.get_material_property_input_node(material,p)) for p in props]
persist=bool(globals().get('PERSIST',False))
if persist:shutil.copy2(root/'Content/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingBasalt.uasset',out/'PavingBasalt-before.uasset')
material.modify();node.modify();node.set_editor_property('texture',texture);lib.recompile_material(material)
assert before==[graph(material,lib.get_material_property_input_node(material,p)) for p in props]
assert abs(normal.get_editor_property('const_alpha')-.12)<.001
if persist:assert unreal.EditorAssetLibrary.save_loaded_asset(material)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'basalt-normal-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',material=path,old_texture=old.get_path_name(),new_texture=texture.get_path_name(),normal_strength=.12,base_color_and_roughness_unchanged=True,qualification='Normal input comparison only; segmented inlays and broader rendering quality require visual review.'),indent=2))
if not globals().get('SKIP_NORMAL_CAPTURE',False):
    if globals().get('COMPARE_GOLD_FLAT_NORMAL',False):
        assert not persist
        gold=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_Gold')
        gold_normal=lib.get_material_property_input_node(gold,unreal.MaterialProperty.MP_NORMAL)
        assert isinstance(gold_normal,unreal.MaterialExpressionLinearInterpolate) and abs(gold_normal.get_editor_property('const_alpha')-.1)<.001
        gold_normal.set_editor_property('const_alpha',0);lib.recompile_material(gold)
        (out/'gold-normal-comparison.json').write_text(json.dumps(dict(status='unsaved',normal_strength_before=.1,normal_strength_after=0,scope='Gold normal contribution only; basalt remains on the preceding fine-normal comparison.'),indent=2))
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('court-entry',unreal.Vector(-500,19800,-950),unreal.Rotator(pitch=-5,yaw=75),90)")
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('court-pattern',unreal.Vector(0,19400,-250),unreal.Rotator(pitch=-35,yaw=90),85)").replace('z01-','z08-')
    exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'basalt_normal_review','exec'))
