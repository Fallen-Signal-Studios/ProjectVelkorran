"""Use the authored translucent touch glass on a standard static mesh, not Nanite."""
from pathlib import Path
import hashlib
import json
import os

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
spec = json.loads((root/'Art/Source/Aurelion/Z11WitnessTablet/manifest.json').read_text(encoding='utf8'))['modules'][0]
assert spec['asset'] == 'SM_Aurelion_KIT_Z11WitnessTablet' and spec['nanite_enabled'] is False
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
mesh = unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/'+spec['asset'])
assert mesh and mesh.get_name() == spec['asset']
materials = [mesh.get_material(i).get_path_name() for i in range(len(mesh.get_editor_property('static_materials')))]
assert len(materials) == 5 and any(name.endswith('M_AurelionKit_ViewGlass.M_AurelionKit_ViewGlass') for name in materials)
maps = {name:root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest = lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
map_hashes = {name:digest(p) for name,p in maps.items()}
asset = root/'Content/Aurelion/Environment/ArchitectureKit/Meshes'/(spec['asset']+'.uasset')
asset_before = digest(asset)
editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
nanite = editor.get_nanite_settings(mesh)
was_enabled = bool(nanite.enabled)
if was_enabled:
    nanite.set_editor_property('enabled',False)
    editor.set_nanite_settings(mesh,nanite,True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
assert not editor.get_nanite_settings(mesh).enabled
assert {name:digest(p) for name,p in maps.items()} == map_hashes
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
    # Updating a referenced static mesh dirties the open map in memory. It
    # is a derived component refresh, not an authored level edit.
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z11-tablet-nanite.json').write_text(json.dumps(dict(
    status='saved',reason='Translucent M_AurelionKit_ViewGlass is unsupported by Nanite',
    asset=mesh.get_path_name(),asset_hash_before=asset_before,
    asset_hash_after=digest(asset),nanite_was_enabled=was_enabled,nanite_enabled=False,
    material_paths=materials,map_hashes=map_hashes),indent=2),encoding='utf8')
print('Z11_TABLET_NANITE_DISABLED')
