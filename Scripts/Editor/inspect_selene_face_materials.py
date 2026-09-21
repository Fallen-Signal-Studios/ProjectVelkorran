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
        row['resolved_parameters']={}
        for kind in ('scalar','vector','texture','static_switch'):
            names=getattr(library,'get_'+kind+'_parameter_names')(material)
            getter=getattr(library,'get_material_instance_'+kind+'_parameter_value')
            row['resolved_parameters'][kind]={str(name):str(getter(material,name)) for name in names}
        task=unreal.AssetExportTask();task.object=material;task.filename=str(out/(material.get_name()+'.copy'))
        task.automated=True;task.prompt=False
        row['instance_exported']=unreal.Exporter.run_asset_export_task(task)
    rows.append(row)
textures=[]
for name in ('T_Head_BC_VT','T_Head_SRMF_VT','T_Head_N_VT','T_Head_Scatter_VT'):
    texture=unreal.load_asset('/Game/MetaHumans/MHC_Selene/Face/Baked/'+name)
    assert texture,'Missing baked face texture: '+name
    row=dict(path=texture.get_path_name(),properties={})
    for prop in ('srgb','virtual_texture_streaming','compression_settings','filter','lod_group','never_stream'):
        row['properties'][prop]=str(texture.get_editor_property(prop))
    if name=='T_Head_BC_VT':
        task=unreal.AssetExportTask();task.object=texture;task.filename=str(out/(name+'.tga'))
        task.automated=True;task.prompt=False
        row['source_exported']=unreal.Exporter.run_asset_export_task(task)
    textures.append(row)
(out/'selene-face-materials.json').write_text(json.dumps(dict(read_only=True,materials=rows,textures=textures),indent=2))
