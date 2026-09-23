"""Read-only saved-content audit of the ten playable protagonist Echo abilities.

Inspect generated CDOs after a clean editor load. This is configuration evidence,
not a substitute for a PIE cast/projectile/impact qualification.
"""
import json
import os
from pathlib import Path

import unreal


OUT = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir())) / 'player-ability-assets.json'
ABILITIES = {
    'Tarrik': ['CinderStickyGrenade', 'VelkorransHunger', 'CinderSlam', 'CinderJudgement', 'CinderlineRequiem'],
    'Selene': ['StillpointGrenade', 'VeritysWake', 'StaccatoZero', 'AxiomNullPulse', 'Dispatch'],
}
FIELDS = [
    'input_tag', 'echo_cost', 'minimum_echo_required', 'cast_montages',
    'cast_niagara_system', 'cast_niagara_scale', 'cast_niagara_socket_name',
    'allowed_weapon_classes', 'requires_allowed_weapon', 'weapon_gate_policy',
    'grenade_class', 'projectile_class', 'wave_class', 'line_class',
    'returning_verity_class', 'empowered_shot_damage_effect_class',
    'detonation_damage_effect_class', 'explosion_damage_effect_class',
    'direct_damage_effect_class', 'radial_damage_effect_class',
    'penetrating_damage_effect_class', 'line_detonation_effect_class',
    'shield_disruption_damage_effect_class',
    'maximum_grenade_launch_speed', 'default_grenade_launch_speed',
]
PROJECTILE_FIELDS = ['explosion_niagara_system', 'impact_niagara_system',
                     'dissipation_niagara_system', 'explosion_decal_material',
                     'flight_niagara_system', 'field_niagara_system',
                     'recall_niagara_system', 'hit_niagara_system',
                     'detonation_niagara_system']
REQUIRED_PROJECTILE_FX = {
    'BP_CinderGrenadeProjectile': ('explosion_niagara_system',),
    'BP_VelkorransHungerProjectile': ('impact_niagara_system', 'dissipation_niagara_system'),
    'BP_CinderlineRequiemLine': ('detonation_niagara_system',),
    'BP_StillpointProjectile': ('flight_niagara_system', 'field_niagara_system', 'hit_niagara_system'),
    'BP_VeritysWakeProjectile': ('flight_niagara_system', 'hit_niagara_system'),
    'BP_DispatchProjectile': ('flight_niagara_system', 'recall_niagara_system', 'hit_niagara_system'),
}


def path(value):
    if value is None:
        return None
    if isinstance(value, (tuple, list)) or (not isinstance(value, (str, unreal.Object)) and hasattr(value, '__iter__')):
        return [path(item) for item in value]
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    return str(value)


def read(obj, key):
    try:
        value = obj.get_editor_property(key)
        if key == 'input_tag':
            return str(unreal.GameplayTagLibrary.get_tag_name(value))
        return path(value)
    except Exception:
        return None


def component_rows(cdo):
    result = []
    try:
        components = cdo.get_components_by_class(unreal.ActorComponent)
    except Exception:
        return result
    for component in components:
        row = {'class': component.get_class().get_name(), 'name': component.get_name()}
        for key in ('static_mesh', 'skeletal_mesh', 'asset', 'template', 'auto_activate'):
            value = read(component, key)
            if value is not None:
                row[key] = value
        result.append(row)
    return result


report = {'status': 'running', 'abilities': [], 'configurations': {}, 'projectiles': {}, 'issues': []}
try:
    for hero, names in ABILITIES.items():
        config = unreal.load_asset('/Game/Abilities/Configurations/AC_' + hero)
        report['configurations'][hero] = [path(cls) for cls in config.get_editor_property('default_abilities')] if config else None
        for name in names:
            asset_path = '/Game/Abilities/' + hero + '/GA_' + hero + '_' + name
            asset = unreal.load_asset(asset_path)
            row = {'ability': asset_path, 'loaded': bool(asset)}
            if asset:
                cls = asset.generated_class()
                cdo = unreal.get_default_object(cls)
                row['class'] = path(cls)
                row['properties'] = {key: read(cdo, key) for key in FIELDS}
                pair = cdo.get_editor_property('cast_montages')
                row['cast_pair_distinct'] = len(pair) == 2 and bool(pair[0]) and bool(pair[1]) and pair[0] != pair[1]
                for key in ('grenade_class', 'projectile_class', 'wave_class', 'line_class', 'returning_verity_class'):
                    projectile_cls = cdo.get_editor_property(key) if read(cdo, key) else None
                    if projectile_cls and path(projectile_cls) not in report['projectiles']:
                        projectile_cdo = unreal.get_default_object(projectile_cls)
                        report['projectiles'][path(projectile_cls)] = {
                            'presentation': {field: read(projectile_cdo, field) for field in PROJECTILE_FIELDS},
                            'components': component_rows(projectile_cdo),
                        }
            report['abilities'].append(row)
    for row in report['abilities']:
        if not row['loaded']:
            report['issues'].append(row['ability'] + ': missing asset')
            continue
        props = row['properties']
        if not row['cast_pair_distinct']:
            report['issues'].append(row['ability'] + ': missing distinct A/B montages')
        if not props['cast_niagara_system']:
            report['issues'].append(row['ability'] + ': missing cast Niagara')
    for name, fields in REQUIRED_PROJECTILE_FX.items():
        key = '/Game/Abilities/Presentation/' + name + '.' + name + '_C'
        projectile = report['projectiles'].get(key)
        if projectile is None:
            report['issues'].append(key + ': missing ability projectile binding')
            continue
        for field in fields:
            if not projectile['presentation'][field]:
                report['issues'].append(key + ': missing ' + field)
    grenade = next(row for row in report['abilities'] if row['ability'].endswith('GA_Tarrik_CinderStickyGrenade'))
    wanted_weapons = ['/Game/Items/Weapons/WI_' + name + '.WI_' + name + '_C'
                      for name in ('Velkorran', 'Cinderline')]
    if grenade['properties']['allowed_weapon_classes'] != wanted_weapons:
        report['issues'].append(grenade['ability'] + ': stale weapon metadata')
    report['status'] = 'passed' if not report['issues'] else 'failed'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    OUT.write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('PLAYER_ABILITY_ASSET_AUDIT ' + report['status'])
