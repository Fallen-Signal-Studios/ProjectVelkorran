"""Persist explicitly unfinished Tharne authoring without changing live appearances."""
import unreal

asset = unreal.load_asset('/Game/Aurelion/Art/Characters/MetaHumans/Working/MHC_Tharne')
assert isinstance(asset, unreal.MetaHumanCharacter)
unreal.EditorAssetLibrary.set_metadata_tag(asset, 'SovereignCastReference', 'Tom Holland')
unreal.EditorAssetLibrary.set_metadata_tag(asset, 'SovereignArtStatus',
    'WIP: Orlando baseline; beard and mustache removed; hair whitening/ombre/highlights disabled. '
    'Face sculpt, eye/skin refinement, mission wardrobe, rig and runtime integration pending. '
    'Not likeness qualified.')
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log('THARNE_WORKING_PROGRESS_SAVED_NOT_GAMEPLAY_READY')
