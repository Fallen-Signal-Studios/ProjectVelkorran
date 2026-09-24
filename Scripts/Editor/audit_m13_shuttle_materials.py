"""Read-only graph and reference audit of the two owned scenic shuttle palettes."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
destination = '/Game/Aurelion/Environment/Blender/Shuttles'
rows = {'_api': [name for name in dir(unreal.MaterialEditingLibrary)
                 if 'material' in name.lower() or 'expression' in name.lower()]}
for faction in ('Dominion', 'Reformation'):
    for key in ('Armor', 'Trim', 'Structure', 'Glass', 'Signal'):
        path = f'{destination}/M_{faction}_{key}'
        material = unreal.load_asset(path)
        assert isinstance(material, unreal.Material), path
        inputs = {}
        getter = getattr(unreal.MaterialEditingLibrary, 'get_material_property_input_node', None)
        if getter:
            for prop in ('MP_BASE_COLOR', 'MP_METALLIC', 'MP_ROUGHNESS', 'MP_EMISSIVE_COLOR'):
                try:
                    node = getter(material, getattr(unreal.MaterialProperty, prop))
                    if node:
                        inputs[prop] = dict(type=node.get_class().get_name(),
                                            constant=str(node.get_editor_property('constant'))
                                            if prop in ('MP_BASE_COLOR', 'MP_EMISSIVE_COLOR') else
                                            str(node.get_editor_property('r')))
                except Exception as error:
                    inputs[prop] = str(error)
        rows[path] = dict(expression_count=unreal.MaterialEditingLibrary.get_num_material_expressions(material),
                          inputs=inputs,
                          referencers=[str(value) for value in
                                       unreal.EditorAssetLibrary.find_package_referencers_for_asset(path)])
(out / 'shuttle-material-audit.json').write_text(json.dumps(rows, indent=2), encoding='utf8')
unreal.log('AURELION_SHUTTLE_MATERIAL_AUDIT_PASS')
