"""Increase prototype cluster strength after the first solver test broke on settling."""
import unreal
asset=unreal.load_asset('/Game/Aurelion/ArtReview/Chaos/GC_Aurelion_CargoPrototype')
assert asset and asset.get_dataflow_asset().get_path_name()=='/Game/Aurelion/ArtReview/Chaos/DF_Aurelion_CargoPrototype.DF_Aurelion_CargoPrototype'
asset.set_editor_property('damage_threshold',[500000.0])
assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
from pathlib import Path
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/validate_aurelion_chaos_prototype.py').read_text(),
             'validate_aurelion_chaos_prototype','exec'),globals())
