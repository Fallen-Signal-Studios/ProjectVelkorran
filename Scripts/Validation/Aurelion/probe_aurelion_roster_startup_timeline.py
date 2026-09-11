"""Fresh real PIE, no inputs: enemies plus all actual M12 story NPCs and roster references.

Root launches this through ExecutePythonScript with a fresh isolated UserDir and
SOV_AURELION_RUN_DIRECTORY. Only editor lifecycle and unchanged isolated-profile
accessibility acknowledgement are written. No runtime initialization is forced.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sys
import time
import traceback
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inspect_aurelion_enemies_pie import inspect_enemies

OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']).resolve()
SAVED = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())).resolve()
ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
assert SAVED == (OUT / 'UserData/Saved').resolve(), 'Requires the dedicated isolated profile'
assert not list((SAVED / 'SaveGames').glob('*.sav')), 'Requires a fresh save profile'
MAP = '/Game/Aurelion/Maps/L_Aurelion_M12'
THRESHOLDS = (.1, .25, .5, 1., 2., 4., 8., 16., 30.)
LIFECYCLE_TAG = 'Narrative.State.DontReturnToSpawn'
ENEMY_ROLES = ('SecurityDrone', 'ContaminatedDrone', 'Enforcer', 'Linkbound', 'WallRunner', 'Weaver', 'Elite')
STORY_ROLES = ('MeetingTarrik', 'TrappedMarine', 'Lyric', 'Malik', 'Tharne', 'Lyessa',
               'WestDominionStretcher', 'WestReformationStretcher', 'EastDominionWalker', 'EastReformationWalker')
DEFINITIONS = tuple('/Game/Aurelion/Enemies/NPC_Aurelion' + name for name in ENEMY_ROLES) + tuple(
    '/Game/Aurelion/Characters/NPC_Aurelion' + name for name in STORY_ROLES + ('TarrikCompanion', 'SeleneCompanion'))
PLACEMENT_TOLERANCE_CM = .5
START = time.monotonic()
report = dict(status='running', method='Real editor PIE with no input; timestamped read-only observations',
              rendered_validation=False, forced_initialization=False, runtime_state_writes=False,
              timing='Requested thresholds are game seconds; actual sample time is always retained, including late frames.',
              samples=[], inspection_errors=[], qualification_failures=[], observation_complete=False,
              definition_scope='19 owned definitions: 7 enemy, 10 story, 2 companion. The 34 placed actors use the first 17.',
              placement_tolerance_cm=PLACEMENT_TOLERANCE_CM)
ctl = dict(index=0, handle=None, done=False, stopping=False, stop_at=0.)
files = sorted(list((ROOT / 'Content/Aurelion').rglob('*.uasset')) + list((ROOT / 'Content/Aurelion').rglob('*.umap')))
before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}


def path(value):
    return value.get_path_name() if value else None


def stable_path(value):
    return re.sub(r'UEDPIE_\d+_', '', value)


def position(value):
    return [float(value.x), float(value.y), float(value.z)]


def required_tag():
    tag = unreal.GameplayTag()
    assert tag.import_text('(TagName="' + LIFECYCLE_TAG + '")')
    assert unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag), 'Required native lifecycle tag is unavailable'
    assert str(unreal.GameplayTagLibrary.get_tag_name(tag)) == LIFECYCLE_TAG
    return tag


def capture_authored_placements():
    authored = {}
    editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for actor in unreal.GameplayStatics.get_all_actors_of_class(editor_world, unreal.SovNPCCharacterBase):
        cls = path(actor.get_class())
        if cls.startswith('/Game/Aurelion/Enemies/BP_Aurelion') or cls.startswith('/Game/Aurelion/Characters/BP_AurelionStoryNPC.'):
            assert unreal.SystemLibrary.is_valid(actor) and actor.get_world() == editor_world
            authored[stable_path(path(actor))] = dict(actor_class=cls, position=position(actor.get_actor_location()))
    assert len(authored) == 34, 'Authored M12 must contain exactly 24 enemies and 10 story actors'
    report['authored_placements'] = authored


def qualify(full):
    failures = report['qualification_failures']
    def check(condition, reason):
        if not condition and reason not in failures:
            failures.append(reason)
        return bool(condition)
    def inspection_errors(value):
        if isinstance(value, dict):
            return [str(v) for k, v in value.items() if k.endswith('_inspection_error')] + sum(
                (inspection_errors(v) for v in value.values()), [])
        if isinstance(value, list):
            return sum((inspection_errors(v) for v in value), [])
        return []
    authored = report.get('authored_placements', {})
    metrics = dict(all_34_valid_current_world=True, all_34_same_authored_identity_and_position=True,
                   all_34_alive_positive_resources=True, all_34_runtime_lifecycle_tags=True,
                   maximum_placement_drift_cm=0.)
    for sample_row in report['samples']:
        when = str(sample_row['requested_game_seconds'])
        actors = sample_row['actors']
        same_roster = (len(actors) == 34 and sample_row['actual_owned_enemy_count'] == 24
                       and sample_row['actual_story_npc_count'] == 10
                       and {stable_path(a['actor']) for a in actors} == set(authored))
        if not check(same_roster, when + 's: actual actor identities/counts differ from the authored 34'):
            metrics['all_34_same_authored_identity_and_position'] = False
        for actor in actors:
            identity = stable_path(actor['actor'])
            prefix = when + 's: ' + identity
            valid = actor.get('object_is_valid') is True and actor.get('same_world') is True and actor.get('being_destroyed') is False
            metrics['all_34_valid_current_world'] &= check(valid, prefix + ': native actor invalid, foreign or being destroyed')
            healthy = actor.get('alive') is True and actor.get('health', 0) > 0 and actor.get('max_health', 0) > 0
            metrics['all_34_alive_positive_resources'] &= check(healthy, prefix + ': invalid living resource state')
            metrics['all_34_runtime_lifecycle_tags'] &= check(actor.get('asc_has_lifecycle_tag') is True,
                prefix + ': actual ASC is missing its required lifecycle tag or could not be inspected')
            if identity in authored:
                drift = math.dist(actor['position_xyz'], authored[identity]['position'])
                metrics['maximum_placement_drift_cm'] = max(metrics['maximum_placement_drift_cm'], drift)
                metrics['all_34_same_authored_identity_and_position'] &= check(
                    math.isfinite(drift) and drift <= PLACEMENT_TOLERANCE_CM
                    and actor['actor_class'] == authored[identity]['actor_class'], prefix + ': authored placement/class changed')
        for director in sample_row['directors']:
            for entry in director['participants']:
                valid = (entry.get('object_is_valid') is True and entry.get('same_world') is True
                         and entry.get('being_destroyed') is False and not entry['duplicate_identity']
                         and not entry['duplicate_actor'] and not entry['mass_represented'])
                metrics['all_34_valid_current_world'] &= check(valid, when + 's: invalid native director membership ' + entry['identity'])
    check(not inspection_errors(report['samples']), 'One or more sample fields could not be inspected')
    check([s['requested_game_seconds'] for s in report['samples']] == list(THRESHOLDS), 'Incomplete requested timeline')
    report['definition_tag_inspection'] = {}
    tag = required_tag()
    for package in DEFINITIONS:
        definition = unreal.load_asset(package)
        valid = unreal.SystemLibrary.is_valid(definition) and isinstance(definition, unreal.NPCDefinition)
        has_tag = valid and unreal.GameplayTagLibrary.has_tag(definition.get_editor_property('default_owned_tags'), tag, True)
        report['definition_tag_inspection'][package] = dict(native_valid=valid, exact_lifecycle_tag=bool(has_tag))
        check(has_tag, 'Owned definition is absent or missing exact lifecycle tag: ' + package)
    expected_placed = set(DEFINITIONS[:17])
    for sample_row in report['samples']:
        check({a['definition'].split('.')[0] for a in sample_row['actors'] if a.get('definition')} == expected_placed,
              str(sample_row['requested_game_seconds']) + 's: placed actor definitions differ from the exact 17 expected roles')
    metrics['all_19_definition_tags'] = all(r['exact_lifecycle_tag'] for r in report['definition_tag_inspection'].values())
    metrics['final_24_inspector_passed'] = check(full.get('status') == 'passed_observable_startup_contract'
        and not full.get('contract_failures') and not full.get('startup_pending') and not full.get('inspection_errors'),
        'Final hostile inspector did not pass every startup and hold check')
    metrics['final_all_director_holds_clear'] = check(all(not d['hold_error'] for d in report['samples'][-1]['directors']),
        'One or more final native pre-entry holds remain unresolved')
    report['qualifications'] = metrics
    report['observation_complete'] = True


def encoded(value):
    if value is None or isinstance(value, (str, bool, int, float)):
        return value
    if isinstance(value, unreal.Object):
        return path(value)
    if hasattr(value, 'export_text'):
        return value.export_text()
    return str(value)


def read(row, key, callback):
    try:
        value = callback()
        row[key] = encoded(value)
        return value
    except Exception as exc:
        row[key + '_inspection_error'] = str(exc)
        return None


def write():
    report['elapsed_wall_seconds'] = round(time.monotonic() - START, 3)
    temp = OUT / 'enemy-startup-timeline.tmp'
    temp.write_text(json.dumps(report, indent=2), encoding='utf8')
    temp.replace(OUT / 'enemy-startup-timeline.json')


def sample(world, threshold):
    # Every UObject is local to this call; the persistent report contains scalars only.
    row = dict(requested_game_seconds=threshold,
               actual_game_seconds=unreal.GameplayStatics.get_time_seconds(world),
               elapsed_wall_seconds=time.monotonic() - START,
               world=path(world), actors=[], directors=[], player={})
    player = unreal.GameplayStatics.get_player_pawn(world, 0)
    row['player']['actor'] = path(player)
    if isinstance(player, unreal.SovPlayerCharacterBase):
        read(row['player'], 'health', player.get_health)
        read(row['player'], 'ready', player.is_character_ready)
        read(row['player'], 'position', player.get_actor_location)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if isinstance(pc, unreal.SovPlayerController):
        state = pc.get_campaign_state()
        row['journal'] = [dict(beat=str(e.beat_id), id=e.event_id.export_text()) for e in state.get_journal()] if state else None
    memberships = {}
    for director in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector):
        dr = dict(actor=path(director), encounter_id=str(director.encounter_id),
                  state=str(director.get_encounter_state()),
                  hold_error=director.get_editor_property('pre_entry_hold_error'), participants=[])
        seen_ids, seen_paths = set(), set()
        for entry in director.participants:
            actor = entry.character
            identity = str(entry.participant_id)
            actor_path = path(actor)
            pr = dict(identity=identity, actor=actor_path,
                      required_for_victory=entry.required_for_victory,
                      duplicate_identity=identity in seen_ids,
                      duplicate_actor=actor_path is not None and actor_path in seen_paths,
                      object_is_valid=unreal.SystemLibrary.is_valid(actor),
                      mass_represented=director.is_participant_mass_represented(entry.participant_id))
            seen_ids.add(identity)
            if actor_path:
                seen_paths.add(actor_path)
            if pr['object_is_valid']:
                read(pr, 'actor_class', actor.get_class)
                read(pr, 'actor_world', actor.get_world)
                read(pr, 'same_world', lambda: actor.get_world() == world)
                read(pr, 'being_destroyed', actor.is_actor_being_destroyed)
                read(pr, 'remaining_life_span', actor.get_life_span)
                read(pr, 'position', actor.get_actor_location)
                read(pr, 'health', actor.get_health)
                read(pr, 'max_health', actor.get_max_health)
                read(pr, 'alive', actor.is_alive)
            else:
                pr['native_state_not_dereferenced'] = 'Null or pending-kill object; path retained only for identity evidence.'
            dr['participants'].append(pr)
            if actor:
                memberships.setdefault(path(actor), []).append(identity)
        row['directors'].append(dr)
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase)
    for actor in actors:
        actor_class = path(actor.get_class())
        is_enemy = actor_class.startswith('/Game/Aurelion/Enemies/BP_Aurelion')
        is_story = actor_class.startswith('/Game/Aurelion/Characters/BP_AurelionStoryNPC.')
        if not (is_enemy or is_story):
            continue
        ar = dict(actor=path(actor), actor_class=path(actor.get_class()),
                  category='enemy' if is_enemy else 'story',
                  object_is_valid=unreal.SystemLibrary.is_valid(actor),
                  actor_world=path(actor.get_world()), same_world=actor.get_world() == world,
                  participant_ids=memberships.get(path(actor), []))
        ar['position_xyz'] = position(actor.get_actor_location())
        read(ar, 'being_destroyed', actor.is_actor_being_destroyed)
        read(ar, 'remaining_life_span', actor.get_life_span)
        read(ar, 'authored_tags', lambda: ','.join(str(tag) for tag in actor.tags))
        for key, callback in (
                ('health', actor.get_health), ('max_health', actor.get_max_health),
                ('alive', actor.is_alive), ('pending_load', actor.is_character_pending_load),
                ('position', actor.get_actor_location), ('velocity', actor.get_velocity),
                ('definition', actor.get_npc_definition), ('visual', actor.get_character_visual)):
            read(ar, key, callback)
        read(ar, 'hidden', lambda: actor.get_editor_property('hidden'))
        capsule = actor.get_editor_property('capsule_component')
        read(ar, 'capsule_radius', capsule.get_scaled_capsule_radius)
        read(ar, 'capsule_half_height', capsule.get_scaled_capsule_half_height)
        read(ar, 'capsule_collision', capsule.get_collision_enabled)
        movement = actor.get_editor_property('character_movement')
        if movement:
            read(ar, 'movement_mode', lambda: movement.get_editor_property('movement_mode'))
        controller = actor.get_controller()
        ar['controller'] = path(controller)
        ar['controller_class'] = path(controller.get_class()) if controller else None
        if controller:
            read(ar, 'controlled_pawn', controller.get_controlled_pawn)
        asc = actor.get_narrative_ability_system_component()
        ar['asc'] = path(asc)
        if asc:
            read(ar, 'asc_avatar', asc.get_avatar_owner)
            read(ar, 'ability_count', lambda: len(asc.get_all_abilities()))
            # Reflected GameplayTagAssetInterface::HasMatchingGameplayTag delegates to ASC's tag counts.
            read(ar, 'asc_has_lifecycle_tag', lambda: asc.has_matching_gameplay_tag(required_tag()))
        activities = actor.get_activity_component()
        ar['activity_component'] = path(activities)
        if activities:
            read(ar, 'activity_active', activities.is_active)
            read(ar, 'current_activity', activities.get_current_activity)
            read(ar, 'current_goal', activities.get_current_activity_goal)
        row['actors'].append(ar)
    row['actual_owned_enemy_count'] = sum(a['category'] == 'enemy' for a in row['actors'])
    row['actual_story_npc_count'] = sum(a['category'] == 'story' for a in row['actors'])
    row['actual_story_definition_count'] = len({a.get('definition') for a in row['actors'] if a['category'] == 'story'})
    report['samples'].append(row)
    write()


def finish(reason, failed=False):
    if ctl['done']:
        return
    ctl['done'] = True
    ctl['stop_at'] = time.monotonic()
    after = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    passed = not failed and after == before and report['observation_complete'] and not report['qualification_failures']
    report.update(status='passed_readonly_startup_qualification' if passed else 'failed', reason=reason,
                  assets_unchanged=before == after, asset_hashes_before=before, asset_hashes_after=after)
    write()


def tick(dt):
    try:
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if ctl['done']:
            if not ctl['stopping']:
                ctl['stopping'] = True
                if editor.is_in_play_in_editor():
                    editor.editor_request_end_play()
                    return
            if not editor.is_in_play_in_editor() or time.monotonic() - ctl['stop_at'] > 15.:
                unreal.unregister_slate_post_tick_callback(ctl['handle'])
                ctl['handle'] = None
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert time.monotonic() - START < 210., 'Startup observation wall-clock limit exceeded'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        package = re.sub(r'UEDPIE_\d+_', '', path(world).split('.')[0])
        assert editor.is_in_play_in_editor() and package == MAP, 'Unexpected current PIE world'
        current_time = unreal.GameplayStatics.get_time_seconds(world)
        while ctl['index'] < len(THRESHOLDS) and current_time >= THRESHOLDS[ctl['index']]:
            sample(world, THRESHOLDS[ctl['index']])
            ctl['index'] += 1
        if ctl['index'] == len(THRESHOLDS):
            full = inspect_enemies(world, OUT / 'enemy-startup-readonly.json', require_initialized=True)
            report['final_inspection'] = {key: full.get(key) for key in
                ('status', 'contract_failures', 'startup_pending', 'inspection_errors', 'actual_owned_enemy_actor_count')}
            qualify(full)
            finish('Requested timeline complete; explicit qualifications determine success. Combat and rendered appearance remain outside this probe.')
    except Exception:
        report['inspection_errors'].append(traceback.format_exc())
        finish('Observation encountered an API or lifecycle failure', True)


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
try:
    settings = unreal.GameUserSettings.get_game_user_settings()
    assert isinstance(settings, unreal.SovGameUserSettings)
    snapshot = settings.get_settings_snapshot()
    assert not snapshot.tap_interactions and abs(snapshot.interaction_hold_scale - 1.) < .001
    assert settings.complete_accessibility_setup(), 'Could not accept unchanged standard settings in isolated profile'
    report['settings'] = snapshot.export_text()
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert not editor.is_in_play_in_editor(), 'Requires a fresh stopped editor'
    assert editor.load_level(MAP)
    assert editor.get_viewport_config_keys(), 'Requires real editor PIE viewport'
    capture_authored_placements()
    ctl['handle'] = unreal.register_slate_post_tick_callback(tick)
    write()
    editor.editor_request_begin_play()
except Exception:
    report['inspection_errors'].append(traceback.format_exc())
    finish('Could not begin fresh PIE observation', True)
    if ctl['handle'] is None:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
