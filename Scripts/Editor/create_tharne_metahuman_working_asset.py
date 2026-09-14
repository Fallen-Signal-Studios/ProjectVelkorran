"""Create an editable cast work asset; never replace a live appearance with an unfinished head."""
import unreal

path = '/Game/Aurelion/Art/Characters/MetaHumans/Working/MHC_Tharne'
assert not unreal.EditorAssetLibrary.does_asset_exist(path)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'MHC_Tharne', '/Game/Aurelion/Art/Characters/MetaHumans/Working',
    unreal.MetaHumanCharacter, unreal.MetaHumanCharacterFactoryNew())
assert asset
unreal.EditorAssetLibrary.set_metadata_tag(asset, 'SovereignCastReference', 'Tom Holland')
unreal.EditorAssetLibrary.set_metadata_tag(asset, 'SovereignArtStatus', 'Working asset: preset selection, face sculpt, hair, wardrobe, rig and runtime integration pending')
assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])
unreal.log('THARNE_METAHUMAN_WORKING_ASSET_CREATED_NOT_GAMEPLAY_READY')
