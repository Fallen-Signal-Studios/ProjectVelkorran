"""Bind hard lock-on and target cycling to real keys (audit finding PC2-07).

USovTargetingComponent only ever hears the semantic tags Narrative.Input.ThreatFocus,
Narrative.Input.CycleTargetLeft and Narrative.Input.CycleTargetRight, and nothing in the
project produced them: DA_CombatInputs had no entry and IMC_Combat had no key. So the
component was unreachable no matter what the player pressed.

This authors one Input Action per semantic tag beside the project's own IA_Evade/IA_Grenade,
adds the mapping rows to DA_CombatInputs (which BP_SovPlayerController already uses as
its AbilityInputMappings), and maps keyboard/mouse and gamepad keys in IMC_Combat (its
DefaultMappingContext). Keys come from a preference list and a key already bound in the
context is never taken; if a whole preference list is exhausted the run fails rather than
silently stealing an existing binding. Re-running updates the same rows in place.
"""
import json
import os
from pathlib import Path
import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', str(PROJECT / 'Saved/Validation/Aurelion/ThreatFocusInputs')))
RUN.mkdir(parents=True, exist_ok=True)
ROOT = '/Game/Input/'
SEED = '/Game/Input/IA_Evade'
MAPPINGS = '/Game/Input/DA_CombatInputs'
CONTEXT = '/Game/Input/IMC_Combat'

# Semantic tag, asset name, display name, and the keys to try in order (keyboard/mouse, then gamepad).
# IMC_Combat's gamepad face, shoulder, trigger, stick and D-pad buttons are all spoken for, so the
# gamepad list is allowed to come up empty: taking a key from an existing action is a design decision,
# not an engineering one. The report names every action left without a pad key.
ACTIONS = [
    ('Narrative.Input.ThreatFocus', 'IA_ThreatFocus', 'Threat Focus',
     ['ThumbMouseButton', 'MiddleMouseButton', 'CapsLock'], ['Gamepad_RightThumbstick', 'Gamepad_LeftShoulder']),
    ('Narrative.Input.CycleTargetLeft', 'IA_CycleTargetLeft', 'Cycle Threat Left',
     ['LeftBracket', 'Z', 'Comma'], ['Gamepad_DPad_Left']),
    ('Narrative.Input.CycleTargetRight', 'IA_CycleTargetRight', 'Cycle Threat Right',
     ['RightBracket', 'B', 'Period'], ['Gamepad_DPad_Right']),
    ('Narrative.Input.Designate', 'IA_Designate', 'Designate Target',
     ['ThumbMouseButton2', 'Z', 'Backslash'], ['Gamepad_RightThumbstick', 'Gamepad_LeftShoulder']),
]

report = {'status': 'running', 'actions': {}, 'context_before': [], 'context_after': [], 'saved': []}


def tag(name):
    result = unreal.GameplayTag()
    assert result.import_text('(TagName="{}")'.format(name)), 'Unknown gameplay tag ' + name
    return result


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()
    path = asset.get_path_name()
    if path not in report['saved']:
        report['saved'].append(path)


def key_name(key):
    return str(key.get_editor_property('key_name'))


def make_key(name):
    key = unreal.Key()
    key.set_editor_property('key_name', unreal.Name(name))
    return key


def author_action(name, display):
    """Duplicate the project's own boolean action so triggers and settings match what already works."""
    path = ROOT + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        action = unreal.load_asset(path)
    else:
        assert unreal.EditorAssetLibrary.duplicate_asset(SEED, path), 'Could not duplicate ' + SEED
        action = unreal.load_asset(path)
    assert isinstance(action, unreal.InputAction), path + ' is not an Input Action'
    action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
    settings = action.get_editor_property('player_mappable_key_settings')
    if settings:
        settings.set_editor_property('name', unreal.Name(name))
        settings.set_editor_property('display_name', unreal.Text(display))
    save(action)
    return action


def bind_semantic_tag(asset, action, tag_name):
    rows = list(asset.get_editor_property('input_abilities'))
    for row in rows:
        if row.get_editor_property('input_action') == action:
            row.set_editor_property('input_tag', tag(tag_name))
            asset.set_editor_property('input_abilities', rows)
            return 'updated'
    row = unreal.AbilityInputMappingData()
    row.set_editor_property('input_action', action)
    row.set_editor_property('input_tag', tag(tag_name))
    rows.append(row)
    asset.set_editor_property('input_abilities', rows)
    return 'added'


def live_mappings(context):
    """UE 5.7 keeps the live rows in DefaultKeyMappings; UInputMappingContext::Mappings is deprecated."""
    return list(context.get_editor_property('default_key_mappings').get_editor_property('mappings'))


def store_mappings(context, mappings):
    data = context.get_editor_property('default_key_mappings')
    data.set_editor_property('mappings', mappings)
    context.set_editor_property('default_key_mappings', data)


def drop_deprecated_rows(context, actions):
    """Clear rows an earlier run of this script wrote to the deprecated array, which nothing reads."""
    stale = list(context.get_editor_property('mappings'))
    kept = [m for m in stale if m.get_editor_property('action') not in actions]
    if len(kept) != len(stale):
        context.set_editor_property('mappings', kept)
    return len(stale) - len(kept)


def describe(context):
    described = []
    for mapping in live_mappings(context):
        action = mapping.get_editor_property('action')
        described.append({'action': action.get_name() if action else None, 'key': key_name(mapping.get_editor_property('key'))})
    return described


def map_keys(context, action, preferences):
    """Take the first preference no other action already uses, and never displace an existing binding."""
    mappings = live_mappings(context)
    taken = {key_name(m.get_editor_property('key')) for m in mappings if m.get_editor_property('action') != action}
    mine = [m for m in mappings if m.get_editor_property('action') == action]
    chosen = []
    for candidates, required in preferences:
        already = next((key_name(m.get_editor_property('key')) for m in mine
                        if key_name(m.get_editor_property('key')) in candidates), None)
        if already:
            chosen.append(already)
            continue
        free = next((name for name in candidates if name not in taken), None)
        if not free and not required:
            continue
        assert free, 'Every candidate key is already bound in IMC_Combat: ' + ', '.join(candidates)
        mapping = unreal.EnhancedActionKeyMapping()
        mapping.set_editor_property('action', action)
        mapping.set_editor_property('key', make_key(free))
        mappings.append(mapping)
        taken.add(free)
        chosen.append(free)
    store_mappings(context, mappings)
    return chosen


try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    schema = unreal.load_asset(MAPPINGS)
    context = unreal.load_asset(CONTEXT)
    assert schema and context, 'The project input assets must exist'
    report['context_before'] = describe(context)
    for tag_name, name, display, keyboard, gamepad in ACTIONS:
        action = author_action(name, display)
        row = bind_semantic_tag(schema, action, tag_name)
        keys = map_keys(context, action, [(keyboard, True), (gamepad, False)])
        report['actions'][name] = {'tag': tag_name, 'row': row, 'keys': keys, 'asset': action.get_path_name(),
                                   'gamepad': any(key.startswith('Gamepad') for key in keys)}
    report['deprecated_rows_removed'] = drop_deprecated_rows(context, [unreal.load_asset(ROOT + name) for _, name, _, _, _ in ACTIONS])
    save(schema)
    save(context)
    report['context_after'] = describe(context)
    report['status'] = 'authored_requires_play_validation'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    (RUN / 'threat-focus-inputs.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf-8')
    unreal.log('THREAT_FOCUS_INPUTS ' + report['status'])
