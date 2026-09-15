"""Bind the prototype's saved Dataflow terminal for reproducible regeneration."""
from pathlib import Path
import unreal
asset=unreal.load_asset('/Game/Aurelion/ArtReview/Chaos/GC_Aurelion_CargoPrototype')
assert asset
instance=asset.get_editor_property('dataflow_instance')
instance.set_editor_property('dataflow_terminal','CargoTerminal')
asset.set_editor_property('dataflow_instance',instance)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/verify_aurelion_chaos_saved.py').read_text(),
             'verify_aurelion_chaos_saved','exec'),globals())
