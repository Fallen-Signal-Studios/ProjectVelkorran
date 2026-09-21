"""Read-only current Selene appearance/material audit for over-bright close shots."""
import json,os
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
exec(compile((root/'Scripts/Editor/inspect_aurelion_surface_palette.py').read_text().split('rows = {}')[0], 'material_graph_helper','exec'))
appearance=unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Appearances/Mannequin/Appearance_Selene')
task=unreal.AssetExportTask();task.object=appearance;task.filename=str(out/'Appearance_Selene.copy')
task.automated=True;task.prompt=False
assert unreal.Exporter.run_asset_export_task(task)
rows=[]
paths=['/Game/MetaHumans/MHC_Selene/Face/Materials/'+n for n in ('M_SeleneFace','MI_Face_Skin_Baked_LOD0_VT','MI_Face_Skin_Baked_LOD1_VT')]
for path in paths:
    material=unreal.load_asset(path)
    assert material,path
    row=dict(path=path,kind=material.get_class().get_name())
    if isinstance(material,unreal.Material):
        row['properties']={name:str(material.get_editor_property(name)) for name in ('blend_mode','shading_model','two_sided')}
        row['inputs']={name:graph(material,library.get_material_property_input_node(material,prop)) for name,prop in (
            ('base_color',unreal.MaterialProperty.MP_BASE_COLOR),('emissive',unreal.MaterialProperty.MP_EMISSIVE_COLOR),
            ('roughness',unreal.MaterialProperty.MP_ROUGHNESS),('specular',unreal.MaterialProperty.MP_SPECULAR))}
    else:
        row['parent']=str(material.get_editor_property('parent'))
        for prop in ('scalar_parameter_values','vector_parameter_values','texture_parameter_values'):
            row[prop]=[v.export_text() for v in material.get_editor_property(prop)]
    rows.append(row)
(out/'selene-face-materials.json').write_text(json.dumps(dict(read_only=True,materials=rows),indent=2))
