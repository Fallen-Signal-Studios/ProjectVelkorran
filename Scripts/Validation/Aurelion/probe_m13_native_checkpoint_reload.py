"""Actual completed M13 -> public CP9 reload -> unchanged native final state.

Import is inert. start(output_directory) requires this process's successful full
M13 input driver and its untouched report. The only gameplay request made here
is SovSaveSubsystem.load_slot(CHECKPOINT, 0). No input injection or correction.
"""
import gc
import hashlib
import inspect
import json
import math
from pathlib import Path
import re
import sys
import time
import traceback
import unreal
import continue_aurelion_m13_input as completed_route

_RUN = None
MISSION = 'M13_ContraryWitness'
MAP = '/Game/Aurelion/Maps/L_Aurelion_M13'
BOUNDARY = 'Aurelion.CP9'
LIFT = 'M13_AurelionExitLift'
TIMEOUT_SECONDS = 180.
STABLE_SECONDS = 4.
LOCATION_TOLERANCE_CM = 6.
ROTATION_TOLERANCE_DEGREES = 3.


def path(obj):
    return obj.get_path_name() if obj is not None else None


def plain(value):
    return json.loads(json.dumps(value, sort_keys=True))


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode('utf8')).hexdigest()


def vec(value):
    result = [float(value.x), float(value.y), float(value.z)]
    assert all(math.isfinite(v) for v in result), 'Non-finite vector'
    return result


def transform(actor):
    rotation = actor.get_actor_rotation()
    return dict(raw=actor.get_actor_transform().export_text(), location=vec(actor.get_actor_location()),
        rotation=[float(rotation.roll), float(rotation.pitch), float(rotation.yaw)],
        scale=vec(actor.get_actor_scale3d()), velocity=vec(actor.get_velocity()))


def native_identity(obj):
    # PyWrapperObject.cpp Hash uses the actual UObject instance; this records
    # primitive identity without retaining a wrapper that would pin old PIE.
    return dict(path=path(obj), native_hash=hash(obj), native_repr=repr(obj))


def stable_id(actor):
    return completed_route.prior.stable_id(actor)


def tag(value):
    return completed_route.gallery.tag_name(value)


def header_data(header):
    return dict(raw=header.export_text(), kind=str(header.kind), slot_index=int(header.slot_index),
        generation=int(header.generation), mission=str(header.mission_id), map=str(header.map_package),
        boundary=str(header.boundary_id), boundary_kind=str(header.boundary_kind),
        protagonist=tag(header.active_protagonist), account=str(header.account_namespace),
        # export_text is stable across loads; a struct wrapper's text embeds its own object address,
        # so two wrappers of the same path would never compare equal.
        definition=header.mission_definition.export_text(), product=str(header.product))


def live_companion(world, pc, pawn):
    actors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovProtagonistCompanionCharacter)
        if a.get_owner() == pc and unreal.SystemLibrary.is_valid(a) and not a.get_editor_property('hidden')]
    assert len(actors) == 1, 'Require exactly one actual controller-owned visible companion'
    actor = actors[0]
    assert tag(actor.get_companion_identity()) == 'Sov.Character.Player.Selene'
    assert actor.is_alive() and actor.get_health() > 0., 'Current companion is not alive'
    component = actor.get_companion_component()
    assert component is not None and not component.is_disabled()
    # Public contextual admission proves current leader/readiness; neither
    # native-only GetCurrentLeader nor IsEncounterSnapshotReady is reflected.
    assert component.can_request_command(pawn, unreal.SovCompanionCommand.HOLD_POSITION, actor) is not None
    assert component.get_command_state() != unreal.SovCompanionCommandState.FAILED
    return actor


def actor_profile(actor, identity):
    asc = actor.get_narrative_ability_system_component()
    assert asc is not None and asc.get_avatar_owner() == actor
    return dict(identity=identity, stable_guid=stable_id(actor), cls=path(actor.get_class()),
        resources=completed_route.prior.resources(actor), items=completed_route.prior.items(actor),
        transform=transform(actor), actor=native_identity(actor), asc=native_identity(asc),
        ready_epoch=int(asc.get_character_ready_epoch()))


