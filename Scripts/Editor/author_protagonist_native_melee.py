"""Put Tarrik's and Selene's melee on the native melee framework (audit finding PC2-01).

Both protagonists attacked through Narrative Blueprint combos (GA_Attack_Melee_Sword_1H_Tarrik,
GA_SovVerityTwinAttack), so none of USovGameplayAbility_Melee's guarantees reached the player:
socket sweeps with substeps, the per-attack hit ledger, the cover lane, finite authored branches,
the 0.22 s buffer, node-specific defensive cancels, melee aim assist and the attack receipt that
Tarrik's heavy multi-hit Echo reward requires.

This authors a light and a heavy native attack graph per protagonist from the montages their
combos already play, and swaps the weapon grants. Re-running updates the same assets in place.

Timing comes from Scripts/Validation/Aurelion/probe_protagonist_blade_paths_readonly.py, which
evaluates each montage and measures blade-tip speed: every Active window below covers the measured
fast part of the swing. Blade edges are measured from the weapon meshes' bounds and bones and are
converted here to offsets from a static socket, so the sweep follows the real blade.
"""
import json
import os
from pathlib import Path
import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', str(PROJECT / 'Saved/Validation/Aurelion/ProtagonistMelee')))
RUN.mkdir(parents=True, exist_ok=True)
ROOT = '/Game/Aurelion/Characters/Melee/'
TARRIK_MONTAGES = '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/3P/'
SELENE_MONTAGES = '/Game/Characters/Animation/VerityTwinBlades/'
EVADE = '/Game/Abilities/Common/GA_Evade.GA_Evade_C'
OLD_GRANTS = {'GA_Attack_Melee_Sword_1H_Tarrik_C', 'GA_SovVerityTwinAttack_C', 'GA_Attack_Combo_Melee_C'}

T_ATTACK, T_HEAVY, T_EVADE = 'Narrative.Input.Attack', 'Narrative.Input.Attack.Heavy', 'Narrative.Input.Evade'
EDGE = 'Sov.Damage.Channel.Edge'
STANDARD, HEAVY_GUARD, HEAVY = 'Sov.Damage.GuardClass.Standard', 'Sov.Damage.GuardClass.Heavy', 'Sov.Damage.Heavy'
# Narrative's combos refused these; the native base does not, and a swing from a ledge or a swim is not a TDD attack.
EXTRA_BLOCKS = ['Narrative.State.Movement.Falling', 'Narrative.State.Movement.Climbing',
                'Narrative.State.Movement.Swimming', 'Narrative.State.Weapon.BlockFiring']

# Weapon-mesh component-space blade edges and the static socket they are expressed against.
# Tarrik: SK_Narrative_Sword_1h runs -27..+97 cm on Z with the grip at the origin; the blade is +Z past the guard.
# Selene: Verity is a double blade. Its blades are skinned to scaled blade bones, so the mesh bounds
# (-131..+116 cm on Z) give the tips; the upper blade starts above blade_upper (Z 14.4) and the lower
# below blade_lower (Z -28.8). 'root' is static (the blade bones animate when Verity deploys).
WEAPONS = {
    'Tarrik': {'item': '/Game/Items/Weapons/WI_Velkorran', 'visual_socket': 'Grip',
               'edges': [((0, 0, 15), (0, 0, 95))]},
    'Selene': {'item': '/Game/Items/Weapons/WI_Verity', 'visual_socket': 'root',
               'edges': [((0, 0, 18), (0, 0, 112)), ((0, 0, -32), (0, 0, -127))]},
}


def node(montage, startup, active, recovery, branch_open, branch_close, damage, poise, radius,
         classes, next_node=-1, follow=None, cancel_cost=None, aim=12.):
    return dict(montage=montage, startup=startup, active=active, recovery=recovery, branch_open=branch_open,
                branch_close=branch_close, damage=damage, poise=poise, radius=radius, classes=classes,
                next_node=next_node, follow=follow, cancel_cost=cancel_cost, aim=aim)


