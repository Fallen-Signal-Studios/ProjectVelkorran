"""Prepare skeleton-matched parasite recoil assets; do not connect unguarded cues."""
from pathlib import Path
import json
import os
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
folder = '/Game/Aurelion/Enemies/Animation'
cases = {
    'Linkbound': 'Parasites/Anim_Parazite_Get_Hit',
    'WallRunner': 'Parasite_Spider/Anim_Spider_Get_hit',
    'Weaver': 'Parasites_Alfa/Animations/Anim_Alfa_Get_Hit',
    'Elite': 'Parasites_Fat/Anim_Fat_Get_hit',
}
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
def finish_compilation():
    # UE 5.7's registered asset-compilation command drains pending animation DDC jobs.
    unreal.SystemLibrary.execute_console_command(world, 'Editor.AsyncAssetCompilationFinishAll')
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
report = dict(roles={}, qualification='Prepared recoil assets only; protected damage-trigger integration and runtime animation acceptance remain required.')
for role, source_name in cases.items():
    source = unreal.load_asset('/Game/Parasites_Pack/Animations/' + source_name)
    bp = unreal.load_asset(folder + '/ABP_Eclipse' + role)
    assert source and bp
    skeleton = source.get_editor_property('skeleton')
    assert skeleton == bp.get_editor_property('target_skeleton'), role
    clip_path = folder + '/AS_Eclipse' + role + '_HitReaction'
    montage_path = folder + '/AM_Eclipse' + role + '_HitReaction'
    set_path = folder + '/DA_Eclipse' + role + '_HitReaction'
    assert not any(lib.does_asset_exist(p) for p in (clip_path, montage_path, set_path)), 'Refusing overwrite'
    clip = lib.duplicate_asset(source.get_path_name(), clip_path)
    finish_compilation()
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('target_skeleton', skeleton)
    factory.set_editor_property('source_animation', clip)
    montage = tools.create_asset(montage_path.rsplit('/', 1)[1], folder, unreal.AnimMontage, factory)
    assert montage
    result = unreal.SovBlueprintAuthoringLibrary.configure_enemy_cast_montage(
        montage, clip, 0., clip.get_play_length(), 1., 'DefaultSlot')
    assert result.succeeded, result.report
    finish_compilation()
    data_factory = unreal.DataAssetFactory()
    data_factory.set_editor_property('data_asset_class', unreal.NarrativeAnimSet)
    anim_set = tools.create_asset(set_path.rsplit('/', 1)[1], folder, unreal.NarrativeAnimSet, data_factory)
    pair = unreal.NarrativeCharacterAnimation()
    pair.set_editor_property('Montage3P', montage)
    anim_set.set_editor_property('character_anims', [pair])
    assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
    assert not clip.get_editor_property('enable_root_motion')
    for asset in (clip, montage, anim_set):
        assert lib.save_loaded_asset(asset, only_if_is_dirty=False)
    finish_compilation()
    report['roles'][role] = dict(source=source.get_path_name(), montage=montage.get_path_name(),
        anim_set=anim_set.get_path_name(), skeleton=skeleton.get_path_name(), duration=clip.get_play_length())
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'hit-reaction-assets.json').write_text(json.dumps(report, indent=2))