def snapshot(world, pc, pawn):
    assert isinstance(pc, unreal.SovPlayerController) and isinstance(pawn, unreal.SovTarrikCharacter)
    assert pawn.is_character_ready() and pawn.is_alive() and pawn.get_health() > 0.
    assert tag(pawn.get_protagonist_identity_tag()) == 'Sov.Character.Player.Tarrik'
    assert pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE
    state = pc.get_campaign_state()
    assert state is not None and state.is_state_valid() and state.get_active_mission() is not None
    assert str(state.get_active_mission().mission_id) == MISSION
    assert state.is_mission_complete(unreal.Name(MISSION))
    assert state.is_mission_complete(unreal.Name(completed_route.prior.MISSION))
    assert not state.get_active_mission().completes_campaign
    events = completed_route.journal(state)
    assert len(events) == 35 and [e['beat'] for e in events] == completed_route.FINAL
    companion = live_companion(world, pc, pawn)
    lifts = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovWorldTransitActor)
        if str(a.transit_id) == LIFT]
    assert len(lifts) == 1, 'Final lift identity is ambiguous or missing'
    lift = lifts[0]
    assert lift.get_transit_state() == unreal.SovWorldTransitState.AT_DESTINATION
    assert lift.kind == unreal.SovWorldTransitKind.LIFT and lift.require_mission_companion_aboard
    checkpoints = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionCheckpoint)
        if str(a.get_boundary_id()) == BOUNDARY]
    assert len(checkpoints) == 1 and checkpoints[0].capture_on_overlap
    evidence = completed_route.evidence(state)
    facts = {}
    for values in completed_route.FACTS.values():
        for key, expected in values.items():
            # Obtain actual authored GameplayTag values rather than constructing
            # native tags or comparing Python GameplayTag wrapper identity.
            writes = [w for b in state.get_active_mission().beats for w in b.state_writes if tag(w.key) == key]
            assert len(writes) == 1 and tag(writes[0].value) == expected and writes[0].canon_protected
            actual = tag(state.get_state_value(writes[0].key))
            assert actual == expected
            facts[key] = actual
    mode = unreal.GameplayStatics.get_game_mode(world)
    return dict(world=native_identity(world), controller=native_identity(pc), game_mode=native_identity(mode),
        options=str(mode.get_editor_property('options_string')),
        game_seconds=float(unreal.GameplayStatics.get_time_seconds(world)),
        paused=bool(unreal.GameplayStatics.is_game_paused(world)),
        player_state=dict(identity=native_identity(pc.player_state), stable_guid=stable_id(pc.player_state)),
        player=actor_profile(pawn, 'Tarrik'), companion=actor_profile(companion, 'Selene'),
        journal=events, journal_raw=[row.export_text() for row in state.get_journal()],
        evidence=evidence, evidence_raw=[row.export_text() for row in state.get_evidence()], facts=facts,
        lift=dict(guid=stable_id(lift), state=str(lift.get_transit_state()),
            body=vec(lift.moving_body.get_world_location()), transform=transform(lift),
            offset=vec(lift.destination_offset), seconds=float(lift.travel_seconds)),
        checkpoint=dict(boundary=str(checkpoints[0].get_boundary_id()), transform=transform(checkpoints[0])),
        m12_complete=True, m13_complete=True, campaign_complete_claim=False)


def retained_world_references(value, current_world, label, seen=None):
    """Inspect completed Python observers without mutating their evidence/state."""
    seen = set() if seen is None else seen
    if value is None or isinstance(value, (str, bytes, int, float, bool, Path, type)) or id(value) in seen:
        return []
    seen.add(id(value))
    if inspect.ismodule(value):
        # A chain retains its child module, whose globals are already audited
        # as their own _RUN; do not walk sys.modules/the entire Unreal module.
        return []
    if isinstance(value, unreal.Object):
        owner = value
        for _ in range(32):
            if owner is None: break
            if owner == current_world or isinstance(owner, unreal.World):
                return [dict(field=label, object=path(value), world=path(owner))]
            owner = owner.get_outer()
        return []
    if isinstance(value, unreal.StructBase):
        raw = value.export_text()
        return [dict(field=label, struct=type(value).__name__, raw=raw)] if 'UEDPIE_' in raw else []
    result = []
    if isinstance(value, (dict, unreal.Map)):
        for index, (key, child) in enumerate(value.items()):
            result += retained_world_references(key, current_world, label+'.keys['+str(index)+']', seen)
            result += retained_world_references(child, current_world, label+'.'+str(key), seen)
    elif isinstance(value, (list, tuple, set, unreal.Array, unreal.Set)):
        for i, child in enumerate(value): result += retained_world_references(child, current_world, label+'['+str(i)+']', seen)
    elif inspect.isfunction(value):
        for i, cell in enumerate(value.__closure__ or ()):
            try: child = cell.cell_contents
            except ValueError: continue
            result += retained_world_references(child, current_world, label+'.closure['+str(i)+']', seen)
        result += retained_world_references(value.__defaults__, current_world, label+'.defaults', seen)
        result += retained_world_references(value.__kwdefaults__, current_world, label+'.kwdefaults', seen)
    elif inspect.ismethod(value) or inspect.isbuiltin(value):
        result += retained_world_references(getattr(value, '__self__', None), current_world, label+'.self', seen)
    elif hasattr(value, '__dict__'):
        result += retained_world_references(vars(value), current_world, label, seen)
    return result


