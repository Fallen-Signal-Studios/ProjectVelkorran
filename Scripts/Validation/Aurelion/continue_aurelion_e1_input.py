"""Current-PIE E1 continuation using only ordinary Enhanced Input.

Import this module, select Cinderline with the actual weapon-wheel UI, stop every
other input driver, then call start(). This script never begins/ends PIE, equips a
weapon, changes transforms, grants resources, or writes campaign/encounter state.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import time
import traceback
import unreal

_RUN = None
MISSION = 'M12_FireAndFrost'
WEAPON = '/Game/Items/Weapons/WI_Cinderline.WI_Cinderline_C'
ACTION_ROOT = '/NarrativePro/Pro/Core/Data/Input/'
BEATS = ['TarrikArrival', 'PressureHall', 'SecureTarrikRoute', 'HandoffToSelene']


def replace_report_with_retry(source, destination, timeout_seconds=0.25):
    """Retry only transient Windows access/sharing errors; never replay a caller."""
    if not 0.0 <= timeout_seconds <= 0.25:
        raise ValueError('Report retry must remain within 250 milliseconds')
    deadline = time.monotonic() + timeout_seconds
    retries = 0
    while True:
        try:
            os.replace(source, destination)
            return retries
        except OSError as error:
            remaining = deadline - time.monotonic()
            if getattr(error, 'winerror', None) not in (5, 32) or remaining <= 0:
                raise
            retries += 1
            time.sleep(min(0.01, remaining))


def _path(obj):
    return obj.get_path_name() if obj else None


def _optional(read):
    try:
        return read()
    except Exception as error:
        return {'unavailable': str(error)}


def _xyz(vector):
    return [vector.x, vector.y, vector.z]


def _journal(state):
    return [dict(mission=str(e.mission_id), beat=str(e.beat_id),
                 id=e.event_id.export_text(), sequence=e.sequence)
            for e in state.get_journal()]


class Run:
    def __init__(self, output_directory):
        self.out = Path(output_directory)
        self.out.mkdir(parents=True, exist_ok=True)
        self.root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
        self.assets = sorted((self.root / 'Content/Aurelion').rglob('*.uasset'))
        self.assets += sorted((self.root / 'Content/Aurelion').rglob('*.umap'))
        self.before = self.hashes()
        self.started = time.monotonic()
        self.phase_at = self.started
        self.phase = 'initialize'
        self.done = False
        self.handle = None
        self.owner = None
        self.world = None
        self.initial_pawn = None
        self.initial_events = None
        self.attempt = None
        self.e1 = None
        self.target = None
        self.last_sample = 0.
        self.last_write = 0.
        self.last_path = 0.
        self.path_points = []
        self.path_target = None
        self.hold_started = None
        self.saw_countdown = False
        self.hold_actor = None
        self.hold_beat = None
        self.waypoints = []
        self.route_then = None
        self.last_position = None
        self.last_motion_at = self.started
        self.report = dict(status='running', scope='E1 combat, secure approach, first native handoff',
                           method='Ordinary Enhanced Input actions in an existing PIE world',
                           physical_keyboard_validation=False, rendered_image_review=False,
                           direct_state_or_resource_or_transform_writes=False,
                           samples=[], stages=[], holds=[], targets=[], input_frames={},
                           assets_before=self.before)
        self.actions = {name: unreal.load_asset(ACTION_ROOT + name) for name in
                        ('IA_Move', 'IA_Look', 'IA_Attack', 'IA_AltAttack', 'IA_Reload', 'IA_Interact')}
        assert all(self.actions.values()), 'Required existing Narrative input actions are missing'

    def hashes(self):
        return {str(p.relative_to(self.root)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in self.assets}

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic() - self.started, 3)
        temp = self.out / 'e1-input-continuation.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        replace_report_with_retry(temp, self.out / 'e1-input-continuation.json')

    def stage(self, name, detail=None):
        self.phase = name
        self.phase_at = time.monotonic()
        self.last_motion_at = self.phase_at
        self.last_position = None
        self.report['stages'].append(dict(phase=name, elapsed=self.phase_at-self.started, detail=detail))
        self.write()

    def inject(self, move=(0., 0.), look=(0., 0.), attack=0., aim=0., reload=0., interact=0.):
        if not self.owner:
            return
        values = {'IA_Move': (*move, 0.), 'IA_Look': (*look, 0.),
                  'IA_Attack': (attack, 0., 0.), 'IA_AltAttack': (aim, 0., 0.),
                  'IA_Reload': (reload, 0., 0.), 'IA_Interact': (interact, 0., 0.)}
        for name, vector in values.items():
            self.owner.inject_input_vector_for_action(self.actions[name], unreal.Vector(*vector), [], [])
            if any(vector):
                counts = self.report['input_frames']
                counts[name] = counts.get(name, 0) + 1

    def finish(self, passed, reason):
        if self.done:
            return
        self.done = True
        # Explicit zero input releases held attack/aim/interact through their normal events.
        self.report['release_input'] = _optional(lambda: self.inject())
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        after = self.hashes()
        self.report.update(status='passed' if passed else 'failed', reason=reason,
                           assets_after=after, assets_unchanged=after == self.before,
                           pie_left_running=True)
        if after != self.before:
            self.report['status'] = 'failed'
        self.release_world_references()
        self.write()
        unreal.log('Aurelion E1 continuation: ' + self.report['status'] + ': ' + reason)
        # E3 inherits this method; only this module's actual E1 run starts the chain.
        if self is _RUN and self.report['status'] == 'passed' and os.environ.get('SOV_AURELION_E1_CONTINUE_ROUTE') == '1':
            try:
                import continue_aurelion_route_input
                chain = continue_aurelion_route_input.start(self.out/'route-follow-on')
                self.report['route_follow_on'] = dict(status=chain.report['status'], output=str(chain.out))
            except Exception:
                self.report['route_follow_on'] = dict(status='failed', error=traceback.format_exc())
            self.write()

    def release_world_references(self):
        # Completed input observers must not keep an old PIE world alive across
        # the mission's genuine map travel. Serialized evidence remains intact.
        self.world = self.owner = self.e1 = self.target = self.hold_actor = None
        self.path_target = None
        self.path_points = []

    def get_input_owner(self, world):
        engine = unreal.GameplayStatics.get_game_instance(world).get_outer()
        matches = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)
                   if isinstance(s.get_outer(), unreal.LocalPlayer) and s.get_outer().get_outer() == engine]
        assert len(matches) == 1, 'Requires one local player in this dedicated editor'
        return matches[0]

    def local_move(self, pc, pawn, target, stop=30.):
        p = pawn.get_actor_location()
        dx, dy = target[0]-p.x, target[1]-p.y
        distance = math.hypot(dx, dy)
        if distance < stop:
            return (0., 0.), True
        yaw = math.radians(pc.get_control_rotation().yaw)
        strength = min(.8, max(.12, distance/160.))
        return ((-dx*math.sin(yaw)+dy*math.cos(yaw))/distance*strength,
                (dx*math.cos(yaw)+dy*math.sin(yaw))/distance*strength), False

    def look(self, world, pc, target):
        camera = unreal.GameplayStatics.get_player_camera_manager(world, 0)
        eye, rotation = camera.get_camera_location(), camera.get_camera_rotation()
        dx, dy, dz = target.x-eye.x, target.y-eye.y, target.z-eye.z
        yaw = (math.degrees(math.atan2(dy, dx))-rotation.yaw+180.) % 360.-180.
        pitch = (math.degrees(math.atan2(dz, math.hypot(dx, dy)))-rotation.pitch+180.) % 360.-180.
        legacy = unreal.InputSettings.get_input_settings().get_editor_property('enable_legacy_input_scales')
        ys = pc.get_deprecated_input_yaw_scale() if legacy else 1.
        ps = pc.get_deprecated_input_pitch_scale() if legacy else 1.
        settings = self.owner.get_user_settings()
        # Respect the unchanged user's inversion settings instead of changing them.
        if isinstance(settings, unreal.NarrativeInputSettings):
            if settings.get_invert_horizontal(): ys *= -1.
            if settings.get_invert_vertical(): ps *= -1.
        assert abs(ys) > .001 and abs(ps) > .001, 'Look input scales are zero'
        clamp = lambda v: max(-.7, min(.7, v))
        return (clamp(yaw*.12/ys), clamp(pitch*.12/ps)), max(abs(yaw), abs(pitch))

    def clear_sight(self, world, pawn, target):
        camera = unreal.GameplayStatics.get_player_camera_manager(world, 0)
        ignored = [pawn] + list(pawn.get_attached_actors())
        visual = pawn.get_character_visual()
        if visual and visual not in ignored:
            ignored.append(visual)
        result = unreal.SystemLibrary.line_trace_single(world, camera.get_camera_location(),
            target.get_actor_location(), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False,
            ignored, unreal.DrawDebugTrace.NONE, True)
        # UE's Python return packing represents the native bool=false result
        # as None when a function also has output parameters (PyGenUtil.cpp).
        # For LineTraceSingle that means no blocking hit along this exact ray.
        if result is None:
            self.report['last_sight'] = dict(clear=True, blocking=False, hit=None,
                                             target=_path(target), impact=None, native_trace_hit=False)
            return True
        hits = [v for v in result if isinstance(v, unreal.HitResult)] if isinstance(result, tuple) else [result]
        assert len(hits) == 1 and isinstance(hits[0], unreal.HitResult), 'Unexpected native trace result wrapper'
        hit = hits[0]
        parts = hit.to_tuple()
        actor = parts[9]
        clear = not parts[0] or actor == target or (actor is not None and actor.get_owner() == target)
        self.report['last_sight'] = dict(clear=clear, blocking=parts[0], hit=_path(actor),
                                         target=_path(target), impact=_xyz(parts[5]))
        return clear

    def approach(self, world, pc, pawn, target):
        now = time.monotonic()
        if self.path_target != target or now-self.last_path > .8:
            self.last_path, self.path_target = now, target
            path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(
                world, pawn.get_actor_location(), target.get_actor_location(), pawn, None)
            valid = path is not None and path.is_valid() and not path.is_partial()
            self.report['last_combat_path'] = dict(target=_path(target), complete=valid,
                                                  points=[_xyz(p) for p in path.path_points] if path else [])
            self.path_points = list(path.path_points)[1:] if valid else []
        while self.path_points:
            result, reached = self.local_move(pc, pawn, _xyz(self.path_points[0]), stop=80.)
            if not reached:
                return result
            self.path_points.pop(0)
        return (0., 0.)

    def weapon(self, pawn):
        weapons = [w for w in pawn.get_wielded_weapons() if w and w.get_class().get_path_name() == WEAPON]
        assert len(weapons) == 1, 'Select the actual Cinderline through its weapon-wheel UI before continuing'
        return weapons[0]

    def roster(self):
        return [dict(id=str(p.participant_id), actor=_path(p.character),
                     health=p.character.get_health() if p.character else None,
                     alive=p.character.is_alive() if p.character else False,
                     hidden=p.character.get_editor_property('hidden') if p.character else None)
                for p in self.e1.participants if p.required_for_victory]

    def start_route(self, waypoints, then):
        self.waypoints, self.route_then = list(waypoints), then
        self.stage('walk_route', dict(points=waypoints, then=then))

    def walk_route(self, pc, pawn):
        if not self.waypoints:
            self.inject()
            self.stage(self.route_then)
            return
        movement, reached = self.local_move(pc, pawn, self.waypoints[0])
        if reached:
            self.waypoints.pop(0)
            self.last_motion_at = time.monotonic()
            self.path_target = None
            self.inject()
        else:
            now = time.monotonic()
            destination = self.waypoints[0]
            if self.path_target != destination or now-self.last_path > .8:
                self.path_target, self.last_path = destination, now
                position = pawn.get_actor_location()
                path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(self.world, position,
                    unreal.Vector(destination[0], destination[1], position.z), pawn, None)
                valid = path is not None and path.is_valid() and not path.is_partial()
                self.path_points = list(path.path_points)[1:] if valid else []
                self.report['last_route_path'] = dict(destination=destination, complete=valid,
                    points=[_xyz(p) for p in path.path_points] if path else [])
            movement = (0., 0.)
            while self.path_points:
                movement, point_reached = self.local_move(pc, pawn, _xyz(self.path_points[0]), stop=25.)
                if not point_reached:
                    break
                self.path_points.pop(0)
            self.inject(move=movement)
        position = pawn.get_actor_location()
        if self.last_position is None or math.hypot(position.x-self.last_position[0], position.y-self.last_position[1]) > 35.:
            self.last_position = _xyz(position)
            self.last_motion_at = time.monotonic()
        assert time.monotonic()-self.last_motion_at < 15., 'Normal route movement stalled; inspect collision/navigation'

    def acquire_hold(self, world, pc, pawn, cls, field, value, beat):
        matches = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls)
                   if str(a.get_editor_property(field)) == value]
        assert len(matches) == 1, 'Missing or duplicate authored interaction: '+value
        actor = matches[0]
        interaction = pc.get_interaction_component()
        component = actor.interactable
        look, error = self.look(world, pc, actor.get_actor_location())
        self.inject(look=look)
        admission = component.can_interact(pawn, interaction)
        focus = interaction.get_editor_property('viewed_interactable')
        self.report['last_interaction'] = dict(actor=_path(actor), focus=_path(focus),
            admission=str(admission), native_action_text=str(component.get_interactable_action_text(pawn, interaction)), admitted=admission is not None, angle_error=error,
            last_result=_optional(lambda: str(actor.get_editor_property('last_result'))))
        if error < 3. and admission is not None and focus == component:
            self.hold_actor, self.hold_beat = actor, beat
            self.saw_countdown = False
            self.hold_started = unreal.GameplayStatics.get_time_seconds(world)
            self.stage('hold_'+beat)

    def hold(self, world, pc, events):
        remaining = pc.get_interaction_component().get_editor_property('remaining_interact_time')
        if 0. < remaining <= .35:
            self.saw_countdown = True
        if any(e['beat'] == self.hold_beat for e in events):
            assert self.saw_countdown, 'Native hold countdown was not observed'
            self.inject()
            self.report['holds'].append(dict(beat=self.hold_beat, native_countdown=True,
                began_world_seconds=self.hold_started, journal=events))
            if self.hold_beat == 'SecureTarrikRoute':
                self.start_route([(-7000., -9200.), (-6950., -8700.)], 'aim_handoff')
            else:
                self.stage('wait_handoff')
        else:
            assert time.monotonic()-self.phase_at < 12., 'Hold did not produce its native receipt'
            self.inject(interact=1.)

    def combat(self, world, pc, pawn, weapon, events):
        encounter_state = self.e1.get_encounter_state()
        assert encounter_state not in (unreal.SovEncounterState.FAILED, unreal.SovEncounterState.RESTORING), 'E1 failed or was retried'
        assert self.e1.get_attempt_id().export_text() == self.attempt, 'E1 attempt changed'
        if encounter_state == unreal.SovEncounterState.SUCCEEDED:
            self.inject()
            assert self.e1.has_confirmed_victory(), 'E1 lacks its native confirmed defeat proof'
            if [e['beat'] for e in events] == BEATS[:2]:
                self.report['e1_victory'] = dict(attempt=self.attempt, roster=self.roster(), journal=events)
                self.stage('wait_pressure_gate_navigation')
            return
        assert encounter_state == unreal.SovEncounterState.ACTIVE, 'Unexpected E1 lifecycle state'
        candidates = [p.character for p in self.e1.participants if p.required_for_victory and p.character
                      and p.character.is_alive() and p.character.get_health() > 0.
                      and not p.character.get_editor_property('hidden')]
        if not candidates:
            self.inject()
            return
        location = pawn.get_actor_location()
        def ordering(actor):
            p = actor.get_actor_location()
            return (str(self.e1.find_participant_id(actor)) != 'E1.Drone2',
                    (p.x-location.x)**2+(p.y-location.y)**2)
        target = min(candidates, key=ordering)
        if target != self.target:
            self.target = target
            self.report['targets'].append(dict(elapsed=time.monotonic()-self.started,
                participant=str(self.e1.find_participant_id(target)), actor=_path(target), health=target.get_health()))
        target_location = target.get_actor_location()
        distance = math.hypot(target_location.x-location.x, target_location.y-location.y)
        look, error = self.look(world, pc, target_location)
        clear = self.clear_sight(world, pawn, target)
        # Walking along a queried path still goes through the player's real collision/movement input.
        in_range = distance < min(2400., max(500., weapon.get_attack_range()*.8))
        movement = self.approach(world, pc, pawn, target) if (not clear or not in_range) and distance > 450. else (0., 0.)
        clip, reserve = weapon.get_ammo_in_clip(), weapon.get_spare_ammo()
        assert clip > 0 or reserve > 0, 'Cinderline ammunition exhausted; no resources were manufactured'
        phase_time = unreal.GameplayStatics.get_time_seconds(world)
        # A short press/release cycle exercises normal input activation without holding through reload.
        reloading = clip <= 0
        reload_input = 1. if reloading and phase_time % 1.2 < .15 else 0.
        attack = 1. if not reloading and clear and in_range and error < 1.5 and phase_time % .6 < .4 else 0.
        self.report['last_combat'] = dict(target=str(self.e1.find_participant_id(target)), distance=distance,
            angle_error=error, clip=clip, reserve=reserve, visible_line=clear, in_range=in_range,
            primary_pressed=bool(attack), target_health=target.get_health())
        self.inject(move=movement, look=look, aim=0. if reloading else 1., attack=attack, reload=reload_input)

    def tick(self, delta):
        if self.done:
            return
        try:
            now = time.monotonic()
            assert now-self.started < 720., 'Continuation deadline exceeded'
            assert now-self.phase_at < (420. if self.phase == 'combat' else 100.), 'Stage deadline: '+self.phase
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world is not None and '/Aurelion/Maps/UEDPIE_' in world.get_path_name(), 'Requires existing Aurelion PIE'
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            if self.phase == 'wait_handoff' or self.phase == 'hold_HandoffToSelene':
                if not pawn or not isinstance(pc, unreal.SovPlayerController):
                    self.inject()
                    return
            assert isinstance(pc, unreal.SovPlayerController) and isinstance(pawn, unreal.SovPlayerCharacterBase)
            state = pc.get_campaign_state()
            assert state and state.get_active_mission() and str(state.get_active_mission().mission_id) == MISSION
            events = _journal(state)
            assert [e['beat'] for e in events] == BEATS[:len(events)] and len(events) <= 4, 'Unexpected journal order/content'
            assert all(e['mission'] == MISSION for e in events)
            if self.phase == 'initialize':
                assert isinstance(pawn, unreal.SovTarrikCharacter) and pawn.is_character_ready() and pawn.is_alive()
                assert pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE
                assert [e['beat'] for e in events] == BEATS[:1], 'Start immediately after TarrikArrival and E1 entry'
                self.world, self.initial_pawn = world, _path(pawn)
                self.owner = self.get_input_owner(world)
                self.initial_events = events
                matches = [d for d in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector)
                           if str(d.encounter_id) == 'M12_E1_PressureHall']
                assert len(matches) == 1
                self.e1 = matches[0]
                assert self.e1.get_encounter_state() == unreal.SovEncounterState.ACTIVE
                roster = self.roster()
                assert len(roster) == 6 and all(p['alive'] and p['health'] > 0. for p in roster)
                assert sum(not p['hidden'] for p in roster) == 4, 'Initial E1 wave must be four live, two reserved'
                weapon = self.weapon(pawn)
                self.attempt = self.e1.get_attempt_id().export_text()
                self.report['initial'] = dict(pawn=self.initial_pawn, world=_path(world), attempt=self.attempt,
                    roster=roster, journal=events, weapon=_path(weapon), clip=weapon.get_ammo_in_clip(), reserve=weapon.get_spare_ammo(),
                    mappings={n: [k.export_text() for k in self.owner.query_keys_mapped_to_action(a)] for n, a in self.actions.items()})
                self.stage('combat')
            assert world == self.world, 'This first handoff must preserve the actual world'
            assert events[:len(self.initial_events)] == self.initial_events, 'Existing journal identity changed'
            if now-self.last_sample > .5:
                self.last_sample = now
                self.report['samples'].append(dict(elapsed=now-self.started, phase=self.phase, pawn=_path(pawn),
                    position=_xyz(pawn.get_actor_location()), health=pawn.get_health(), ready=pawn.is_character_ready(),
                    transition=str(pc.get_campaign_transition_state()), protagonist=str(state.get_active_protagonist()),
                    journal=events, encounter=str(self.e1.get_encounter_state()), roster=self.roster()))
            if now-self.last_write > 1.:
                self.last_write = now
                self.write()
            assert pawn.is_alive() and pawn.get_health() > 0., 'Player died; no retry, healing, or resurrection was issued'
            if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world):
                self.inject()
                return
            if self.phase.startswith('hold_'):
                self.hold(world, pc, events)
                return
            if pc.get_campaign_transition_state() != unreal.SovCampaignTransitionState.IDLE:
                self.inject()
                return
            if self.phase == 'combat':
                self.combat(world, pc, pawn, self.weapon(pawn), events)
            elif self.phase == 'wait_pressure_gate_navigation':
                self.inject()
                gates = [g for g in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionJournalGate)
                         if str(g.beat_id) == 'PressureHall' and g.use_gate_body]
                assert len(gates) == 1
                path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,
                    unreal.Vector(-7000., -12000., 0.), unreal.Vector(-7000., -11400., 0.), pawn, None)
                valid = path is not None and path.is_valid() and not path.is_partial()
                self.report['pressure_gate_navigation'] = dict(blocking=gates[0].is_blocking_route(), complete=valid,
                    points=[_xyz(p) for p in path.path_points] if path else [])
                if valid and not gates[0].is_blocking_route():
                    self.start_route([(-7000., -12200.), (-7000., -11600.), (-7000., -9800.), (-6650., -9200.)], 'aim_secure')
            elif self.phase == 'walk_route':
                self.walk_route(pc, pawn)
            elif self.phase == 'aim_secure':
                self.acquire_hold(world, pc, pawn, unreal.SovCampaignInteractionTerminal,
                                  'terminal_id', 'Aurelion_SecureTarrikRoute', 'SecureTarrikRoute')
            elif self.phase == 'aim_handoff':
                self.acquire_hold(world, pc, pawn, unreal.SovAurelionRequestActor,
                                  'beat_id', 'HandoffToSelene', 'HandoffToSelene')
            elif self.phase == 'wait_handoff':
                self.inject()
                assert [e['beat'] for e in events] == BEATS
                if isinstance(pawn, unreal.SovSeleneCharacter) and _path(pawn) != self.initial_pawn:
                    position = pawn.get_actor_location()
                    assert math.hypot(position.x-7000., position.y+19400.) < 300., 'Unexpected handoff destination'
                    self.report['handoff'] = dict(pawn=_path(pawn), ready=True, health=pawn.get_health(),
                        position=_xyz(position), world=_path(world), protagonist=str(state.get_active_protagonist()), journal=events)
                    self.finish(True, 'Ordinary Cinderline combat earned native E1 victory; two actual holds secured the route and handed off to a ready Selene. Later route remains unqualified.')
        except Exception:
            self.report['error'] = traceback.format_exc()
            self.finish(False, self.report['error'])


def start(output_directory=None):
    """Run only after other input drivers stop and actual UI selection wields Cinderline."""
    global _RUN
    assert _RUN is None or _RUN.done, 'Continuation already running; call stop() first'
    target = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (target / 'e1-input-continuation.json').exists(), 'Use a new output directory to preserve prior evidence'
    _RUN = Run(target)
    _RUN.write()
    _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    """Release injected buttons and stop recording; leave the current game untouched."""
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False, 'Stopped by operator')


if __name__ == '__main__':
    start()
