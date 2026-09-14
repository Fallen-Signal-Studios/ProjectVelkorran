"""Persist required Nanite usage on the shared lens and grout materials."""
from pathlib import Path
import json, os
import unreal

paths=[]
for name in ('UplightLens','StoneGrout'):
    path='/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_'+name
    material=unreal.load_asset(path)
    assert isinstance(material,unreal.Material)
    material.modify()
    material.set_editor_property('used_with_nanite',True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
    assert material.get_editor_property('used_with_nanite')
    paths.append(path)
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'lens-nanite-save.json').write_text(json.dumps(dict(status='saved',materials=paths,used_with_nanite=True),indent=2))
