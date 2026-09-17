"""Read-only dump of what the player's input and ability grants actually contain.

Answers, without starting play: which keys IMC_Combat maps and with which triggers, which
semantic tag every DA_CombatInputs row carries, and which input tag each authored grenade and
protagonist ability declares. Saves nothing and modifies nothing.
"""
import json
import os
from pathlib import Path
import unreal

RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'input-and-grants.json'
CONTEXT = '/Game/Input/IMC_Combat'
SCHEMA = '/Game/Input/DA_CombatInputs'
CONFIGS = ['/Game/Abilities/Configurations/AC_Tarrik', '/Game/Abilities/Configurations/AC_Selene']
ABILITIES = [
    '/Game/Abilities/Tarrik/GA_Tarrik_CinderStickyGrenade',
    '/Game/Abilities/Selene/GA_Selene_StillpointGrenade',
]

report = {'status': 'running'}


def name(obj):
    return obj.get_name() if obj else None


def classes(array):
    return [name(entry.get_class()) for entry in array] if array else []


try:
    context = unreal.load_asset(CONTEXT)
    data = context.get_editor_property('default_key_mappings')
    rows = []
    for mapping in data.get_editor_property('mappings'):
        rows.append({
            'action': name(mapping.get_editor_property('action')),
            'key': str(mapping.get_editor_property('key').get_editor_property('key_name')),
            'triggers': classes(mapping.get_editor_property('triggers')),
            'modifiers': classes(mapping.get_editor_property('modifiers')),
        })
    report['context'] = {'path': context.get_path_name(), 'mappings': rows, 'count': len(rows),
                         'deprecated_rows': len(list(context.get_editor_property('mappings')))}

    schema = unreal.load_asset(SCHEMA)
    schema_rows = []
    for row in schema.get_editor_property('input_abilities'):
        action = row.get_editor_property('input_action')
        schema_rows.append({
            'action': name(action),
            'tag': str(unreal.GameplayTagLibrary.get_tag_name(row.get_editor_property('input_tag'))),
            'required_modifier': str(unreal.GameplayTagLibrary.get_tag_name(row.get_editor_property('required_modifier_tag'))),
            'modified_tag': str(unreal.GameplayTagLibrary.get_tag_name(row.get_editor_property('modified_input_tag'))),
            'action_triggers': classes(action.get_editor_property('triggers')) if action else None,
        })
    report['schema'] = {'path': schema.get_path_name(), 'rows': schema_rows, 'count': len(schema_rows)}

    # Every input action an action row names must also be reachable from a key.
    keyed = {row['action'] for row in rows}
    report['schema_rows_without_a_key'] = [row['action'] for row in schema_rows if row['action'] not in keyed]

    report['ability_configurations'] = {}
    for path in CONFIGS:
        config = unreal.load_asset(path)
        if not config:
            report['ability_configurations'][path] = 'missing'
            continue
        granted = []
        for cls in config.get_editor_property('default_abilities'):
            cdo = unreal.get_default_object(cls) if cls else None
            granted.append({'class': name(cls),
                            'input_tag': str(unreal.GameplayTagLibrary.get_tag_name(cdo.get_editor_property('input_tag'))) if cdo else None})
        report['ability_configurations'][path] = granted

    report['abilities'] = {}
    for path in ABILITIES:
        asset = unreal.load_asset(path)
        if not asset:
            report['abilities'][path] = 'missing'
            continue
        generated = asset.generated_class() if hasattr(asset, 'generated_class') else None
        cdo = unreal.get_default_object(generated) if generated else None
        report['abilities'][path] = {
            'class': name(generated),
            'input_tag': str(unreal.GameplayTagLibrary.get_tag_name(cdo.get_editor_property('input_tag'))) if cdo else None,
        }
    report['status'] = 'read'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    OUT.write_text(json.dumps(report, indent=2, default=str), encoding='utf-8')
    unreal.log('INPUT_AND_GRANTS ' + report['status'])
