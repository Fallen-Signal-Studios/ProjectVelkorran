"""Read-only: which clause of the sticky grenade's payload requirement fails."""
import json, os
from pathlib import Path
import unreal

RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'grenade-payload.json'
FIELDS = ['grenade_class', 'explosion_damage_effect_class', 'burn_effect_class', 'fuse_duration',
          'explosion_radius', 'explosion_damage', 'explosion_poise_damage',
          'minimum_explosion_damage_fraction', 'burn_damage_per_tick', 'burn_duration',
          'grenade_fallback_spawn_offset', 'grenade_aim_trace_distance', 'default_grenade_launch_speed',
          'grenade_fallback_throw_pitch', 'grenade_gravity_scale', 'maximum_grenade_launch_speed',
          'maximum_grenade_spawn_distance', 'payload_release_delay', 'post_release_recovery',
          'maximum_active_duration', 'echo_cost', 'minimum_echo_required', 'grenade_throw_socket_name']
report = {'status': 'running', 'values': {}, 'failing': []}
try:
    bp = unreal.load_asset('/Game/Abilities/Tarrik/GA_Tarrik_CinderStickyGrenade')
    cdo = unreal.get_default_object(bp.generated_class())
    v = {}
    for f in FIELDS:
        try:
            raw = cdo.get_editor_property(f)
            v[f] = raw.get_name() if isinstance(raw, unreal.Object) else str(raw)
        except Exception as exc:
            v[f] = 'unreadable: ' + type(exc).__name__
    report['values'] = v
    def num(f):
        try:
            return float(cdo.get_editor_property(f))
        except Exception:
            return None
    checks = [
        ('FuseDuration > 0', num('fuse_duration')),
        ('ExplosionRadius > 0', num('explosion_radius')),
        ('ExplosionDamage > 0', num('explosion_damage')),
        ('BurnDamagePerTick > 0', num('burn_damage_per_tick')),
        ('BurnDuration > 0', num('burn_duration')),
        ('GrenadeAimTraceDistance > 0', num('grenade_aim_trace_distance')),
        ('DefaultGrenadeLaunchSpeed > 0', num('default_grenade_launch_speed')),
        ('MaximumGrenadeSpawnDistance > 0', num('maximum_grenade_spawn_distance')),
    ]
    for label, value in checks:
        if value is None or value <= 1e-4:
            report['failing'].append({'clause': label, 'value': value})
    mx, df = num('maximum_grenade_launch_speed'), num('default_grenade_launch_speed')
    if mx is not None and df is not None and mx + 1e-4 < df:
        report['failing'].append({'clause': 'MaximumGrenadeLaunchSpeed >= DefaultGrenadeLaunchSpeed', 'value': [mx, df]})
    md, pr, po = num('maximum_active_duration'), num('payload_release_delay'), num('post_release_recovery')
    if None not in (md, pr, po) and md < pr + po:
        report['failing'].append({'clause': 'MaximumActiveDuration >= PayloadReleaseDelay + PostReleaseRecovery',
                                  'value': [md, pr, po]})
    report['status'] = 'read'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    OUT.write_text(json.dumps(report, indent=2, default=str), encoding='utf-8')
    unreal.log('GRENADE_PAYLOAD ' + report['status'])
