"""Read-only graph inventory of the actual kit stone materials."""
import json,os
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
exec(compile((root/'Scripts/Editor/inspect_aurelion_surface_palette.py').read_text().split('rows = {}')[0],'palette_graph_helper','exec'))
rows={}
for name in ('Ivory','PavingIvory','StoneGrout'):
    material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_'+name);assert material
    rows[name]={key:graph(material,library.get_material_property_input_node(material,prop)) for key,prop in [('base_color',unreal.MaterialProperty.MP_BASE_COLOR),('normal',unreal.MaterialProperty.MP_NORMAL),('roughness',unreal.MaterialProperty.MP_ROUGHNESS)]}
(out/'kit-stone-inputs.json').write_text(json.dumps(dict(status='read_only',materials=rows),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
print('KIT_STONE_INPUTS_READ_PASS')
