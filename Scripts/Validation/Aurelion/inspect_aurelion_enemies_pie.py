"""Read-only, importable M12 startup audit. Never initializes, equips or activates.

Call inspect_enemies(world, output_path, require_initialized=True) after the native
startup wait and before gameplay input. False leaves missing startup work pending.
The returned report contains strings/scalars only and retains no world references.
"""
import json
import math
from pathlib import Path
import re
import unreal

BASE = '/Game/Aurelion/Enemies/'
GROUPS = {
    'E1': [('Drone'+str(i), 'SecurityDrone', 0 if i <= 4 else 1) for i in range(1, 7)],
    'E2': [('Enforcer'+str(i), 'Enforcer', 0) for i in range(1, 5)] +
          [('Drone1', 'ContaminatedDrone', 0), ('Drone2', 'ContaminatedDrone', 0)],
    'E3': [('Linkbound1', 'Linkbound', 0), ('Linkbound2', 'Linkbound', 0),
           ('Linkbound3', 'Linkbound', 0), ('WallRunner', 'WallRunner', 0),
           ('Linkbound4', 'Linkbound', 1), ('Linkbound5', 'Linkbound', 1), ('Weaver', 'Weaver', 1)],
    'E4': [('Linkbound1', 'Linkbound', 0), ('Linkbound2', 'Linkbound', 0),
           ('Weaver', 'Weaver', 0), ('Elite', 'Elite', 0), ('WallRunner', 'WallRunner', 1)],
}
DIRECTORS = {'E1': 'M12_E1_PressureHall', 'E2': 'M12_E2_RelayOverlook',
             'E3': 'M12_E3_SharedBreach', 'E4A': 'M12_E4_QuarantineCrucibleA',
             'E4B': 'M12_E4_QuarantineCrucibleB'}


def _path(obj):
    return obj.get_path_name() if obj else None


def _encode(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, unreal.Object):
        return _path(value)
    if isinstance(value, (list, tuple, unreal.Array)):
        return [_encode(v) for v in value]
    if isinstance(value, (dict, unreal.Map)):
        return {str(k): _encode(v) for k, v in value.items()}
    if hasattr(value, 'export_text'):
        return value.export_text()
    return str(value)


def _reference(value):
    if isinstance(value, unreal.Object) or value is None:
        return value
    match = re.search(r'(/[A-Za-z0-9_/]+(?:\.[A-Za-z0-9_]+)?)', str(value))
    return unreal.load_object(None, match.group(1)) if match else None


def _capsule_pair(actor):
    def dimensions(owner):
        capsule = owner.get_editor_property('capsule_component')
        radius = float(capsule.get_scaled_capsule_radius())
        half_height = float(capsule.get_scaled_capsule_half_height())
        return {'component': _path(capsule), 'radius': radius, 'half_height': half_height,
                'height': 2*half_height, 'actor_scale': _encode(owner.get_actor_scale3d()),
                'valid': math.isfinite(radius) and math.isfinite(half_height) and 0 < radius <= half_height}
    live = dimensions(actor)
    cdo = dimensions(unreal.get_default_object(actor.get_class()))
    return {'live': live, 'cdo': cdo, 'radius_delta': live['radius']-cdo['radius'],
            'half_height_delta': live['half_height']-cdo['half_height'],
            'exceeds_cdo_dimensions': live['radius'] > cdo['radius']+.1 or live['half_height'] > cdo['half_height']+.1}