# Measured fast swing windows (tip speed, 0.05 s samples):
#   Attack_1 .30-.50, Attack_2 .33-.53, Attack_2_Variation_1 .33-.53, Attack_1_Variation_1 .25-.47
#   Twin_01 .20-.62, Twin_02 .15-.65, Twin_03 .42-.68, Twin_04 .22-.52
GRAPHS = {
    'Tarrik_MeleeLight': ('Tarrik', T_ATTACK, [
        # Committed links: follow-ups open only after the swing, and only the first two may evade out.
        node(TARRIK_MONTAGES + 'AM_Sword_3P_1H_Attack_1_Tarrik', .30, .20, .95, .55, 1.05, 50., 12., 12., [STANDARD], 1, T_ATTACK, 8.),
        node(TARRIK_MONTAGES + 'AM_Sword_3P_1H_Attack_2_Tarrik', .33, .20, .92, .58, 1.05, 50., 12., 12., [STANDARD], 2, T_ATTACK, 8.),
        node(TARRIK_MONTAGES + 'AM_Sword_3P_1H_Attack_2_Variation_1_Tarrik', .33, .20, .92, .58, 1.05, 55., 14., 12., [STANDARD], 3, T_ATTACK),
        node(TARRIK_MONTAGES + 'AM_Sword_3P_1H_Attack_1_Variation_1_Tarrik', .25, .22, 1.13, .47, .47, 65., 20., 12., [STANDARD]),
    ]),
    'Tarrik_MeleeHeavy': ('Tarrik', T_HEAVY, [
        # Heavy cleaves: wider trace, heavy guard class, poise pressure, no defensive exit.
        node(TARRIK_MONTAGES + 'AM_Sword_3P_1H_Attack_1_Variation_1_Tarrik', .25, .22, 1.13, .60, 1.20, 85., 35., 16., [HEAVY, HEAVY_GUARD], 1, T_HEAVY, None, 10.),
        node(TARRIK_MONTAGES + 'AM_Sword_3P_1H_Attack_2_Variation_1_Tarrik', .33, .20, 1.20, .53, .53, 105., 45., 16., [HEAVY, HEAVY_GUARD], aim=10.),
    ]),
    'Selene_MeleeLight': ('Selene', T_ATTACK, [
        # Rapid precision: every link may evade out cheaply.
        node(SELENE_MONTAGES + 'AM_VerityTwin_01', .20, .42, .58, .64, 1.20, 30., 6., 10., [STANDARD], 1, T_ATTACK, 4., 15.),
        node(SELENE_MONTAGES + 'AM_VerityTwin_02', .15, .50, .55, .67, 1.20, 30., 6., 10., [STANDARD], 2, T_ATTACK, 4., 15.),
        node(SELENE_MONTAGES + 'AM_VerityTwin_03', .42, .26, .52, .70, 1.20, 34., 8., 10., [STANDARD], 3, T_ATTACK, 4., 15.),
        node(SELENE_MONTAGES + 'AM_VerityTwin_04', .22, .30, 1.08, .52, .52, 42., 12., 10., [STANDARD], cancel_cost=4., aim=15.),
    ]),
    'Selene_MeleeHeavy': ('Selene', T_HEAVY, [
        node(SELENE_MONTAGES + 'AM_VerityTwin_03', .42, .26, .92, .68, .68, 60., 25., 12., [HEAVY, HEAVY_GUARD], aim=12.),
    ]),
}

report = {'status': 'running', 'assets': {}, 'weapons': {}, 'saved': []}
tools = unreal.AssetToolsHelpers.get_asset_tools()


def tag(name):
    result = unreal.GameplayTag()
    assert result.import_text('(TagName="{}")'.format(name)), 'Unknown gameplay tag ' + name
    return result


def container(names):
    result = unreal.GameplayTagContainer()
    text = '(GameplayTags=(' + ','.join('(TagName="{}")'.format(n) for n in names) + '))'
    assert result.import_text(text), 'Could not build tags ' + text
    return result


def tag_names(tags):
    import re
    return re.findall(r'TagName="([^"]+)"', tags.export_text())


def load_or_create(path, cls, factory):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    folder, name = path.rsplit('/', 1)
    asset = tools.create_asset(name, folder, cls, factory)
    assert asset, 'Could not create ' + path
    return asset


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()
    report['saved'].append(asset.get_path_name())


def socket_offsets(hero):
    """Blade edges converted from mesh component space into the chosen socket's space."""
    cfg = WEAPONS[hero]
    item_class = unreal.load_class(None, cfg['item'] + '.' + cfg['item'].rsplit('/', 1)[1] + '_C')
    visual_class = unreal.SystemLibrary.load_class_asset_blocking(
        unreal.get_default_object(item_class).get_editor_property('weapon_visual_class'))
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(visual_class, unreal.Vector(0, 0, -100000))
    try:
        mesh = actor.get_editor_property('weapon_mesh')
        socket = cfg['visual_socket']
        assert mesh.does_socket_exist(socket), hero + ' weapon mesh lacks ' + socket
        frame = mesh.get_socket_transform(socket, unreal.RelativeTransformSpace.RTS_COMPONENT)
        edges = []
        for start, end in cfg['edges']:
            local = [frame.inverse_transform_location(unreal.Vector(*p)) for p in (start, end)]
            # Round-trip check: the runtime resolves socket.TransformPosition(offset).
            for p, l in zip((start, end), local):
                back = frame.transform_location(l)
                assert (back - unreal.Vector(*p)).length() < .05, 'Offset round trip failed for ' + hero
            edges.append((socket, local[0], local[1]))
        report['weapons'].setdefault(hero, {})['trace'] = {
            'socket': socket, 'socket_component_transform': str(frame),
            'edges_component_space': cfg['edges'], 'edges_socket_space': [[str(a), str(b)] for _, a, b in edges]}
        return edges
    finally:
        actor.destroy_actor()


