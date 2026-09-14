"""Read current stone material graphs without editing assets or live gameplay."""
import json
import os
from pathlib import Path
import unreal

library = unreal.MaterialEditingLibrary

def graph(material, node, depth=0):
    if not node:
        return None
    row = dict(path=node.get_path_name(), type=node.get_class().get_name())
    for name in ('r', 'constant', 'default_value', 'parameter_name', 'const_a', 'const_b',
                 'const_alpha', 'texture', 'u_tiling', 'v_tiling'):
        try:
            value = node.get_editor_property(name)
            row[name] = value.get_path_name() if isinstance(value, unreal.Object) else str(value)
        except Exception:
            pass
    if depth < 10:
        row['inputs'] = [graph(material, child, depth+1)
            for child in library.get_inputs_for_material_expression(material, node)]
    return row

rows = {}
for name in ('M_Radiance_LowerBlack', 'M_Radiance_LowerBlackWorld', 'M_Radiance_IvoryStone'):
    material = unreal.load_asset('/Game/Aurelion/Environment/RadianceMaterials/' + name)
    assert isinstance(material, unreal.Material), name
    rows[name] = {key: graph(material, library.get_material_property_input_node(material, prop))
        for key, prop in [('base_color', unreal.MaterialProperty.MP_BASE_COLOR),
                          ('normal', unreal.MaterialProperty.MP_NORMAL),
                          ('roughness', unreal.MaterialProperty.MP_ROUGHNESS)]}
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'surface-palette.json'
out.write_text(json.dumps(dict(read_only=True, materials=rows), indent=2), encoding='utf-8')
unreal.log('SURFACE_PALETTE_READONLY ' + str(out))