def inspect_enemies(world, output_path, require_initialized=True):
    """Inspect only the current PIE M12 world; never hold UObject references after return."""
    report = {'read_only': True, 'status': 'inspecting', 'world': _path(world),
              'expected_hostiles': 24, 'directors': {}, 'definitions': {}, 'hostiles': [],
              'contract_failures': [], 'startup_pending': [], 'inspection_errors': [], 'findings': [],
              'limits': ['Native IsEncounterSnapshotReady and IsThreatMemorySuspended are not reflected; no direct claim about those flags.',
                         'Held/reserved actors need not have a selected attack, wielded weapon, loaded clip, visible body or active activity tick.',
                         'Current-activity snapshots do not prove attacks, traversal, thermal payoff or death completion.'],
              'rendered_validation': False}

    def fail(message):
        report['contract_failures'].append(message)

    def startup(condition, message):
        if not condition:
            report['startup_pending'].append(message)

    def read(row, key, fn):
        try:
            value = fn()
            row[key] = _encode(value)
            return value
        except Exception as exc:
            message = key+': '+str(exc)
            row[key+'_inspection_error'] = str(exc)
            report['inspection_errors'].append(message)
            return None

    def definition_data(definition, role):
        key = _path(definition)
        if key in report['definitions']:
            return
        row = {'role': role, 'definition': key}
        report['definitions'][key] = row
        for field in ('npc_class_path', 'default_factions', 'default_item_loadout', 'default_appearance'):
            read(row, field, lambda field=field: definition.get_editor_property(field))
        ability = _reference(definition.get_editor_property('ability_configuration'))
        activity = _reference(definition.get_editor_property('activity_configuration'))
        row['ability_configuration'] = _path(ability)
        row['activity_configuration'] = _path(activity)
        if not ability or not activity:
            fail(role+': authored definition lacks its ability/activity configuration')
            return
        for field in ('default_attributes', 'default_abilities', 'startup_effects'):
            read(row, field, lambda field=field: ability.get_editor_property(field))
        for field in ('default_activities', 'goal_generators'):
            values = read(row, field, lambda field=field: activity.get_editor_property(field))
            if values is not None and not values:
                fail(role+': empty authored '+field)

    try:
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        package = re.sub(r'UEDPIE_\d+_', '', _path(world).split('.')[0]) if world else ''
        if not editor.is_in_play_in_editor() or world != current or package != '/Game/Aurelion/Maps/L_Aurelion_M12':
            raise RuntimeError('The current PIE M12 world is required; editor/foreign worlds are rejected')
        report['game_time'] = unreal.GameplayStatics.get_time_seconds(world)
        player = unreal.GameplayStatics.get_player_pawn(world, 0)
        report['player'] = _path(player)
        if player:
            read(report, 'player_capsule_comparison', lambda: _capsule_pair(player))
        report['navigation'] = [{'actor': _path(nav), 'radius': float(nav.get_editor_property('agent_radius')),
                                 'height': float(nav.get_editor_property('agent_height')),
                                 'runtime_generation': str(nav.get_editor_property('runtime_generation'))}
                                for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)]
        startup(isinstance(player, unreal.SovPlayerCharacterBase) and player.is_character_ready(),
                'Current managed player is not ready for the pre-input startup audit')
        expected = {group+'.'+suffix: (group, role, wave) for group, rows in GROUPS.items() for suffix, role, wave in rows}
        found_directors = {}
        memberships = {identity: [] for identity in expected}
        for director in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector):
            encounter_id = str(director.get_editor_property('encounter_id'))
            if encounter_id in DIRECTORS.values():
                found_directors.setdefault(encounter_id, []).append(director)
            entries = list(director.get_editor_property('participants'))
            seen_ids = set()
            for entry in entries:
                identity = str(entry.get_editor_property('participant_id'))
                if identity in seen_ids:
                    fail(encounter_id+': duplicate roster ID '+identity)
                seen_ids.add(identity)
                if identity in expected:
                    memberships[identity].append((director, entry.get_editor_property('character')))
                elif entry.get_editor_property('required_for_victory') and encounter_id in DIRECTORS.values():
                    fail(encounter_id+': unexpected victory participant '+identity)
            if encounter_id in DIRECTORS.values():
                coordination = director.get_coordination_component()
                report['directors'][encounter_id] = {
                    'actor': _path(director), 'state': str(director.get_encounter_state()),
                    'attempt_id': _encode(director.get_attempt_id()), 'participant_ids': sorted(seen_ids),
                    'hold_before_entry': director.get_editor_property('hold_participants_before_entry'),
                    'pre_entry_hold_error': director.get_editor_property('pre_entry_hold_error'),
                    'current_wave': coordination.get_current_wave() if coordination else None}
                if report['directors'][encounter_id]['pre_entry_hold_error']:
                    startup(False, encounter_id+': '+report['directors'][encounter_id]['pre_entry_hold_error'])
        for encounter_id in DIRECTORS.values():
            if len(found_directors.get(encounter_id, [])) != 1:
                fail('Expected one exact native director: '+encounter_id)

        actors_by_path = {}
        for identity, (group, role, wave) in expected.items():
            row = {'participant_id': identity, 'role': role, 'authored_wave': wave}
            report['hostiles'].append(row)
            entries = memberships[identity]
            row['registered_directors'] = [_path(d) for d, _ in entries]
            if not entries or any(not actor for _, actor in entries):
                fail(identity+': missing live registered actor')
                continue
            actor_paths = {_path(actor) for _, actor in entries}
            if len(actor_paths) != 1:
                fail(identity+': separate actors claim the same participant ID')
                continue
            actor = entries[0][1]
            actual_directors = {str(d.get_editor_property('encounter_id')) for d, _ in entries}
            allowed_directors = {DIRECTORS['E4A'], DIRECTORS['E4B']} if group == 'E4' else {DIRECTORS[group]}
            if not actual_directors.issubset(allowed_directors) or len(entries) > (2 if group == 'E4' else 1):
                fail(identity+': foreign or duplicate director membership')
            if group == 'E4' and len(entries) == 2:
                if sum(d.get_encounter_state() == unreal.SovEncounterState.ACTIVE for d, _ in entries) > 1:
                    fail(identity+': both Crucible phases simultaneously own an active roster')
                row['phase_membership'] = 'same actor shared by A/B; at most one active owner'
            actor_path = _path(actor)
            if actor_path in actors_by_path:
                fail(identity+': actor also occupies '+actors_by_path[actor_path])
            actors_by_path[actor_path] = identity
            row.update(actor=actor_path, actor_class=_path(actor.get_class()), position=_encode(actor.get_actor_location()))
            if actor.get_world() != world or not isinstance(actor, unreal.SovNPCCharacterBase):
                fail(identity+': actor is not a current-world native encounter NPC')
                continue
            capsules = read(row, 'capsule_comparison', lambda: _capsule_pair(actor))
            if capsules:
                if not capsules['live']['valid']:
                    fail(identity+': invalid actual live collision capsule dimensions')
                if capsules['exceeds_cdo_dimensions']:
                    report['findings'].append(identity+': actual initialized capsule exceeds its authored CDO; inspect exit/floor/navigation clearance')
                row['live_capsule_fits_nav_envelope'] = any(
                    capsules['live']['radius'] <= nav['radius']+.1 and capsules['live']['height'] <= nav['height']+.1
                    for nav in report['navigation'])
                if not row['live_capsule_fits_nav_envelope']:
                    report['findings'].append(identity+': no current Recast agent envelope includes the actual live capsule')
            class_path = BASE+'BP_Aurelion'+role+'.BP_Aurelion'+role+'_C'
            if row['actor_class'] != class_path:
                fail(identity+': wrong actual role class '+row['actor_class'])
            for director, _ in entries:
                if director.get_participant(unreal.Name(identity)) != actor or str(director.find_participant_id(actor)) != identity:
                    fail(identity+': native identity lookup disagrees with roster')
                if director.is_participant_mass_represented(unreal.Name(identity)):
                    fail(identity+': combat role was unexpectedly converted to Mass')
            row['hidden'] = read(row, 'hidden', lambda: actor.get_editor_property('hidden'))
            row['alive'] = actor.is_alive()
            health, maximum = actor.get_health(), actor.get_max_health()
            row.update(health=health, max_health=maximum)
            startup(row['alive'] and math.isfinite(health) and health > 0 and math.isfinite(maximum) and maximum > 0,
                    identity+': live startup health is not initialized')
            definition = actor.get_npc_definition()
            row['definition'] = _path(definition)
            startup(definition is not None, identity+': native NPC definition has not initialized')
            if definition and _path(definition) != BASE+'NPC_Aurelion'+role+'.NPC_Aurelion'+role:
                fail(identity+': unexpected native role definition '+_path(definition))
            controller = actor.get_controller()
            row['controller'] = _path(controller)
            row['controller_class'] = _path(controller.get_class()) if controller else None
            startup(isinstance(controller, unreal.NarrativeNPCController) and controller.get_controlled_pawn() == actor,
                    identity+': current Narrative controller possession is missing')
            expected_controller = unreal.get_default_object(actor.get_class()).get_editor_property('ai_controller_class')
            row['authored_controller_class'] = _path(expected_controller)
            if controller and expected_controller and controller.get_class() != expected_controller:
                fail(identity+': actual controller class differs from its authored role CDO')
            if role == 'ContaminatedDrone' and isinstance(controller, unreal.NarrativeNPCController):
                enabled = read(row, 'accept_network_threats', lambda: controller.get_editor_property('accept_network_threats'))
                if enabled is False:
                    fail(identity+': authored scanner receiver did not retain its network-sensor opt-in')
            asc = actor.get_narrative_ability_system_component()
            row['asc'] = _path(asc)
            startup(asc is not None and asc.get_avatar_owner() == actor, identity+': actual ASC avatar differs')
            visual = actor.get_character_visual()
            row['visual'] = _path(visual)
            startup(visual is not None, identity+': native visual is not loaded')
            pending_load = read(row, 'character_pending_load', actor.is_character_pending_load)
            if pending_load is not None:
                startup(not pending_load, identity+': native definition/appearance load is still pending')
            read(row, 'factions', lambda: unreal.ArsenalStatics.get_actor_factions(actor))
            if player:
                attitude = read(row, 'attitude_to_player', lambda: unreal.ArsenalStatics.get_attitude(actor, player))
                if attitude is not None and attitude != unreal.TeamAttitude.HOSTILE:
                    fail(identity+': actual current faction matrix does not classify the player as hostile')
            current_wave = max((d.get_coordination_component().get_current_wave() for d, _ in entries), default=0)
            held = all(d.get_encounter_state() == unreal.SovEncounterState.INACTIVE and d.get_editor_property('hold_participants_before_entry') for d, _ in entries)
            row['lifecycle_interpretation'] = 'pre-entry hold requested' if held else ('reserved future wave' if wave > current_wave else 'released/current wave')
            row['native_snapshot_ready_flag'] = 'unavailable: non-reflected C++ getter'
            if asc:
                abilities = []
                for handle in asc.get_all_abilities():
                    result = unreal.AbilitySystemLibrary.get_gameplay_ability_from_spec_handle(asc, handle)
                    candidates = result if isinstance(result, tuple) else (result,)
                    ability = next((v for v in candidates if isinstance(v, unreal.GameplayAbility)), None)
                    if ability:
                        abilities.append({'class': _path(ability.get_class()), 'object': _path(ability), 'handle': _encode(handle)})
                row['granted_abilities'] = abilities
                startup(bool(abilities), identity+': no actual granted abilities')
            if definition:
                definition_data(definition, role)
                required_abilities = report['definitions'][_path(definition)].get('default_abilities', [])
                actual_abilities = {a['class'] for a in row.get('granted_abilities', [])}
                missing = [path for path in required_abilities if path not in actual_abilities]
                row['missing_default_abilities'] = missing
                startup(not missing, identity+': missing configured ability grants '+repr(missing))
                activity_config = _reference(definition.get_editor_property('activity_configuration'))
                activities = actor.get_activity_component()
                startup(activities is not None, identity+': native activity component missing')
                if activities and activity_config:
                    row['activity_component'] = _path(activities)
                    row['activity_active'] = activities.is_active()
                    row['current_activity'] = _path(activities.get_current_activity())
                    row['current_goal'] = _path(activities.get_current_activity_goal())
                    row['activity_instances'] = [_path(activities.get_activity(cls)) for cls in activity_config.get_editor_property('default_activities')]
                    row['goal_generators'] = [_path(activities.get_goal_generator(cls)) for cls in activity_config.get_editor_property('goal_generators')]
                    startup(all(row['activity_instances']) and all(row['goal_generators']), identity+': configured activities/goal generators not installed')
            inventory = actor.get_inventory_component()
            startup(inventory is not None, identity+': native inventory missing')
            row['inventory'] = []
            if inventory:
                for item in inventory.get_items():
                    item_row = {'item': _path(item), 'class': _path(item.get_class()), 'quantity': item.get_quantity()}
                    if isinstance(item, unreal.WeaponItem):
                        item_row.update(equipped=item.is_equipped(), wielded=item.is_wielded(),
                                        loaded_ammo=item.get_ammo_in_clip(), spare_ammo=item.get_spare_ammo())
                    row['inventory'].append(item_row)
                expected_weapon = 'Weapon_DemoSword_C' if role in ('Linkbound', 'WallRunner', 'Elite') else 'Weapon_DemoPistol_C' if role in ('Enforcer', 'Weaver') else None
                if expected_weapon:
                    startup(any(item['class'].endswith('.'+expected_weapon) and item.get('equipped') for item in row['inventory']),
                            identity+': authored weapon is not present/equipped in its normal inventory')
            if isinstance(controller, unreal.NarrativeNPCController):
                read(row, 'current_tree', controller.get_current_tree)
        owned_actors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase)
                        if _path(a.get_class()).startswith(BASE+'BP_Aurelion')]
        report['actual_owned_enemy_actor_count'] = len(owned_actors)
        report['unique_roster_actor_count'] = len(actors_by_path)
        if len(owned_actors) != 24 or {_path(a) for a in owned_actors} != set(actors_by_path):
            fail('Actual owned enemy actors differ from the exact unique 24-actor roster')
        if len(report['definitions']) != 7:
            startup(False, 'All seven actual role definitions have not initialized')
    except Exception as exc:
        report['inspection_errors'].append(type(exc).__name__+': '+str(exc))
    report['status'] = ('failed' if report['contract_failures'] or (require_initialized and report['startup_pending'])
                        else 'incomplete_inspection' if report['inspection_errors']
                        else 'pending_initialization' if report['startup_pending'] else 'passed_observable_startup_contract')
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report
