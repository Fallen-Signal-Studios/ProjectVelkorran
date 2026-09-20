"""Persist receiver status material support on the authored Nanite consoles."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
relative = 'Content/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ReceiverStatus.uasset'
source = Path(unreal.Paths.project_dir()) / relative
backup = out / 'M_AurelionKit_ReceiverStatus.before.uasset'
assert not backup.exists()
shutil.copy2(source, backup)
map_file = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before_map = hashlib.sha256(map_file.read_bytes()).hexdigest()
material = unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ReceiverStatus')
assert isinstance(material, unreal.Material)
before = material.get_editor_property('used_with_nanite')
material.modify()
material.set_editor_property('used_with_nanite', True)
unreal.MaterialEditingLibrary.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
assert material.get_editor_property('used_with_nanite')
assert hashlib.sha256(map_file.read_bytes()).hexdigest() == before_map
(out / 'receiver-status-nanite.json').write_text(json.dumps(dict(
    status='saved_requires_fresh_readback', before=before, after=True,
    material=material.get_path_name(), map_unchanged=True), indent=2))
