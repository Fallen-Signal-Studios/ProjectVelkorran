"""Validate the approved Aurelion layout's authoring contracts on a host without Unreal.

Checks dimensions, connected physical topology, sequential perspective transitions,
clock accounting, bounded reinforcements, phase/choice/checkpoint references and
shared-first build order. Passing is not evidence of authored assets or runtime play.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / 'Scripts/Manifests/AurelionFullLayout-2026-09-07.json'
ZONE_IDS = [f'Z{i:02}' for i in range(13)]
CHECKPOINT_IDS = ['CP0', 'CP1', 'CP2', 'CP2b', 'CP3', 'CP4', 'CP4b', 'CP5', 'CP5b', 'CP6', 'CP7', 'CP8', 'CP9']
ENCOUNTER_BUDGETS = {'E1': (6, 4), 'E2': (6, 6), 'E3': (7, 6), 'E4': (5, 5)}
ENCOUNTER_ROSTERS = {'E1': {'ReformationSecurityDrone': 6}, 'E2': {'DominionEnforcer': 4, 'ContaminatedReformationDrone': 2}, 'E3': {'Linkbound': 5, 'WallRunner': 1, 'Weaver': 1}, 'E4': {'Linkbound': 2, 'Weaver': 1, 'WallRunner': 1, 'Elite': 1}}
CHECKPOINT_ZONES = dict(zip(CHECKPOINT_IDS, ['Z00', 'Z01', 'Z03', 'Z04', 'Z05', 'Z06', 'Z07', 'Z08', 'Z08', 'Z09', 'Z10', 'Z11', 'Z12']))


def number(value, positive=False):
    return type(value) in (float, int) and math.isfinite(value) and (not positive or value > 0)


def validate_layout(layout, native_source=None):
    errors = []

    def require(condition, message):
        if not condition:
            errors.append(message)
        return bool(condition)

    def index(rows, key, label):
        if not require(isinstance(rows, list) and bool(rows), label + ' must be a nonempty list.'):
            return {}
        result = {}
        for row in rows:
            if not require(isinstance(row, dict) and isinstance(row.get(key), str) and bool(row[key]), label + ' needs stable string ' + key + ' values.'):
                continue
            identity = row[key]
            require(identity.casefold() not in {x.casefold() for x in result}, label + ': duplicate identifier ' + identity)
            result[identity] = row
        return result

    if not require(isinstance(layout, dict), 'Layout must be an object.'):
        return errors
    require(type(layout.get('schema_version')) is int and layout['schema_version'] == 1, 'Expected layout schema version 1.')
    require(layout.get('units') == {'distance': 'meters', 'unreal_units_per_meter': 100}, 'Dimensions must use meters and 100 Unreal units per meter.')
    qualification = layout.get('qualification', {})
    require(qualification == {'unreal_authored': False, 'native_automation_run': False, 'gameplay_measured': False, 'packaged_route_verified': False}, 'This authoring manifest cannot claim engine or gameplay qualification.')
    zones = index(layout.get('zones'), 'id', 'zones')
    require(list(zones) == ZONE_IDS, 'Expected ordered zones Z00 through Z12 exactly once.')
    for zone_id, zone in zones.items():
        geom = zone.get('geometry', {})
        if not isinstance(geom, dict):
            require(False, zone_id + ': geometry must be an object.'); continue
        shape = geom.get('shape')
        dimensions = ('width_m', 'depth_m') if shape == 'rectangle' else ('diameter_m',) if shape == 'circle' else ()
        require(bool(dimensions), zone_id + ': shape must be rectangle or circle.')
        for key in dimensions:
            require(number(geom.get(key), True), zone_id + ': ' + key + ' must be finite and positive.')
        elevation = zone.get('elevation', {})
        if not isinstance(elevation, dict):
            require(False, zone_id + ': elevation must be an object.'); continue
        low, high = elevation.get('min_m'), elevation.get('max_m')
        require((low is None and high is None) or (number(low) and number(high) and low <= high), zone_id + ': elevation bounds must be ordered finite values, or both unspecified.')
        require(bool(elevation.get('datum')) and bool(elevation.get('description')), zone_id + ': retain elevation datum and source description.')

    edges = layout.get('physical_edges', [])
    adjacency = {zone: [] for zone in zones}
    seen_edges = set()
    if not isinstance(edges, list):
        require(False, 'physical_edges must be a list.'); edges = []
    for edge in edges:
        if not require(isinstance(edge, list) and len(edge) == 2 and all(isinstance(x, str) and x in zones for x in edge), 'Physical edge references an unknown zone.'):
            continue
        a, b = edge
        require(a != b and (a, b) not in seen_edges, 'Physical edges cannot be self loops or duplicates.')
        seen_edges.add((a, b)); adjacency[a].append(b)
    require(layout.get('physical_entries') == ['Z00', 'Z03'] and layout.get('physical_exit') == 'Z12', 'Both independent entrances must reach the common departure.')
    reachable = set()
    for entry in ('Z00', 'Z03'):
        pending, visited = [entry], set()
        while pending:
            node = pending.pop()
            if node in visited:
                continue
            visited.add(node); pending.extend(adjacency.get(node, []))
        reachable |= visited
        require('Z05' in visited and 'Z12' in visited, entry + ' must reach the shared atrium and departure.')
    require(reachable == set(zones), 'Physical topology contains unreachable zones.')
    require(('Z02', 'Z05') in seen_edges and ('Z04', 'Z05') in seen_edges and ('Z02', 'Z03') not in seen_edges, 'The separate entrances converge at Z05; Z02 to Z03 is a perspective cut, not a doorway.')
    # Cycle detection matters: the mandatory physical route must not deadlock in a gate loop.
    indegrees = Counter(b for _, b in seen_edges)
    ready = [z for z in zones if indegrees[z] == 0]
    removed = set()
    while ready:
        node = ready.pop(); removed.add(node)
        for nxt in adjacency[node]:
            indegrees[nxt] -= 1
            if indegrees[nxt] == 0:
                ready.append(nxt)
    require(removed == set(zones), 'Physical mandatory topology must be acyclic; optional loops belong to optional pockets.')
    require(layout.get('play_order') == ZONE_IDS, 'Sequential player order must cover Z00 through Z12.')
    cuts = layout.get('perspective_cuts', [])
    require(cuts == [{'from_zone': 'Z02', 'to_zone': 'Z03', 'beat': 'HandoffToSelene', 'physical_connection': False}], 'Preserve the explicit separate-entrance perspective cut.')

    streaming = layout.get('streaming', {})
    chunks = index(streaming.get('chunks') if isinstance(streaming, dict) else None, 'id', 'streaming chunks')
    require(list(chunks) == list('ABCDEF'), 'Expected streaming chunks A through F.')
    chunk_zones = [z for row in chunks.values() for z in row.get('zones', [])]
    require(Counter(chunk_zones) == Counter(list(zones)), 'Every zone belongs to exactly one streaming chunk.')
    for chunk_id, row in chunks.items():
        for z in row.get('zones', []):
            require(z in zones and zones[z].get('streaming_chunk') == chunk_id, 'Zone and chunk membership disagree: ' + str(z))
    for flag in ('prestream_destination_before_transition', 'retain_previous_until_checkpoint_committed', 'state_survives_unload'):
        require(isinstance(streaming, dict) and streaming.get(flag) is True, 'Streaming requires ' + flag + '.')
    passes = layout.get('build_passes', [])
    require(isinstance(passes, list) and len(passes) >= 2 and passes[0].get('zones') == ZONE_IDS[5:] and passes[1].get('zones') == ZONE_IDS[:5], 'Build shared zones Z05-Z12 first, then entrances Z00-Z04.')

    clock = layout.get('clock', {})
    if not isinstance(clock, dict):
        require(False, 'clock must be an object.'); clock = {}
    segments = clock.get('segments', [])
    total = 0; timed_zones = []
    if not isinstance(segments, list):
        require(False, 'Clock segments must be a list.'); segments = []
    for segment in segments:
        if not require(isinstance(segment, dict) and number(segment.get('minutes'), True), 'Clock segments require finite positive minutes.'):
            continue
        allocation = segment.get('allocation', [])
        valid = isinstance(allocation, list) and all(isinstance(a, dict) and number(a.get('minutes'), True) for a in allocation)
        require(valid and math.isclose(sum(a['minutes'] for a in allocation) if valid else 0, segment['minutes']), 'Segment activity times must sum to the segment duration.')
        total += segment['minutes']; timed_zones.extend(segment.get('zones', []))
    require(total == clock.get('mandatory_minutes') == 50, 'Mandatory route must total 50 minutes; do not add the local decision twice.')
    require(timed_zones == ZONE_IDS, 'Every mandatory zone appears in one clock segment in play order.')
    pockets = index(clock.get('optional_pockets'), 'id', 'optional pockets')
    require(list(pockets) == ['O1', 'O2', 'O3', 'O4'], 'Expected optional pockets O1 through O4.')
    optional = 0
    for pocket_id, pocket in pockets.items():
        if number(pocket.get('minutes'), True):
            optional += pocket['minutes']
        else:
            require(False, pocket_id + ': optional time must be finite and positive.')
        require(pocket.get('zone') in zones and pocket.get('return_zone') == pocket.get('zone') and pocket.get('required_for_progression') is False, pocket_id + ': optional pocket must rejoin its own zone without gating progress.')
    require([pockets.get(i, {}).get('minutes') for i in ('O1', 'O2', 'O3', 'O4')] == [2, 2, 1, 3], 'Optional durations must remain 2, 2, 1, 3 minutes.')
    require(optional == clock.get('optional_minutes') == 8 and total + optional == clock.get('all_content_minutes') == 58, 'Complete route target must total 58 minutes including eight optional minutes.')

    missions = index(layout.get('missions'), 'mission_id', 'missions')
    require(set(missions) == {'M12_FireAndFrost', 'M13_ContraryWitness'}, 'Full layout binds M12 and M13, never the technical preparation mission.')
    beats = {}
    for mission_id, mission in missions.items():
        beats[mission_id] = index(mission.get('beats'), 'beat_id', mission_id + ' beats')
        for beat in beats[mission_id].values():
            require(beat.get('zone') in zones, 'Beat references an unknown zone: ' + beat['beat_id'])
    encounters = index(layout.get('encounters'), 'id', 'encounters')
    require(list(encounters) == list(ENCOUNTER_BUDGETS), 'Four physical encounters E1-E4 are required; E4 phases are not extra encounters.')
    for encounter_id, encounter in encounters.items():
        expected = ENCOUNTER_BUDGETS.get(encounter_id)
        cap, committed = encounter.get('maximum_active'), encounter.get('committed_total')
        require(expected == (committed, cap), encounter_id + ': committed/active budget changed.')
        require(encounter.get('zone') in ('Z01', 'Z04', 'Z06', 'Z08') and encounter.get('runtime_qualified') is False, encounter_id + ': combat must remain before quarantine and unqualified.')
        roster = encounter.get('roster', {})
        valid_roster = isinstance(roster, dict) and bool(roster) and all(type(v) is int and v > 0 for v in roster.values())
        require(valid_roster and sum(roster.values()) == committed, encounter_id + ': roster must match committed count.')
        require(roster == ENCOUNTER_ROSTERS.get(encounter_id), encounter_id + ': role composition must match the adopted layout.')
        wave_totals = Counter()
        waves = index(encounter.get('waves'), 'id', encounter_id + ' waves')
        for wave_id, wave in waves.items():
            members = wave.get('roster', {})
            gate = wave.get('gate', {})
            valid = isinstance(members, dict) and bool(members) and all(type(v) is int and v > 0 for v in members.values())
            require(valid, encounter_id + '/' + wave_id + ': wave needs real positive actor counts.')
            if valid:
                wave_totals.update(members)
            before = gate.get('maximum_alive_before_spawn') if isinstance(gate, dict) else None
            require(type(before) is int and before >= 0 and type(cap) is int and valid and before + sum(members.values()) <= cap, encounter_id + '/' + wave_id + ': releasing this wave could exceed the active cap.')
            require(isinstance(gate, dict) and gate.get('operator') == 'all' and isinstance(gate.get('all_events'), list) and bool(gate['all_events']), encounter_id + '/' + wave_id + ': wave conditions must be conjunctive and nonempty.')
        require(valid_roster and wave_totals == Counter(roster), encounter_id + ': wave rosters must commit every hostile exactly once.')
        first = next(iter(waves.values()), {})
        require(sum(first.get('roster', {}).values()) == {'E1': 4, 'E2': 6, 'E3': 4, 'E4': 4}.get(encounter_id), encounter_id + ': initial active roster changed.')
        mission_beats = beats.get(encounter.get('mission_id'), {})
        for beat_id in encounter.get('completion_beats', []):
            require(beat_id in mission_beats and mission_beats[beat_id].get('zone') == encounter.get('zone'), encounter_id + ': completion beat/zone binding is invalid.')
        bindings = encounter.get('native_bindings', [])
        require(isinstance(bindings, list) and bool(bindings), encounter_id + ': native encounter bindings are required.')
        for binding in bindings:
            require(isinstance(binding, dict) and binding.get('beat_id') in mission_beats and isinstance(binding.get('encounter_id'), str), encounter_id + ': invalid native binding.')
    if 'E1' in encounters:
        require('first_formation_broken' in encounters['E1']['waves'][-1].get('gate', {}).get('all_events', []), 'E1 reinforcements require first formation break as well as alive-count gate.')
    if 'E3' in encounters:
        require('rescue_approach_open' in encounters['E3']['waves'][-1].get('gate', {}).get('all_events', []), 'E3 reinforcement requires rescue approach open as well as alive-count gate.')
    e2 = encounters.get('E2', {})
    receivers = e2.get('relay_receivers', [])
    require(isinstance(receivers, list) and len(receivers) == 2 and len({x.get('id') for x in receivers if isinstance(x, dict)}) == 2 and all(x.get('close_interaction_disable') is True for x in receivers), 'E2 needs two distinct receivers, each with an ammunition-independent close disable.')
    require({r.get('id') for r in receivers if isinstance(r, dict)} == {'M12_E2_ReceiverWest', 'M12_E2_ReceiverEast'}, 'E2 receiver identities must match the native proof contract.')
    require('both_relay_receivers_disabled' in e2.get('completion_conditions', []), 'E2 completion requires both receiver disables.')
    require(e2.get('sensor_failure') == {'alerts_existing_drones': 2, 'extra_spawns': 0, 'mission_failure': False}, 'Sensor failure alerts the existing drones; it cannot add enemies or fail the mission.')
    e4 = encounters.get('E4', {})
    phases = e4.get('phases', [])
    require(isinstance(phases, list) and [(p.get('id'), p.get('lead'), p.get('partner')) for p in phases] == [('A', 'Selene', 'Tarrik'), ('B', 'Tarrik', 'Selene')], 'E4 requires explicit Selene-to-Tarrik phases and the opposite partner.')
    require(e4.get('same_elite_preserved_across_handoff') is True and len(e4.get('native_bindings', [])) == 2, 'E4 native phase directors must preserve the same elite, not respawn or double-count it.')
    resonance = e4.get('resonance', {})
    require(resonance.get('id') == 'ThermalFracture' and resonance.get('required_demonstrations') == 1 and resonance.get('scarce_resource_required') is False and resonance.get('miss_spends_scarce_resource') is False and resonance.get('conventional_finish_after_demonstration') is True, 'One recoverable Thermal Fracture demonstration is required without a scarce-resource softlock.')

    checkpoints = index(layout.get('checkpoints'), 'id', 'checkpoints')
    require(list(checkpoints) == CHECKPOINT_IDS, 'Checkpoint schedule must include every CP0-CP9 boundary, including CP2b, CP4b and CP5b.')
    last_position = (-1, -1)
    arena_keys = {'CP1': 'M12_E1_PressureHall', 'CP2b': 'M12_E2_RelayOverlook', 'CP4': 'M12_E3_SharedBreach', 'CP5b': 'M12_E4_QuarantineCrucibleB'}
    seen_save_keys = set()
    for checkpoint_id, checkpoint in checkpoints.items():
        mission_id = checkpoint.get('mission_id')
        mission_beats = beats.get(mission_id, {})
        ordered = list(mission_beats)
        after, before = checkpoint.get('after_beat'), checkpoint.get('before_beat')
        require(checkpoint.get('zone') == CHECKPOINT_ZONES.get(checkpoint_id) and checkpoint.get('lead') in ('Tarrik', 'Selene'), checkpoint_id + ': invalid zone or protagonist.')
        owner = checkpoint.get('native_owner', {})
        expected_key = arena_keys.get(checkpoint_id, 'Aurelion.' + checkpoint_id)
        expected_boundary = 'ArenaEntry' if checkpoint_id in arena_keys else 'ExplicitCheckpoint'
        expected_class = ('ASovAurelionLinkPhaseDirector' if checkpoint_id == 'CP5b' else 'ASovCampaignEncounterObjective') if checkpoint_id in arena_keys else 'ASovAurelionPriorityTerminal' if checkpoint_id in ('CP4b', 'CP5') else 'ASovAurelionCheckpoint'
        require(isinstance(owner, dict) and owner.get('save_key') == expected_key and owner.get('boundary') == expected_boundary and owner.get('class') == expected_class, checkpoint_id + ': preserve its sole native checkpoint owner and save key.')
        require(expected_key not in seen_save_keys, checkpoint_id + ': duplicated checkpoint save key.')
        seen_save_keys.add(expected_key)
        require((after is None or after in mission_beats) and (before is None or before in mission_beats) and (after or before), checkpoint_id + ': unknown or empty beat boundary.')
        if after in ordered and before in ordered:
            require(ordered.index(after) < ordered.index(before), checkpoint_id + ': after/before beats are reversed.')
        if mission_id in missions and (after in ordered or before in ordered):
            position = (list(missions).index(mission_id), ordered.index(after) if after in ordered else ordered.index(before) - 0.5)
            require(position >= last_position, checkpoint_id + ': checkpoint progression runs backward.')
            last_position = position
    for phase in phases if isinstance(phases, list) else []:
        checkpoint = checkpoints.get(phase.get('checkpoint'), {})
        require(checkpoint.get('zone') == 'Z08' and checkpoint.get('lead') == phase.get('lead'), 'E4 phase checkpoint must retain the correct room and lead.')

    choice = layout.get('choice', {})
    m12 = beats.get('M12_FireAndFrost', {})
    require(choice.get('key') == 'Aurelion.RescuePriority' and choice.get('zone') == 'Z07' and choice.get('before_encounter') == 'E4', 'Local rescue priority must occur in Z07 before E4.')
    for flag in ('commit_requires_acknowledgment', 'commit_once', 'both_groups_safe', 'rewards_mutually_exclusive', 'groups_mixed_faction'):
        require(choice.get(flag) is True, 'Priority choice requires ' + flag + '.')
    require(choice.get('pending_checkpoint') == 'CP4b' and choice.get('committed_checkpoint') == 'CP5', 'Priority requires pre-prompt CP4b and post-acknowledgment CP5.')
    require(choice.get('acknowledgment_beat') in m12 and choice.get('east_flank_opens_for_both_after_beat') == 'HandoffToTarrikCrucible', 'Choice acknowledgment and normal phase-B flank release must have native beats.')
    outcomes = index(choice.get('outcomes'), 'id', 'priority outcomes')
    require(set(outcomes) == {'WestStretchers', 'EastWalkers'}, 'Priority outcomes must be WestStretchers or EastWalkers.')
    for outcome_id, outcome in outcomes.items():
        require(outcome.get('beat_id') in m12 and outcome.get('immediate_support') != outcome.get('excluded_support') and bool(outcome.get('aftermath')), outcome_id + ': missing native choice, exclusive support or aftermath acknowledgment.')
    west, east = outcomes.get('WestStretchers', {}), outcomes.get('EastWalkers', {})
    require(west.get('immediate_support') == east.get('excluded_support') == 'west_field_cache' and east.get('immediate_support') == west.get('excluded_support') == 'early_east_shutter', 'West cache and early east shutter are alternatives, never cumulative grants.')

    guards = layout.get('canonical_guards', {})
    for flag in ('all_present_day_combat_before_quarantine', 'no_combat_in_core_or_aftermath', 'carrier_rescue_fixed_success', 'boundary_closed', 'release_withheld', 'grammar_propagation_unavoidable', 'crownmark_five_stays_integrated', 'lyric_alive_not_cured'):
        require(guards.get(flag) is True, 'Canonical layout guard missing: ' + flag)
    if native_source is not None:
        # This catches stale names only. C++ automation remains responsible for native
        # dependency ordering, authority, receipt behavior and execution correctness.
        literals = set(re.findall(r'TEXT\("([^"\\]*)"\)', native_source))
        for mission_id, mission_beats in beats.items():
            for value in [mission_id, *mission_beats]:
                require(value in literals, 'Native contract token missing: ' + value)
        for receiver in receivers:
            require(receiver.get('id') in literals, 'Native receiver token missing: ' + str(receiver.get('id')))
        for encounter in encounters.values():
            for binding in encounter.get('native_bindings', []):
                require(binding.get('encounter_id') in literals, 'Native encounter token missing: ' + str(binding.get('encounter_id')))
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument('--check-native-bindings', action='store_true', help='Also check referenced names occur in the native mission source; this does not execute Unreal.')
    args = parser.parse_args()
    try:
        layout = json.loads(args.manifest.read_text(encoding='utf-8-sig'))
        source = (ROOT / 'Source/ProjectVelkorran/Private/Campaign/SovAurelionMissionDefinition.cpp').read_text(encoding='utf-8') if args.check_native_bindings else None
        errors = validate_layout(layout, source)
    except (OSError, ValueError, KeyError, TypeError, AttributeError, IndexError) as exc:
        errors = ['Invalid manifest structure or unavailable input: ' + str(exc)]
    print(json.dumps({'status': 'invalid' if errors else 'host_contract_valid_unreal_unexecuted', 'errors': errors, 'native_name_tokens_checked': args.check_native_bindings, 'unreal_executed': False, 'gameplay_qualified': False}, indent=2))
    return 2 if errors else 0


if __name__ == '__main__':
    raise SystemExit(main())
