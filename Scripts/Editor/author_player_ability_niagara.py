"""Bind modest protagonist cast and projectile Niagara to the native cosmetic hooks.

The ability gameplay payloads, grants, costs, weapon gates, and montage pairs are
read and validated but never changed. Saves only the ten project ability BPs,
three project Selene projectile BPs, and a new Requiem line presentation BP.
"""
import json
import os
from pathlib import Path

import unreal


RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'player-ability-niagara-authoring.json'
ROOT = '/Game/Abilities/Presentation/'
FIRE = '/Game/Fire_Magic/VFX_Niagara/'
ICE = '/Game/Ice_Magic/VFX_Niagara/'
LIGHTNING = '/Game/Lightning_Magic/VFX_Niagara/'
CASTS = {
    ('Tarrik', 'CinderStickyGrenade'): (FIRE + 'NS_Fire_Magic_Orb', .22, 'hand_r'),
    ('Tarrik', 'VelkorransHunger'): (FIRE + 'NS_Fire_Magic_Slash1', .28, 'hand_r'),
    ('Tarrik', 'CinderSlam'): (FIRE + 'NS_Fire_Magic_Shockwave', .32, 'None'),
    ('Tarrik', 'CinderJudgement'): (FIRE + 'NS_Fire_Magic_Muzzle', .28, 'hand_r'),
    ('Tarrik', 'CinderlineRequiem'): (FIRE + 'NS_Fire_Magic_Circle', .30, 'None'),
    ('Selene', 'StillpointGrenade'): (ICE + 'NS_Ice_Magic_Orb', .20, 'hand_r'),
    ('Selene', 'VeritysWake'): (ICE + 'NS_Ice_Magic_Slash2', .28, 'hand_r'),
    ('Selene', 'StaccatoZero'): (ICE + 'NS_Ice_Magic_Muzzle', .26, 'hand_r'),
    ('Selene', 'AxiomNullPulse'): (LIGHTNING + 'NS_Lightning_Magic_Orb', .22, 'hand_r'),
    ('Selene', 'Dispatch'): (ICE + 'NS_Ice_Magic_Slash1', .26, 'hand_r'),
}
PROJECTILES = {
    'BP_StillpointProjectile': {
        'flight_niagara_system': ICE + 'NS_Ice_Magic_Projectile',
        'field_niagara_system': ICE + 'NS_Ice_Magic_Circle2',
        'hit_niagara_system': ICE + 'NS_Ice_Magic_Hit',
    },
    'BP_VeritysWakeProjectile': {
        'flight_niagara_system': ICE + 'NS_Ice_Magic_Slash2',
        'hit_niagara_system': ICE + 'NS_Ice_Magic_Splash',
    },
    'BP_DispatchProjectile': {
        'flight_niagara_system': ICE + 'NS_Ice_Magic_Projectile',
        'recall_niagara_system': LIGHTNING + 'NS_Lightning_Magic_Slash1',
        'hit_niagara_system': ICE + 'NS_Ice_Magic_Hit',
    },
}
report = {'status': 'running', 'abilities': [], 'projectiles': [], 'saved': []}


def asset(path):
    value = unreal.load_asset(path)
    if not value:
        raise RuntimeError('Missing required asset ' + path)
    return value


def default(bp):
    return unreal.get_default_object(bp.generated_class())


def save(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + bp.get_path_name())
    report['saved'].append(bp.get_path_name())


try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    for (hero, name), (fx_path, scale, socket) in CASTS.items():
        bp = asset('/Game/Abilities/' + hero + '/GA_' + hero + '_' + name)
        cdo = default(bp)
        before_pair = [m.get_path_name() for m in cdo.get_editor_property('cast_montages')]
        if len(before_pair) != 2 or before_pair[0] == before_pair[1]:
            raise RuntimeError('Cannot author Niagara on broken cast pair: ' + bp.get_name())
        cdo.set_editor_property('cast_niagara_system', asset(fx_path))
        cdo.set_editor_property('cast_niagara_scale', unreal.Vector(scale, scale, scale))
        cdo.set_editor_property('cast_niagara_socket_name', unreal.Name(socket))
        save(bp)
        after = default(bp)
        after_pair = [m.get_path_name() for m in after.get_editor_property('cast_montages')]
        if after_pair != before_pair or after.get_editor_property('cast_niagara_system').get_path_name() != asset(fx_path).get_path_name():
            raise RuntimeError('Cast save changed montage pair or FX: ' + bp.get_name())
        report['abilities'].append({'ability': bp.get_path_name(), 'cast_niagara': fx_path,
                                    'cast_scale': scale, 'socket': socket, 'pair': after_pair})

    for name, bindings in PROJECTILES.items():
        bp = asset(ROOT + name)
        cdo = default(bp)
        if not isinstance(cdo, unreal.SovSeleneCombatProjectile):
            raise RuntimeError('Expected Selene native projectile child: ' + name)
        for key, fx_path in bindings.items():
            cdo.set_editor_property(key, asset(fx_path))
        save(bp)
        after = default(bp)
        for key, fx_path in bindings.items():
            if after.get_editor_property(key).get_path_name() != asset(fx_path).get_path_name():
                raise RuntimeError('Projectile FX binding did not persist: ' + name + '/' + key)
        report['projectiles'].append({'projectile': bp.get_path_name(), 'bindings': bindings})

    line_path = ROOT + 'BP_CinderlineRequiemLine'
    if unreal.EditorAssetLibrary.does_asset_exist(line_path):
        line_bp = asset(line_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.SovCinderRequiemLine)
        line_bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'BP_CinderlineRequiemLine', ROOT.rstrip('/'), unreal.Blueprint, factory)
        if not line_bp:
            raise RuntimeError('Could not create Requiem presentation class')
    line_fx = FIRE + 'NS_Fire_Magic_Hit'
    default(line_bp).set_editor_property('detonation_niagara_system', asset(line_fx))
    save(line_bp)
    ability_bp = asset('/Game/Abilities/Tarrik/GA_Tarrik_CinderlineRequiem')
    ability_cdo = default(ability_bp)
    pair = [m.get_path_name() for m in ability_cdo.get_editor_property('cast_montages')]
    ability_cdo.set_editor_property('line_class', line_bp.generated_class())
    save(ability_bp)
    if [m.get_path_name() for m in default(ability_bp).get_editor_property('cast_montages')] != pair:
        raise RuntimeError('Requiem line save changed montage pair')
    report['requiem_line'] = {'class': line_bp.generated_class().get_path_name(), 'detonation_niagara': line_fx}
    report['status'] = 'saved'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    OUT.write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('PLAYER_ABILITY_NIAGARA_AUTHORING ' + report['status'])
