"""Read material inputs and visible chamber text without changing assets."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
exec(compile((root/'Scripts/Editor/inspect_aurelion_surface_palette.py').read_text().split('rows = {}')[0],'material_graph_helper','exec'))
material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_Reveal')
inputs={name:graph(material,library.get_material_property_input_node(material,getattr(unreal.MaterialProperty,prop))) for name,prop in [('base','MP_BASE_COLOR'),('roughness','MP_ROUGHNESS'),('metallic','MP_METALLIC'),('emissive','MP_EMISSIVE_COLOR')]}
texts=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in actor.get_components_by_class(unreal.TextRenderComponent):
        texts.append(dict(actor=actor.get_actor_label(),component=c.get_path_name(),text=str(c.get_editor_property('text')),color=str(c.get_editor_property('text_render_color')),size=c.get_editor_property('world_size'),transform=c.get_world_transform().export_text(),material=c.get_material(0).get_path_name() if c.get_material(0) else None,visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game')))
(out/'m13-readability.json').write_text(json.dumps(dict(rail_material=inputs,texts=texts),indent=2))
