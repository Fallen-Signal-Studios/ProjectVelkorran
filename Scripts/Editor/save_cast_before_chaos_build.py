"""Preserve the open working cast asset, then close the owned editor for compilation."""
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
asset = unreal.load_asset('/Game/Aurelion/Art/Characters/MetaHumans/Working/MHC_Lyessa')
assert asset
subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if subsystem.is_object_added_for_editing(asset):
    subsystem.commit_face_state(asset)
unreal.EditorAssetLibrary.set_metadata_tag(asset, 'SovereignWorkingStatus',
    'Jelena baseline with MediumBobCurly. Working only; likeness, rig, wardrobe and runtime integration pending.')
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log('CAST_WORK_PRESERVED_BEFORE_CHAOS_BUILD')
unreal.SystemLibrary.quit_editor()
