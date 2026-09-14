"""Apply measured throw trim and rebuild brace clips with clean animation tracks."""
import json
import os
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/ProtagonistCasts/'
weapon = '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
report = dict(saved=[], live_qualified=False)
for hero, name in [('Tarrik', 'CinderStickyGrenade'), ('Selene', 'StillpointGrenade')]:
    suffix = hero + '_' + name + '_B'
    clip = unreal.load_asset(root + 'AS_' + suffix)
    montage = unreal.load_asset(root + 'AM_' + suffix)
    # Source motion probe locates the throw burst at 2.50-2.63 seconds.
    # This window puts its peak near Tarrik's unchanged .35-second release.
    result = unreal.SovBlueprintAuthoringLibrary.configure_protagonist_cast_montage(montage, clip, 2., 3.2, 1.6)
    assert result.succeeded, result.report
    assert unreal.EditorAssetLibrary.save_loaded_asset(clip, False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(montage, False)
    report['saved'].append(montage.get_path_name())
for hero, name, family, duration, release in [
    ('Tarrik', 'CinderJudgement', 'Rifle', .73, .28),
    ('Tarrik', 'CinderlineRequiem', 'Rifle', 1.1, .45),
    ('Selene', 'StaccatoZero', 'Rifle', .65, 0.),
    ('Selene', 'AxiomNullPulse', 'Pistol', .7, 0.),
]:
    source = unreal.load_asset(weapon + family + '/3P/MM_' + family + '_Idle')
    for variant in ('A', 'B'):
        suffix = hero + '_' + name + '_' + variant
        clip = unreal.load_asset(root + 'AS_' + suffix)
        result = unreal.SovBlueprintAuthoringLibrary.author_braced_cast_clip(clip, source, duration, release, variant == 'B')
        assert result.succeeded, result.report
        montage = unreal.load_asset(root + 'AM_' + suffix)
        length = clip.get_editor_property('sequence_length')
        result = unreal.SovBlueprintAuthoringLibrary.configure_protagonist_cast_montage(montage, clip, 0., length, length / duration)
        assert result.succeeded, result.report
        for asset in (clip, montage):
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
            report['saved'].append(asset.get_path_name())
report['status'] = 'throw trim and clean brace tracks saved'
(out / 'cast-pair-finalization.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
review_out = out / 'FinalCastAudit'
review_out.mkdir(exist_ok=True)
old_keep = os.environ.get('SOV_AURELION_ENTRY_KEEP_OPEN', '0')
os.environ['SOV_AURELION_RUN_DIRECTORY'] = str(review_out)
os.environ['SOV_AURELION_ENTRY_KEEP_OPEN'] = '0'
try:
    review_script = Path(unreal.Paths.project_dir()).resolve() / 'Scripts/Editor/review_protagonist_cast_pairs.py'
    exec(compile(review_script.read_text(encoding='utf-8'), str(review_script), 'exec'), {})
finally:
    os.environ['SOV_AURELION_RUN_DIRECTORY'] = str(out)
    os.environ['SOV_AURELION_ENTRY_KEEP_OPEN'] = old_keep
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([
    unreal.load_asset(root + 'AM_Selene_AxiomNullPulse_A'),
    unreal.load_asset(root + 'AM_Tarrik_CinderStickyGrenade_B'),
    unreal.load_asset(root + 'AM_Tarrik_CinderSlam_A'),
])