class Run:
    def __init__(self, output_directory):
        self.out = Path(output_directory)
        assert not self.out.exists(), 'Use a new, nonexistent checkpoint observation directory'
        self.out.mkdir(parents=True)
        self.started = time.monotonic()
        self.done = False
        self.handle = self.instance = self.saves = self.delegate = self.callback = None
        self.stable_since = None
        self.last_write = 0.
        self.report = dict(status='initializing', qualified=False, scope='Actual earned final M13 CP9 public reload only',
            mutations=['one public SovSaveSubsystem.load_slot(CHECKPOINT, 0) request'],
            gameplay_inputs=False, manufactured_progress=False, samples=[], callbacks=[],
            required_stable_seconds=STABLE_SECONDS, timeout_seconds=TIMEOUT_SECONDS,
            tolerances=dict(position_cm=LOCATION_TOLERANCE_CM, angle_degrees=ROTATION_TOLERANCE_DEGREES,
                resources=.01, scale=.001),
            resource_policy='Exact observed pre-load public values; private checkpoint payload is not decoded or changed.',
            pending=['Any absent native completion, engine crash or interruption remains unqualified.'])

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 3)
        target = self.out/'checkpoint-reload.json'
        temporary = self.out/'checkpoint-reload.tmp'
        temporary.write_text(json.dumps(self.report, indent=2), encoding='utf8')
        temporary.replace(target)

    def finish(self, passed, reason):
        if self.done: return
        self.done = True
        cleanup = []
        if self.handle is not None:
            try: unreal.unregister_slate_post_tick_callback(self.handle)
            except Exception: cleanup.append(traceback.format_exc())
        self.handle = None
        if self.delegate is not None and self.callback is not None:
            try: self.delegate.remove_callable(self.callback)
            except Exception: cleanup.append(traceback.format_exc())
        self.delegate = self.callback = self.saves = self.instance = None
        self.report.update(status='passed' if passed and not cleanup else 'failed', qualified=bool(passed and not cleanup),
            reason=reason, cleanup_errors=cleanup, retained_gameplay_references_cleared=True,
            observation_finished_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()))
        if self.report['qualified']: self.report['pending'] = ['Physical keyboard and rendered presentation qualification remain separate.']
        self.write()
        unreal.log('Aurelion final checkpoint reload: '+self.report['status']+': '+reason)

    def begin(self):
        previous = completed_route._RUN
        assert previous is not None and previous.done and previous.handle is None
        assert previous.report.get('status') == 'passed', 'Requires an actual successful full M13 input pass in this process'
        source_path = Path(previous.out)/'m13-input-continuation.json'
        source_bytes = source_path.read_bytes()
        source = json.loads(source_bytes)
        assert source['status'] == 'passed' and source.get('retained_gameplay_references_cleared')
        earned = source['native_departure']
        assert earned == plain(previous.report['native_departure']), 'Source report and actual completed observer disagree'
        assert earned['m12_complete'] and earned['m13_complete'] and not earned['campaign_completion_claim']
        assert earned['paired_physical_lift'] and source['native_lift']['actual_riders']
        assert len(earned['journal']) == 35 and set(source['completed_scenes']) == set(completed_route.SCENES)
        assert all(v['completed'] and not v['skipped'] for v in source['completed_scenes'].values())
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        # A PIE world's own name is the map name; only its package path carries the UEDPIE_ prefix.
        assert world is not None and 'UEDPIE_' in world.get_path_name() and 'L_Aurelion_M13' in world.get_path_name()
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        assert [path(world), path(pc), path(pawn)] == [earned['world'], earned['controller'], earned['profile']['player']['actor']]
        checked, leaked = [], []
        for name, module in list(sys.modules.items()):
            if not name.startswith('continue_aurelion_'): continue
            observer = getattr(module, '_RUN', None)
            if observer is None: continue
            assert observer.done and getattr(observer, 'handle', None) is None, 'Input observer remains active: '+name
            refs = retained_world_references(vars(observer), world, name+'._RUN')
            checked.append(dict(module=name, completed_status=observer.report.get('status'), world_reference_count=len(refs)))
            leaked += refs
        self.report['completed_driver_reference_audit'] = dict(checked=checked, leaked=leaked)
        assert not leaked, 'Completed drivers still retain old-world references: '+json.dumps(leaked)
        current = snapshot(world, pc, pawn)
        assert not current['paused'] and current['journal'] == earned['journal'] and current['evidence'] == earned['evidence']
        assert current['player_state']['stable_guid'] == earned['profile']['player_state_guid']
        assert current['companion']['stable_guid'] == earned['profile']['companion']['guid']
        for who in ('player', 'companion'):
            assert current[who]['items'] == earned['profile'][who]['items']
            assert current[who]['resources'] == earned['profile'][who]['resources'], 'Resources changed since final qualification: '+who
        assert math.dist(current['player']['transform']['location'], earned['player_exit']) <= LOCATION_TOLERANCE_CM
        assert math.dist(current['companion']['transform']['location'], earned['companion_exit']) <= LOCATION_TOLERANCE_CM
        self.instance = unreal.GameplayStatics.get_game_instance(world)
        owners = [s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == self.instance]
        assert len(owners) == 1
        self.saves = owners[0]
        assert self.saves.is_platform_storage_owner_available() and not self.saves.is_awaiting_failure_decision()
        assert not self.saves.is_load_pending() and not self.saves.is_mission_travel_pending()
        headers = [h for h in self.saves.list_slots() if h.kind == unreal.SovSaveSlotKind.CHECKPOINT and h.slot_index == 0]
        assert len(headers) == 1
        header = headers[0]
        assert str(header.boundary_id) == BOUNDARY and header.boundary_kind == unreal.SovSaveBoundary.EXPLICIT_CHECKPOINT
        assert str(header.mission_id) == MISSION and str(header.map_package) == MAP and header.generation > 0
        assert tag(header.active_protagonist) == 'Sov.Character.Player.Tarrik'
        writes = source['checkpoints'][BOUNDARY]
        assert writes and all(r['succeeded'] and r['checkpoint'] and r['slot_index'] == 0 and r['journal'] == earned['journal'] for r in writes)
        assert int(header.generation) == max(int(r['generation']) for r in writes), 'Slot0 was replaced after actual CP9 qualification'
        self.report.update(source_report=dict(path=str(source_path.resolve()), sha256=hashlib.sha256(source_bytes).hexdigest()),
            expected_header=header_data(header), initial=current, initial_journal_sha256=digest(current['journal_raw']),
            initial_evidence_sha256=digest(current['evidence_raw']),
            game_instance=native_identity(self.instance), save_owner=native_identity(self.saves),
            source_lift=plain(source['native_lift']), source_cp9=plain(writes))
        def completed(result, saved, message):
            self.report['callbacks'].append(dict(result=str(result), success=result == unreal.SovSaveResult.SUCCESS,
                header=header_data(saved), message=str(message), elapsed=time.monotonic()-self.started))
            self.write()
        self.callback = completed
        self.delegate = self.saves.on_load_completed
        self.delegate.add_callable(self.callback)
        # All old-world objects above are locals. The actual request is delayed
        # to the next observer tick, after begin() has returned and dropped them.
        self.report.update(status='armed', qualified=False, old_world_references_retained_by_observer=False)
        self.write()

    def request(self):
        assert not retained_world_references(vars(self), None, 'checkpoint_observer'), 'Observer retained a World wrapper before travel'
        gc.collect()  # Python wrapper release only; never force Unreal GC in live PIE.
        self.report.update(status='requesting', request_started_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()))
        self.write()
        result = self.saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
        assert isinstance(result, tuple) and len(result) == 2, 'Unexpected native enum + out Error wrapper: '+repr(result)
        returned, message = result
        self.report['request_result'] = dict(result=str(returned), message=str(message))
        assert returned == unreal.SovSaveResult.LOAD_STARTED and self.saves.is_load_pending(), 'Public checkpoint request rejected: '+repr(result)
        self.report['status'] = 'load_requested'
        self.write()

    def verify(self, actual):
        expected = self.report['initial']
        assert actual['world']['native_hash'] != expected['world']['native_hash'], 'No different native UWorld instance observed'
        assert actual['game_mode']['native_hash'] != expected['game_mode']['native_hash'], 'Old game mode survived the supposed map reload'
        assert actual['controller']['native_hash'] != expected['controller']['native_hash'], 'Old controller survived the supposed map reload'
        assert 'SovCampaignSlotLoad=1' in actual['options']
        token = re.search(r'(?:^|\?)SovCampaignLoadRequest=([0-9A-Fa-f]{32})(?:\?|$)', actual['options'])
        assert token is not None and int(token.group(1), 16) != 0, 'Restored world lacks the actual native load request token'
        assert token.group(1).lower() not in expected['options'].lower(), 'Load reused the old request token'
        assert actual['journal_raw'] == expected['journal_raw'], 'Reload changed an earned native journal row'
        assert actual['evidence_raw'] == expected['evidence_raw'], 'Reload changed canonical evidence provenance'
        assert actual['facts'] == expected['facts'] and actual['player_state']['stable_guid'] == expected['player_state']['stable_guid']
        assert not actual['paused'], 'Restored game remains paused'
        for who in ('player', 'companion'):
            restored, original = actual[who], expected[who]
            assert restored['identity'] == original['identity'] and restored['stable_guid'] == original['stable_guid']
            assert restored['cls'] == original['cls'] and restored['items'] == original['items'], 'Inventory/ammo changed for '+who
            for key, value in original['resources'].items():
                assert abs(restored['resources'][key]-value) <= .01, 'Restored public resource differs: '+who+'.'+key
            assert math.dist(restored['transform']['location'], original['transform']['location']) <= LOCATION_TOLERANCE_CM, 'Separate exit moved: '+who
            for a, b in zip(restored['transform']['rotation'], original['transform']['rotation']):
                assert abs((a-b+180.)%360.-180.) <= ROTATION_TOLERANCE_DEGREES, 'Separate exit rotation changed: '+who
            assert max(abs(a-b) for a, b in zip(restored['transform']['scale'], original['transform']['scale'])) <= .001
            assert math.dist(restored['transform']['velocity'], [0.,0.,0.]) < 5., 'Departure is not settled: '+who
        assert math.dist(actual['player']['transform']['location'], actual['companion']['transform']['location']) > 2400.
        assert actual['lift']['guid'] == expected['lift']['guid'] and actual['lift']['state'] == expected['lift']['state']
        assert math.dist(actual['lift']['body'], expected['lift']['body']) <= LOCATION_TOLERANCE_CM
        assert actual['lift']['offset'] == expected['lift']['offset'] and actual['lift']['seconds'] == expected['lift']['seconds']
        return token.group(1)

    def tick(self, _delta):
        if self.done: return
        try:
            elapsed = time.monotonic()-self.started
            assert elapsed < TIMEOUT_SECONDS, 'Native checkpoint reload observation exceeded180seconds'
            if self.report['status'] == 'armed': self.request(); return
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if world is None: return
            assert unreal.GameplayStatics.get_game_instance(world) == self.instance, 'Load replaced or lost the retained GameInstance'
            owners = [s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == self.instance]
            assert len(owners) == 1 and owners[0] == self.saves, 'Load replaced the actual platform save owner'
            assert len(self.report['callbacks']) <= 1, 'More than one native load-completion callback observed'
            if self.report['callbacks']:
                result = self.report['callbacks'][0]
                assert result['success'], 'Native load failed: '+result['message']
                assert result['header'] == self.report['expected_header'], 'Native callback loaded a different checkpoint'
            if self.saves.is_load_pending() or not self.report['callbacks']: return
            assert self.saves.is_platform_storage_owner_available() and not self.saves.is_awaiting_failure_decision()
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            actual = snapshot(world, pc, pawn)
            token = self.verify(actual)
            now = time.monotonic()
            if self.stable_since is None: self.stable_since = now
            if now-self.last_write > .5:
                self.last_write = now
                self.report['samples'].append(dict(elapsed=elapsed, stable_seconds=now-self.stable_since,
                    world=actual['world'], player=actual['player']['transform'], companion=actual['companion']['transform'],
                    player_resources=actual['player']['resources'], companion_resources=actual['companion']['resources'],
                    journal_sha256=digest(actual['journal_raw']), evidence_sha256=digest(actual['evidence_raw']), lift=actual['lift']))
                self.write()
            if now-self.stable_since < STABLE_SECONDS: return
            self.report.update(restored=actual, load_request_token=token,
                stable_departure_seconds=now-self.stable_since, callback_count=len(self.report['callbacks']))
            self.finish(True, 'The actual final CP9 checkpoint reloaded into a new native M13 world through the public save owner. One native success callback, all35 raw journal rows/evidence, stable identities, inventory/resources, completed lift and separate exits remained intact for4seconds.')
        except Exception:
            self.finish(False, traceback.format_exc())


def start(output_directory):
    global _RUN
    assert _RUN is None or _RUN.done, 'This final checkpoint observer is already active'
    _RUN = Run(output_directory)
    try:
        _RUN.begin()
        _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    except Exception:
        _RUN.finish(False, traceback.format_exc())
    return _RUN


def stop():
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False, 'Stopped by operator; no successful reload qualification claimed')
