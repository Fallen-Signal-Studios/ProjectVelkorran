"""Fresh Unreal asset-load check for the unsaved Z12 open-vista mesh candidates."""
import hashlib
import json
import os
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root / 'Art/Source/Aurelion/Z12OpenVistaKit'
manifest = json.loads((source / 'manifest.json').read_text())
sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
rows = []
for spec in manifest['modules']:
    path = '/Game/Aurelion/Environment/ArchitectureKit/Meshes/' + spec['asset']
    mesh = unreal.load_asset(path)
    assert mesh and mesh.get_editor_property('asset_import_data')
    assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve() == (source / (spec['asset'] + '.fbx')).resolve()
    assert sm.get_num_uv_channels(mesh, 0) == spec['uv_layers'] == 2
    assert sm.get_simple_collision_count(mesh) == sm.get_convex_collision_count(mesh) == 0
    extent = mesh.get_bounds().box_extent
    dimensions = [2 * extent.x, 2 * extent.y, 2 * extent.z]
    assert all(abs(a - b * 100) < 1 for a, b in zip(dimensions, spec['nominal_dimensions_m']))
    assert sm.get_nanite_settings(mesh).enabled
    assert set(str(slot.get_editor_property('imported_material_slot_name'))
               for slot in mesh.get_editor_property('static_materials')) == set(spec['materials'])
    rows.append(dict(asset=path, dimensions_cm=dimensions, uv_channels=2,
                     simple_collision=0, convex_collision=0, nanite=True))
maps = {name: hashlib.sha256((root / 'Content/Aurelion/Maps' / name).read_bytes()).hexdigest()
        for name in ('L_Aurelion_M12.umap', 'L_Aurelion_M13.umap')}
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'open-vista-fresh-assets.json').write_text(json.dumps(dict(
    status='fresh_assets_pass', assets=rows, map_hashes=maps,
    qualification='Fresh editor asset load only; the three mesh actors are intentionally unplaced.'), indent=2))
print('Z12_OPEN_VISTA_FRESH_ASSETS_PASS')
