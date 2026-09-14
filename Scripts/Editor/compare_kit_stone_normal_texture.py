"""Unsaved normal-only isolation; preserve the stone base color and roughness inputs."""
import json,os
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);lib=unreal.MaterialEditingLibrary
material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingIvory');assert material
base=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL);assert isinstance(base,unreal.MaterialExpressionLinearInterpolate)
inputs=lib.get_inputs_for_material_expression(material,base);texture_node=inputs[1];texture=texture_node.get_editor_property('texture')
source=texture.get_editor_property('asset_import_data').get_first_filename()
before=base.get_editor_property('const_alpha');assert abs(before-.12)<.001
(out/'normal-texture-isolation.json').write_text(json.dumps(dict(status='unsaved_diagnostic',texture=texture.get_path_name(),source_filename=source,before_normal_texture_blend=before,after_normal_texture_blend=0,base_color_and_roughness_unchanged=True,scope='PavingIvory is shared; this temporary change affects every use in the preview, and is not saved.'),indent=2))
material.modify();base.modify();base.set_editor_property('const_alpha',0);lib.recompile_material(material)
exec(compile((root/'Scripts/Editor/preview_z06_gate_housing.py').read_text(),'preview_z06_gate_housing','exec'),globals())
