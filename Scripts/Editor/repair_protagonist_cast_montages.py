"""Put back the cosmetic A/B cast montages an ability re-save dropped.

USovGameplayAbility_EchoBase::PlayAlternatingCastMontage requires exactly two distinct montages
and plays nothing otherwise, so an ability that loses one casts in silence. Re-saving
GA_Tarrik_CinderStickyGrenade dropped AM_Tarrik_CinderStickyGrenade_B while the montage asset
itself stayed on disk.

Each protagonist ability GA_<Name> pairs with AM_<Name>_A and AM_<Name>_B in
/Game/Characters/Animation/ProtagonistCasts. This checks every one of them against that pairing
and repairs only the assets that do not match, so unrelated edits to those Blueprints survive.
Assets already correct are left untouched and unsaved.
"""
import json
import os
from pathlib import Path
import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', str(PROJECT / 'Saved/Validation/Aurelion/CastMontages')))
RUN.mkdir(parents=True, exist_ok=True)
CASTS = '/Game/Characters/Animation/ProtagonistCasts/'
ABILITIES = [
    '/Game/Abilities/Tarrik/GA_Tarrik_CinderStickyGrenade',
    '/Game/Abilities/Tarrik/GA_Tarrik_CinderSlam',
    '/Game/Abilities/Tarrik/GA_Tarrik_CinderJudgement',
    '/Game/Abilities/Tarrik/GA_Tarrik_CinderlineRequiem',
    '/Game/Abilities/Tarrik/GA_Tarrik_VelkorransHunger',
    '/Game/Abilities/Selene/GA_Selene_StillpointGrenade',
    '/Game/Abilities/Selene/GA_Selene_StaccatoZero',
    '/Game/Abilities/Selene/GA_Selene_VeritysWake',
    '/Game/Abilities/Selene/GA_Selene_AxiomNullPulse',
    '/Game/Abilities/Selene/GA_Selene_Dispatch',
]

report = {'status': 'running', 'abilities': {}, 'repaired': [], 'saved': []}


def name(obj):
    return obj.get_name() if obj else None


try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    for path in ABILITIES:
        blueprint = unreal.load_asset(path)
        assert blueprint, 'Missing ability ' + path
        cdo = unreal.get_default_object(blueprint.generated_class())
        current = [m for m in cdo.get_editor_property('cast_montages')]
        stem = path.rsplit('/', 1)[1][len('GA_'):]
        wanted = []
        for suffix in ('_A', '_B'):
            montage = unreal.load_asset(CASTS + 'AM_' + stem + suffix)
            assert montage, 'Missing authored cast montage AM_' + stem + suffix
            wanted.append(montage)
        row = {'before': [name(m) for m in current], 'expected': [name(m) for m in wanted]}
        if [name(m) for m in current] == [name(m) for m in wanted]:
            row['action'] = 'already correct'
            report['abilities'][path] = row
            continue
        cdo.set_editor_property('cast_montages', wanted)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        # Read the pairing back off a freshly compiled default object, not the one just written to.
        verified = [name(m) for m in unreal.get_default_object(blueprint.generated_class()).get_editor_property('cast_montages')]
        assert verified == [name(m) for m in wanted], 'Repair did not hold for ' + path + ': ' + str(verified)
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), path
        row['action'] = 'repaired'
        row['after'] = verified
        report['abilities'][path] = row
        report['repaired'].append(path)
        report['saved'].append(blueprint.get_path_name())
    report['status'] = 'repaired' if report['repaired'] else 'no_change_needed'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    (RUN / 'cast-montages.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf-8')
    unreal.log('CAST_MONTAGES ' + report['status'])
