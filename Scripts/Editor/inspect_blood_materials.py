import json, os
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
rows = {}
for data in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/RealisticBlood', recursive=True):
    if str(data.asset_class_path.asset_name) != 'MaterialInstanceConstant':
        continue
    asset = data.get_asset()
    rows[asset.get_path_name()] = {
        'vectors': {str(n): unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(asset, n).export_text()
                    for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(asset)},
        'parent': asset.get_editor_property('parent').get_path_name()}
(out/'blood-materials.json').write_text(json.dumps(rows, indent=2))
