"""Read-only court material inputs for unresolved inlay rendering artifacts."""
from pathlib import Path
import json,os
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
exec(compile((root/'Scripts/Editor/inspect_aurelion_surface_palette.py').read_text().split('rows = {}')[0],'material_graph_helper','exec'))
rows={}
for name in ('PavingIvory','PavingBasalt','Gold','Reveal'):
    material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_'+name)
    assert isinstance(material,unreal.Material)
    inputs={}
    for key,enum_name in [('base_color','MP_BASE_COLOR'),('normal','MP_NORMAL'),('roughness','MP_ROUGHNESS'),('opacity_mask','MP_OPACITY_MASK'),('pixel_depth_offset','MP_PIXEL_DEPTH_OFFSET'),('world_position_offset','MP_WORLD_POSITION_OFFSET')]:
        prop=getattr(unreal.MaterialProperty,enum_name,None)
        inputs[key]=dict(status='unavailable_in_python_api',enum=enum_name) if prop is None else graph(material,library.get_material_property_input_node(material,prop))
    rows[name]=dict(blend_mode=str(material.get_editor_property('blend_mode')),inputs=inputs)
(out/'court-material-inputs.json').write_text(json.dumps(dict(status='read_only',materials=rows),indent=2))
