"""Read-only check that the Aurelion Elite's authored configuration carries its boss loadout.

A .uasset cannot be read as text, so this loads the authored definition and reports what its
ability configuration actually holds: the startup effects that set the boss health pool, and the
granted abilities. It writes nothing and spawns nothing.
"""
import json
import os
from pathlib import Path
import unreal

DEFINITION = '/Game/Aurelion/Enemies/NPC_AurelionElite.NPC_AurelionElite'
EXPECTED_EFFECTS = ('SovAurelionElitePoiseAttributes', 'SovAurelionEliteDurability')
EXPECTED_ABILITIES = ('SovGameplayAbility_AurelionEliteSlam', 'SovGameplayAbility_AurelionEliteLance',
                      'SovGameplayAbility_AurelionEliteSummon')


def _name(entry):
    if entry is None:
        return None
    try:
        return entry.get_name()
    except Exception:
        return str(entry)


def run(output_directory=None):
    out = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    out.mkdir(parents=True, exist_ok=True)
    report = dict(read_only=True, status='failed', definition=DEFINITION)
    try:
        definition = unreal.EditorAssetLibrary.load_asset(DEFINITION)
        assert definition is not None, 'The authored Elite definition did not load'
        configuration = definition.get_editor_property('ability_configuration')
        assert configuration is not None, 'The Elite definition has no ability configuration'
        report['ability_configuration'] = configuration.get_path_name()
        effects = [_name(e) for e in configuration.get_editor_property('startup_effects')]
        abilities = [_name(a) for a in configuration.get_editor_property('default_abilities')]
        report['startup_effects'] = effects
        report['default_abilities'] = abilities
        report['missing_effects'] = [e for e in EXPECTED_EFFECTS if not any(e in str(n) for n in effects)]
        report['missing_abilities'] = [a for a in EXPECTED_ABILITIES if not any(a in str(n) for n in abilities)]
        report['status'] = ('passed: the elite carries its boss durability and repertoire'
                            if not report['missing_effects'] and not report['missing_abilities']
                            else 'failed: authored configuration is missing entries')
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        (out / 'elite-ability-configuration.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf8')
        unreal.log('AURELION_ELITE_CONFIG ' + str(report['status']) + ' ' + str(out / 'elite-ability-configuration.json'))
    return report


if __name__ == '__main__':
    run()
