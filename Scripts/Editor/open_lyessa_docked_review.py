"""Reopen the working head in the main window, bypassing the tiny floating layout."""
import unreal

asset = unreal.load_asset('/Game/Aurelion/Art/Characters/MetaHumans/Working/MHC_Lyessa')
assert isinstance(asset, unreal.MetaHumanCharacter)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
editors = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
editors.close_all_editors_for_asset(asset)
settings_class = unreal.load_class(None, '/Script/UnrealEd.EditorStyleSettings')
assert settings_class
settings = unreal.get_default_object(settings_class)
previous = settings.get_editor_property('AssetEditorOpenLocation')
try:
    settings.set_editor_property('AssetEditorOpenLocation', type(previous).cast(2))
    editors.open_editor_for_assets([asset])
finally:
    settings.set_editor_property('AssetEditorOpenLocation', previous)


