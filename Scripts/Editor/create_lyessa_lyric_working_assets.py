"""Create independent editable supporting-cast heads for visual authoring."""
import unreal

folder = '/Game/Aurelion/Art/Characters/MetaHumans/Working'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
for name in ('Lyessa', 'Lyric'):
    assert not unreal.EditorAssetLibrary.does_asset_exist(f'{folder}/MHC_{name}')
created = []
for name, reference in (('Lyessa', 'Eva Green'), ('Lyric', 'Florence Pugh')):
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        f'MHC_{name}', folder, unreal.MetaHumanCharacter, unreal.MetaHumanCharacterFactoryNew())
    assert asset
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'SovereignCastReference', reference)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'SovereignArtStatus',
        'Working head: preset selection, face sculpt, hair, wardrobe, rig and runtime integration pending; not likeness qualified.')
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    created.append(asset)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([created[0]])
unreal.log('LYESSA_LYRIC_WORKING_ASSETS_CREATED_NOT_GAMEPLAY_READY')