def author_definition(name, hero, spec, edges):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.SovMeleeAttackDefinition)
    definition = load_or_create(ROOT + 'DA_' + name, unreal.SovMeleeAttackDefinition, factory)
    evade = unreal.load_class(None, EVADE)
    nodes = []
    for index, n in enumerate(spec):
        montage = unreal.load_asset(n['montage'])
        assert montage, 'Missing montage ' + n['montage']
        total = n['startup'] + n['active'] + n['recovery']
        assert total <= float(montage.get_play_length()) + 1e-3, 'Node %d outlives its montage in %s' % (index, name)
        entry = unreal.SovMeleeAttackNode()
        socket, start, end = edges[0]
        values = {'montage': montage, 'start_socket': socket, 'end_socket': socket, 'start_offset': start, 'end_offset': end,
                  'trace_radius': n['radius'], 'startup': n['startup'], 'active': n['active'], 'recovery': n['recovery'],
                  'branch_open': n['branch_open'], 'branch_close': n['branch_close'], 'hit_confirm_advance': .05,
                  'next_node': n['next_node'], 'damage': n['damage'], 'poise_damage': n['poise'],
                  'damage_channels': container([EDGE]), 'attack_classifications': container(n['classes']),
                  'maximum_aim_correction': n['aim']}
        if n['follow']:
            values['follow_up_input'] = tag(n['follow'])
        if n['cancel_cost'] is not None:
            values.update(defensive_input=tag(T_EVADE), defensive_ability=evade, defensive_cancel_cost=n['cancel_cost'])
        extra = []
        for socket_name, a, b in edges[1:]:
            segment = unreal.SovMeleeTraceSegment()
            for k, v in {'start_socket': socket_name, 'end_socket': socket_name, 'start_offset': a, 'end_offset': b}.items():
                segment.set_editor_property(k, v)
            extra.append(segment)
        values['additional_segments'] = extra
        for key, value in values.items():
            entry.set_editor_property(key, value)
        nodes.append(entry)
    definition.set_editor_property('nodes', nodes)
    valid = definition.validate()
    # UE Python returns the out Error on success ('' here) and None when Validate returns false.
    assert valid == '', 'Definition %s failed native validation: %r' % (name, valid)
    save(definition)
    return definition


def author_ability(name, input_tag, definition):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.SovGameplayAbility_Melee)
    blueprint = load_or_create(ROOT + 'GA_' + name, unreal.Blueprint, factory)
    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property('attack_definition', definition)
    cdo.set_editor_property('allow_unarmed', False)
    cdo.set_editor_property('input_tag', tag(input_tag))
    blocked = tag_names(cdo.get_editor_property('activation_blocked_tags'))
    cdo.set_editor_property('activation_blocked_tags', container(blocked + [t for t in EXTRA_BLOCKS if t not in blocked]))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)
    return blueprint.generated_class()


def regrant(hero, light, heavy):
    cfg = WEAPONS[hero]
    blueprint = unreal.load_asset(cfg['item'])
    cdo = unreal.get_default_object(blueprint.generated_class())
    record = {}
    for field in ('weapon_abilities', 'mainhand_weapon_abilities', 'equipment_abilities'):
        grants = list(cdo.get_editor_property(field))
        before = [g.get_name() for g in grants]
        kept = [g for g in grants if g.get_name() not in OLD_GRANTS and g not in (light, heavy)]
        if field != 'equipment_abilities' and before:
            kept = [light, heavy] + kept
        cdo.set_editor_property(field, kept)
        record[field] = {'before': before, 'after': [g.get_name() for g in kept]}
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)
    report['weapons'].setdefault(hero, {})['grants'] = record


try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    edges = {hero: socket_offsets(hero) for hero in WEAPONS}
    classes = {}
    for name, (hero, input_tag, spec) in GRAPHS.items():
        definition = author_definition(name, hero, spec, edges[hero])
        classes[name] = author_ability(name, input_tag, definition)
        report['assets'][name] = {'definition': definition.get_path_name(), 'ability': classes[name].get_path_name(),
                                  'input': input_tag, 'nodes': len(spec)}
    regrant('Tarrik', classes['Tarrik_MeleeLight'], classes['Tarrik_MeleeHeavy'])
    regrant('Selene', classes['Selene_MeleeLight'], classes['Selene_MeleeHeavy'])
    # Mission snapshots only admit explicitly curated grants. Keep the primary
    # attack in sync when replacing the weapon's old Blueprint combo.
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from align_companion_primary_grants import align
    report['companion_primary_alignment'] = [row for hero in WEAPONS
        for row in align(hero, RUN / 'companion-grant-backups')]
    report['status'] = 'authored_requires_play_validation'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    (RUN / 'protagonist-native-melee.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf-8')
    unreal.log('PROTAGONIST_NATIVE_MELEE ' + report['status'])
