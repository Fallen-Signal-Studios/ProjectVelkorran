"""Author two cosmetic cast montages for every native protagonist Echo ability.

Only saves explicit project copies. Native costs, payloads and timing are retained.
"""
import json
import os
import shutil
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/ProtagonistCasts'
weapon = '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/'
twin = '/Game/Characters/Animation/VerityTwinBlades/SovVerity_Twinblades_Attack_'
throw_a = weapon + 'Generic/Lyra/MM_Rifle_GrenadeToss'
throw_b = '/NarrativePro/Pro/Demo/Cinematics/TestAnims/Throw_ue5'
template = weapon + 'Sword/1H/3P/AM_Sword_3P_1H_Attack_1'
# Character-specific casts; no ordinary attacks or defensive stance loops.
specs = [
    ('Tarrik', 'CinderStickyGrenade', [throw_a, throw_b], .75, None),
    ('Tarrik', 'VelkorransHunger', [weapon + 'Sword/1H/3P/A_Sword_3P_1H_Attack_1',
                                 weapon + 'Sword/1H/3P/A_Sword_3P_1H_Attack_2'], .75, None),
    ('Tarrik', 'CinderSlam', [weapon + 'Sword/2H/3P/A_Sword_3P_2H_Attack_1',
                           weapon + 'Sword/2H/3P/A_Sword_3P_2H_Attack_2'], 1.2, None),
    ('Tarrik', 'CinderJudgement', [weapon + 'Rifle/3P/MM_Rifle_Idle'] * 2, .73, .28),
    ('Tarrik', 'CinderlineRequiem', [weapon + 'Rifle/3P/MM_Rifle_Idle'] * 2, 1.1, .45),
    ('Selene', 'StillpointGrenade', [throw_a, throw_b], .75, None),
    ('Selene', 'VeritysWake', [twin + '01', twin + '03'], .75, None),
    ('Selene', 'StaccatoZero', [weapon + 'Rifle/3P/MM_Rifle_Idle'] * 2, .65, 0.),
    ('Selene', 'AxiomNullPulse', [weapon + 'Pistol/3P/MM_Pistol_Idle'] * 2, .7, 0.),
    ('Selene', 'Dispatch', [twin + '02', twin + '04'], .85, None),
]
report = dict(status='preflight', abilities=[], assets=[], live_qualified=False)

def duplicate(source, name):
    destination = root + '/' + name
    assert not unreal.EditorAssetLibrary.does_asset_exist(destination), destination
    asset = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    assert asset, destination
    report['assets'].append(asset.get_path_name())
    return asset

try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    # Load every dependency before editing any ability defaults.
    loaded = {p: unreal.load_asset(p) for _, _, paths, _, _ in specs for p in paths}
    assert all(loaded.values())
    skeleton = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SK_Mannequin_Narrative')
    for path, asset in loaded.items():
        assert isinstance(asset, unreal.AnimSequence), path
        assert asset.get_editor_property('skeleton') == skeleton, path
    project = Path(unreal.Paths.project_dir()).resolve()
    for hero, name, paths, duration, release in specs:
        bp_path = '/Game/Abilities/' + hero + '/GA_' + hero + '_' + name
        bp = unreal.load_asset(bp_path)
        assert bp
        shutil.copy2(project / ('Content' + bp_path[5:] + '.uasset'), out / (bp.get_name() + '.uasset'))
        cdo = unreal.get_default_object(bp.generated_class())
        assert not list(cdo.get_editor_property('cast_montages')), bp_path
        pair = []
        row = dict(ability=bp_path, casts=[])
        for i, source in enumerate(paths):
            suffix = hero + '_' + name + '_' + ('A' if i == 0 else 'B')
            clip = duplicate(source, 'AS_' + suffix)
            if release is not None:
                result = unreal.SovBlueprintAuthoringLibrary.author_braced_cast_clip(
                    clip, loaded[source], duration, release, i == 1)
                assert result.succeeded, result.report
            length = clip.get_editor_property('sequence_length')
            # Retargeted Twin Blade clips include long idle recoveries. Use their action portion.
            start = 2.0 if source == throw_b else 0.
            end = 3.2 if source == throw_b else (min(length, 1.2) if source.startswith(twin) else length)
            montage = duplicate(template, 'AM_' + suffix)
            result = unreal.SovBlueprintAuthoringLibrary.configure_protagonist_cast_montage(
                montage, clip, start, end, (end - start) / duration)
            assert result.succeeded, result.report
            for asset in (clip, montage):
                assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
            pair.append(montage)
            row['casts'].append(dict(montage=montage.get_path_name(), clip=clip.get_path_name(),
                                     source=source, duration=duration, contract=str(result.report)))
        cdo.set_editor_property('cast_montages', pair)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert list(unreal.get_default_object(bp.generated_class()).get_editor_property('cast_montages')) == pair
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
        report['abilities'].append(row)
    report['status'] = 'authored; reload and visual review pending'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (out / 'protagonist-cast-pairs.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
