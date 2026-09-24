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


def _incoming_damage_observer(run, pawn_path):
    # One-argument delegate; capture the run and exact pawn in a factory.
    def damaged(result):
        if _path(result.target_actor) != pawn_path:
            return
        run.report['incoming_damage'].append(dict(elapsed=time.monotonic()-run.started,
            source=_path(result.source_actor), health=result.applied_health_damage,
            shield=result.applied_shield_damage, poise=result.applied_poise_damage, fatal=result.fatal,
            relief=run.relief_active()))
    return damaged


# The pilot follows the game's own fatal recovery after a death, like a player, within these bounds.
NATIVE_RETRY_LIMIT = 3
# The authored checkpoint reconstructs the PIE world and asynchronously
# initializes its campaign pawn; loaded editor asset work can exceed a minute.
NATIVE_RECOVERY_SECONDS = 120.


def _recovery_state(pawn):
    component = pawn.get_component_by_class(unreal.SovFatalRecoveryComponent) if pawn else None
    return str(component.get_recovery_state()) if component else None

class Run:
    def __init__(self, output_directory, resume_report=None):
        self.out = Path(output_directory)
        self.out.mkdir(parents=True, exist_ok=True)
        self.root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
        self.assets = sorted((self.root / 'Content/Aurelion').rglob('*.uasset'))
        self.assets += sorted((self.root / 'Content/Aurelion').rglob('*.umap'))
        self.before = self.hashes()
        self.resume_report = resume_report
        if resume_report:
            assert resume_report['assets_after'] == self.before, 'Assets changed since retained combat failure'
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
        self.occluded_since = None
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
        self.last_evade_request = -1000.
        self.evade_until = -1000.
        self.cover_goal = None
        self.cover_until = -1000.
        self.next_cover_search = -1000.
        self.last_pickup = None
        self.pressure = None
        self.damage_binding = None
        self.recovery_started = None
        self.recovery_attempt = None
        self.retry_input = None
        self.report = dict(status='running', scope='E1 combat, secure approach, first native handoff',
                           method='Ordinary Enhanced Input actions in an existing PIE world',
                           physical_keyboard_validation=False, rendered_image_review=False,
                           direct_state_or_resource_or_transform_writes=False,
                           driver_source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                           samples=[], stages=[], holds=[], targets=[], input_frames={}, rocket_reactions=[],
                           cover_attempts=[], cover_exposures=[], occluded_target_switches=[],
                           pickup_approaches=[], native_retries=[],
                           incoming_damage=[], pressure_observation='Read-only native damage receipts and coordination relief state',
                           assets_before=self.before)
        self.actions = {name: unreal.load_asset(ACTION_ROOT + name) for name in
                        ('IA_Move', 'IA_Look', 'IA_Attack', 'IA_AltAttack', 'IA_Reload', 'IA_Interact')}
        self.actions['IA_Evade'] = unreal.load_asset('/Game/Input/IA_Evade')
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

    def inject(self, move=(0., 0.), look=(0., 0.), attack=0., aim=0., reload=0., interact=0., evade=0.):
        if not self.owner:
            return
        values = {'IA_Move': (*move, 0.), 'IA_Look': (*look, 0.),
                  'IA_Attack': (attack, 0., 0.), 'IA_AltAttack': (aim, 0., 0.),
                  'IA_Reload': (reload, 0., 0.), 'IA_Interact': (interact, 0., 0.),
                  'IA_Evade': (evade, 0., 0.)}
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

    def follow_native_recovery(self, now, world, pc, pawn):
        # No input, healing or state writes: the fatal recovery component rescues or retries on its own timers.
        self.inject()
        state = _recovery_state(pawn)
        if self.phase != 'native_recovery':
            assert self.phase == 'combat', 'Player died outside E1 combat; no recovery is followed here: ' + self.phase
            assert len(self.report['native_retries']) < NATIVE_RETRY_LIMIT, 'Player died again after the bounded native retries'
            self.recovery_started = now
            self.recovery_attempt = self.e1.get_attempt_id().export_text()
            self.report['native_retries'].append(dict(died_elapsed=now-self.started, attempt=self.recovery_attempt,
                health=pawn.get_health(), recovery_state=state, position=_xyz(pawn.get_actor_location()),
                alive_roster=[r['id'] for r in self.roster() if r['alive']]))
            self.stage('native_recovery')
            self.phase_at = now
            return
        row = self.report['native_retries'][-1]
        row['last_recovery_state'] = state
        assert state is None or 'FAILED' not in state.upper(), 'Native fatal recovery reported failure'
        assert now - self.recovery_started < NATIVE_RECOVERY_SECONDS, 'Native fatal recovery did not resume play in time'
        if world != self.world:
            # Fatal recovery may take the authored checkpoint-load path. PIE
            # keeps the M12 package name but replaces every actor and world
            # object; rebind the input and damage observers to that world.
            assert 'L_Aurelion_M12' in _path(world), 'Native recovery loaded a different map'
            row['checkpoint_world_replaced'] = True
            if self.damage_binding is not None:
                _optional(lambda binding=self.damage_binding: binding[0].remove_callable(binding[1]))
                self.damage_binding = None
            self.damage_pawn = None
            self.world = world
            self.owner = self.get_input_owner(world)
            self.e1 = None
            self.pressure = None
        players_ready = isinstance(pc, unreal.SovPlayerController) and isinstance(pawn, unreal.SovPlayerCharacterBase)
        campaign = pc.get_campaign_state() if players_ready else None
        mission = campaign.get_active_mission() if campaign else None
        transition = str(pc.get_campaign_transition_state()) if players_ready else None
        if not (players_ready and mission and pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE):
            # Transient checkpoint-load state: record it and keep waiting within the recovery bound.
            row['load_transients'] = row.get('load_transients', 0) + 1
            row['last_transient'] = dict(pawn=_path(pawn) if pawn else None, mission=bool(mission), transition=transition)
            return
        assert str(mission.mission_id) == MISSION, 'Native recovery restored a different mission'
        if self.e1 is None or not unreal.SystemLibrary.is_valid(self.e1):
            matches = [d for d in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector)
                       if str(d.encounter_id) == 'M12_E1_PressureHall']
            assert len(matches) == 1, 'Native recovery left no single E1 director'
            self.e1 = matches[0]
            row['e1_director_replaced'] = True
        encounter = self.e1.get_encounter_state()
        row.update(last_e1_state=str(encounter), last_pawn=_path(pawn),
                   checkpoint_load=bool(row.get('load_transients') or row.get('checkpoint_world_replaced')))
        if (encounter == unreal.SovEncounterState.FAILED and pawn.is_alive()
                and pawn.get_health() > 0. and pawn.is_character_ready()
                and (state is None or 'READY' in state.upper())):
            # A loaded active encounter deliberately remains Failed until its
            # physical retry hold. Follow that authored player action after
            # fatal recovery falls back to a checkpoint.
            from aurelion_retry_input import RetryInput
            self.retry_input = RetryInput(world, self.e1,
                self.out / ('AuthoredRetry'+str(len(self.report['native_retries']))))
            row['authored_retry_required'] = True
            self.stage('authored_retry')
            return
        attempt = self.e1.get_attempt_id().export_text()
        resumed = (pawn.is_alive() and pawn.get_health() > 0. and pawn.is_character_ready()
                   and encounter == unreal.SovEncounterState.ACTIVE
                   and (state is None or any(name in state.upper() for name in ('READY', 'RESCUED'))))
        if not resumed:
            return
        self.pressure = self.e1.get_component_by_class(unreal.SovEncounterCoordinationComponent)
        if _path(pawn) != getattr(self, 'damage_pawn', None):
            # A checkpoint load can respawn the protagonist; observe the restored pawn's native damage from here on.
            if self.damage_binding is not None:
                _optional(lambda binding=self.damage_binding: binding[0].remove_callable(binding[1]))
            damage_delegate = pawn.get_narrative_ability_system_component().on_damage_resolved_as_target
            damage_callback = _incoming_damage_observer(self, _path(pawn))
            damage_delegate.add_callable(damage_callback)
            self.damage_binding = (damage_delegate, damage_callback)
            self.damage_pawn = _path(pawn)
            row['damage_observer_rebound'] = True
        row.update(resumed_elapsed=now-self.started, recovery_seconds=now-self.recovery_started,
                   new_attempt=attempt, rescued_same_attempt=attempt == self.recovery_attempt,
                   health=pawn.get_health(), position=_xyz(pawn.get_actor_location()))
        self.attempt = attempt
        self.target = self.cover_goal = self.last_pickup = self.path_target = None
        self.occluded_since = None
        self.path_points = []
        self.cover_until = self.next_cover_search = -1000.
        self.last_position = None
        self.last_motion_at = now
        self.stage('combat')

    def follow_authored_retry(self, now, world, pc, pawn):
        assert self.retry_input is not None and world == self.world
        if not self.retry_input.step(world, pc, pawn):
            return
        row = self.report['native_retries'][-1]
        row['authored_retry'] = dict(samples=self.retry_input.samples,
            input_frames=self.retry_input.driver.report['input_frames'].copy(),
            active_attempt=self.e1.get_attempt_id().export_text())
        self.retry_input.stop()
        self.retry_input = None
        self.stage('native_recovery')
        self.follow_native_recovery(now, world, pc, pawn)

    def relief_active(self):
        return _optional(lambda: bool(self.pressure.is_pressure_relief_active())) if self.pressure else None

    def release_world_references(self):
        # Completed input observers must not keep an old PIE world alive across
        # the mission's genuine map travel. Serialized evidence remains intact.
        if self.retry_input is not None:
            self.retry_input.stop()
            self.retry_input = None
        if self.damage_binding is not None:
            _optional(lambda binding=self.damage_binding: binding[0].remove_callable(binding[1]))
            self.damage_binding = None
        self.pressure = None
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
        # Mouse-delta input must turn fast enough to follow an orbiting drone;
        # the previous .7 cap repeatedly lagged behind during lateral movement.
        clamp = lambda v: max(-6., min(6., v))
        return (clamp(yaw*.8/ys), clamp(pitch*.8/ps)), max(abs(yaw), abs(pitch))

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
            valid = path is not None and path.is_valid()
            points = list(path.path_points) if valid else []
            partial = valid and path.is_partial()
            origin = pawn.get_actor_location()
            advance = math.hypot(points[-1].x-origin.x, points[-1].y-origin.y) if len(points) >= 2 else 0.
            # A partial Recast path ends on reachable navmesh before an
            # obstruction. Follow that prefix, then request a fresh path;
            # never steer directly toward the blocked target beyond it.
            usable = valid and len(points) >= 2 and (not partial or advance >= 100.)
            self.report['last_combat_path'] = dict(target=_path(target), complete=valid and not partial,
                                                  partial=partial, advance_cm=round(advance, 1),
                                                  points=[_xyz(p) for p in points])
            self.path_points = points[1:] if usable else []
        while self.path_points:
            # Recast can place adjacent corners less than 80 cm apart around
            # railings. Skipping both cuts across their collision instead of
            # following the complete path. Match the route-walking tolerance.
            result, reached = self.local_move(pc, pawn, _xyz(self.path_points[0]), stop=25.)
            if not reached:
                return result
            self.path_points.pop(0)
        return (0., 0.)

    def approaching_rocket(self, world, pawn):
        """Read actual visible projectiles; never alter their flight or resolution."""
        location = pawn.get_actor_location()
        for rocket in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovReformationDroneRocketProjectile):
            if rocket.has_resolved() or rocket.get_editor_property('hidden'):
                continue
            p, velocity = rocket.get_actor_location(), rocket.get_velocity()
            dx, dy, dz = location.x-p.x, location.y-p.y, location.z-p.z
            distance = math.sqrt(dx*dx+dy*dy+dz*dz)
            if distance < 1. or distance > 1000.:
                continue
            closing = (dx*velocity.x+dy*velocity.y+dz*velocity.z)/distance
            if closing > 100. and distance/closing < .28 and self.clear_sight(world, pawn, rocket):
                return dict(projectile=_path(rocket), distance=distance, estimated_seconds=distance/closing)
        return None

    def cover_trace_blocked(self, raw):
        """A trace shelters the pawn only when world geometry blocks the ray.

        Unreal Python can return (blocking, HitResult) rather than a bare hit.
        A non-None tuple with blocking=False is still an exposed sightline.
        """
        if raw is None:
            return False
        hits = [v for v in raw if isinstance(v, unreal.HitResult)] if isinstance(raw, tuple) else [raw]
        for hit in hits:
            parts = hit.to_tuple()
            obstacle = parts[9]
            if parts[0] and obstacle and not isinstance(obstacle, unreal.NarrativeCharacter):
                return True
        return False

    def cover_sample_offsets(self, pawn):
        capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
        assert capsule, 'E1 cover check needs the live player capsule'
        half_height = float(capsule.get_scaled_capsule_half_height())
        assert 60. <= half_height <= 140., 'Unexpected E1 player capsule height'
        # The actor origin is the capsule centre. A +150 cm ray was above
        # Tarrik's measured 88 cm half-height and rejected valid body cover.
        return (0., min(75., half_height-10.))

    def cover_movement(self, world, pc, pawn, enemies, phase_time):
        shield = pawn.get_component_by_class(unreal.SovShieldComponent)
        assert shield and shield.is_initialized(), 'Native shield readiness missing'
        sample_offsets = self.cover_sample_offsets(pawn)
        value = shield.get_shield()
        if self.cover_goal is not None and (value >= shield.get_max_shield()*.85 or phase_time > self.cover_until):
            self.cover_goal = None
        if self.cover_goal is None and value < shield.get_max_shield()*.35 and phase_time > self.next_cover_search:
            self.next_cover_search = phase_time+2.
            location = pawn.get_actor_location()
            choices = []
            for radius in (450., 850.):
                for index in range(8):
                    angle = index*math.pi/4.
                    goal = unreal.Vector(location.x+math.cos(angle)*radius, location.y+math.sin(angle)*radius, location.z)
                    path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world, location, goal, pawn, None)
                    if not path or not path.is_valid() or path.is_partial() or len(path.path_points)<2:
                        continue
                    end = path.path_points[-1]
                    if abs(end.z-(location.z-88.))>150. or math.hypot(end.x-goal.x,end.y-goal.y)>150.:
                        continue
                    # Test the same capsule-centre/upper-body heights at selection and arrival.
                    # A single mid-height ray chose low coffers that exposed the
                    # head, causing repeated moves to immediately rejected cover.
                    nav_to_pawn_height = location.z-path.path_points[0].z
                    if not 0. <= nav_to_pawn_height <= 200.:
                        continue
                    blocked = 0
                    for enemy in enemies:
                        ignored = [pawn,enemy]+list(pawn.get_attached_actors())
                        ignored += [v for v in (pawn.get_character_visual(),enemy.get_character_visual()) if v]
                        rays = [unreal.SystemLibrary.line_trace_single(world,
                            end+unreal.Vector(0.,0.,nav_to_pawn_height+height),
                            enemy.get_actor_location(), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False,
                            ignored, unreal.DrawDebugTrace.NONE, True)
                            for height in sample_offsets]
                        if all(self.cover_trace_blocked(ray) for ray in rays):
                            blocked += 1
                    # Recovery waits require shelter from every current enemy,
                    # matching the arrival exposure gate below. Partial shelter
                    # otherwise causes repeated travel to immediately rejected spots.
                    if blocked == len(enemies) and blocked:
                        choices.append((-blocked, radius, [_xyz(p) for p in path.path_points[1:]]))
            if choices:
                _, _, points = min(choices, key=lambda c:(c[0],c[1]))
                self.cover_goal = points
                self.cover_until = phase_time+10.
                self.report['cover_attempts'].append(dict(elapsed=time.monotonic()-self.started,
                    shield=value, blocked_enemies=-min(c[0] for c in choices), points=points.copy()))
        if self.cover_goal is not None:
            while self.cover_goal:
                movement, reached = self.local_move(pc,pawn,self.cover_goal[0],stop=25.)
                if not reached:
                    return movement
                self.cover_goal.pop(0)
            if value < shield.get_max_shield()*.85 and phase_time <= self.cover_until:
                # Flying enemies can invalidate a previously sheltered point.
                # Recheck from the pawn, not the camera (which can be behind a wall).
                exposed = []
                for enemy in enemies:
                    ignored = [pawn, enemy] + list(pawn.get_attached_actors())
                    ignored += [v for v in (pawn.get_character_visual(), enemy.get_character_visual()) if v]
                    # A low coffer can hide the centre while leaving the upper
                    # capsule exposed to flying drones. Both samples need shelter.
                    rays = [unreal.SystemLibrary.line_trace_single(world,
                        pawn.get_actor_location()+unreal.Vector(0.,0.,height), enemy.get_actor_location(),
                        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignored, unreal.DrawDebugTrace.NONE, True)
                        for height in sample_offsets]
                    if any(not self.cover_trace_blocked(ray) for ray in rays):
                        exposed.append(_path(enemy))
                if exposed:
                    self.report['cover_exposures'].append(dict(elapsed=time.monotonic()-self.started,
                        enemies=exposed, shield=value))
                    self.cover_goal = None
                    self.next_cover_search = phase_time+.5
                    return None
                return (0.,0.)
        return None

    def ammo_movement(self, world, pc, pawn, weapon):
        if weapon.get_spare_ammo() >= 64:
            self.last_pickup = None
            return None
        location = pawn.get_actor_location()
        pickups = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAmmoCombatSustainPickup)
            if not a.is_claimed() and not a.get_editor_property('hidden')
            and a.get_ammo_item_class() == weapon.get_editor_property('required_ammo')]
        def pickup_distance(actor):
            p = actor.get_actor_location()
            return (p.x-location.x)**2+(p.y-location.y)**2+(p.z-location.z)**2
        pickups.sort(key=pickup_distance)
        for pickup in pickups:
            p = pickup.get_actor_location()
            if math.hypot(p.x-location.x,p.y-location.y)>1800.:
                continue
            path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,location,p,pawn,None)
            if not path or not path.is_valid() or path.is_partial():
                continue
            if self.last_pickup != _path(pickup):
                self.last_pickup = _path(pickup)
                self.report['pickup_approaches'].append(dict(elapsed=time.monotonic()-self.started,
                    actor=self.last_pickup, quantity=pickup.get_ammo_quantity(), reserve=weapon.get_spare_ammo()))
            return self.approach(world,pc,pawn,pickup)
        return None

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
        # Kitbash/layout revisions can move the physical terminal away from the
        # coarse traversal waypoint. Approach its current location through the
        # same navigation/input path before asking for focus or holding interact.
        position = pawn.get_actor_location()
        destination = actor.get_actor_location()
        interaction_range = float(component.get_editor_property('interaction_distance'))
        if isinstance(actor, unreal.SovCampaignHandoffAnchor):
            interaction_range = min(interaction_range, float(actor.get_editor_property('request_range')))
        assert math.isfinite(interaction_range) and interaction_range > 0., 'Invalid authored interaction range'
        dx, dy = position.x-destination.x, position.y-destination.y
        distance = math.hypot(dx, dy)
        if distance > interaction_range*.8:
            assert distance > 1.
            stand_off = interaction_range*.55
            approach = (destination.x+dx/distance*stand_off, destination.y+dy/distance*stand_off)
            self.report.setdefault('authored_interaction_approaches', []).append(dict(
                actor=_path(actor), position=_xyz(destination), range=interaction_range, approach=approach))
            self.start_route([approach], self.phase)
            return
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
        if os.environ.get('SOV_E1_DEATH_RECOVERY_PROBE') == '1' and not self.report['native_retries']:
            # Stand in ordinary enemy fire until native fatal recovery runs.
            # This optional probe supplies movement input only: no health,
            # resource, damage, encounter, or checkpoint state is changed.
            started = self.report.setdefault('death_probe_started', time.monotonic()-self.started)
            exposure_bound = float(os.environ.get('SOV_E1_EXPOSURE_SECONDS', '150'))
            assert 20. <= exposure_bound <= 150.
            assert time.monotonic()-self.started-started < exposure_bound, 'No native player death during bounded E1 exposure'
            position = pawn.get_actor_location()
            nearest = min(candidates, key=lambda actor: pawn.get_distance_to(actor))
            distance = pawn.get_distance_to(nearest)
            clear = self.clear_sight(world, pawn, nearest)
            movement = self.approach(world, pc, pawn, nearest) if distance > 1200. or not clear else (0., 0.)
            self.report['death_probe'] = dict(target=_path(nearest), distance=distance,
                clear_sight=clear, movement=movement, health=pawn.get_health(),
                incoming_damage_count=len(self.report['incoming_damage']))
            self.inject(move=movement)
            return
        location = pawn.get_actor_location()
        def ordering(actor):
            p = actor.get_actor_location()
            return (str(self.e1.find_participant_id(actor)) != 'E1.Drone2',
                    (p.x-location.x)**2+(p.y-location.y)**2)
        # Keep tracking a living target. Re-ranking moving drones every
        # frame made the driver switch 60 times in one failed run, often turning
        # away before it could fire. This changes only the ordinary-input pilot.
        target = self.target if self.target in candidates else min(candidates, key=ordering)
        if target != self.target:
            self.target = target
            self.report['targets'].append(dict(elapsed=time.monotonic()-self.started,
                participant=str(self.e1.find_participant_id(target)), actor=_path(target), health=target.get_health()))
        target_location = target.get_actor_location()
        clear = self.clear_sight(world, pawn, target)
        now = time.monotonic()
        if clear:
            self.occluded_since = None
        else:
            if self.occluded_since is None:
                self.occluded_since = now
            elif now-self.occluded_since > 2.:
                visible = [actor for actor in candidates if actor != target and self.clear_sight(world, pawn, actor)]
                if visible:
                    replacement = min(visible, key=ordering)
                    self.report['occluded_target_switches'].append(dict(
                        elapsed=now-self.started, from_target=str(self.e1.find_participant_id(target)),
                        to_target=str(self.e1.find_participant_id(replacement)),
                        occluded_seconds=now-self.occluded_since))
                    target = self.target = replacement
                    target_location = target.get_actor_location()
                    self.occluded_since = None
                    clear = self.clear_sight(world, pawn, replacement)
                    self.report['targets'].append(dict(elapsed=now-self.started,
                        participant=str(self.e1.find_participant_id(target)),
                        actor=_path(target), health=target.get_health()))
        distance = math.hypot(target_location.x-location.x, target_location.y-location.y)
        look, error = self.look(world, pc, target_location)
        # Walking along a queried path still goes through the player's real collision/movement input.
        in_range = distance < min(2400., max(500., weapon.get_attack_range()*.8))
        # Occlusion can persist inside 450 cm (for example across a ramp).
        # Follow the queried path there too, preserving normal capsule collision.
        movement = self.approach(world, pc, pawn, target) if (not clear or not in_range) and distance > 80. else (0., 0.)
        clip, reserve = weapon.get_ammo_in_clip(), weapon.get_spare_ammo()
        phase_time = unreal.GameplayStatics.get_time_seconds(world)
        # Do not stand exposed through every shot/reload. These alternating
        # lateral inputs still obey the player's movement, collision and aim.
        if clear and in_range:
            movement = (.8 if phase_time % 5. < 2.5 else -.8, 0.)
        cover_move = self.cover_movement(world,pc,pawn,candidates,phase_time)
        pickup_move = self.ammo_movement(world,pc,pawn,weapon) if cover_move is None else None
        assert clip > 0 or reserve > 0 or pickup_move is not None, 'Cinderline ammunition exhausted with no reachable matching pickup; no resources were manufactured'
        if cover_move is not None:
            movement = cover_move
        elif pickup_move is not None:
            movement = pickup_move
        # A short press/release cycle exercises normal input activation without holding through reload.
        reloading = clip <= 0
        reload_input = 1. if reloading and phase_time % 1.2 < .15 else 0.
        threat = self.approaching_rocket(world, pawn)
        if phase_time-self.last_evade_request > .45 and (threat or (cover_move is None and phase_time % 3. < .12)):
            self.last_evade_request = phase_time
            self.evade_until = phase_time+.12
            if threat:
                self.report['rocket_reactions'].append(dict(elapsed=time.monotonic()-self.started, **threat))
        evade_input = 1. if phase_time < self.evade_until else 0.
        # Diagnostic only: attempted spread/settling gates did not qualify.
        # Preserve the previously passing ordinary firing/movement policy.
        spread = float(weapon.get_weapon_spread())
        velocity = pawn.get_velocity()
        speed = math.sqrt(velocity.x**2 + velocity.y**2 + velocity.z**2)
        assert math.isfinite(spread) and spread >= 0. and math.isfinite(speed)
        # Keep returning fire while running toward shelter. The old pilot
        # dropped its weapon for the whole approach (up to ten seconds), even
        # when it had a clear shot; a player can move and fire together.
        moving_to_cover = cover_move is not None and math.hypot(*cover_move) > .01
        can_fire = cover_move is None or moving_to_cover
        attack = 1. if can_fire and not reloading and clear and in_range and error < 1.2 and phase_time % .6 < .4 else 0.
        self.report['last_combat'] = dict(target=str(self.e1.find_participant_id(target)), distance=distance,
            angle_error=error, clip=clip, reserve=reserve, visible_line=clear, in_range=in_range,
            primary_pressed=bool(attack), target_health=target.get_health(), movement=movement,
            evade_requested=bool(evade_input), seeking_cover=cover_move is not None, seeking_ammo=pickup_move is not None,
            native_spread_degrees=spread, native_speed_cm_s=speed)
        self.inject(move=movement, look=look, aim=0. if reloading or evade_input or not can_fire else 1.,
                    attack=0. if evade_input else attack, reload=reload_input, evade=evade_input)

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
            if self.phase == 'native_recovery':
                self.follow_native_recovery(now, world, pc, pawn)
                return
            if self.phase == 'authored_retry':
                self.follow_authored_retry(now, world, pc, pawn)
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
                self.pressure = self.e1.get_component_by_class(unreal.SovEncounterCoordinationComponent)
                if self.pressure is not None:
                    self.report['pressure_configuration'] = {name: _optional(lambda n=name: self.pressure.get_editor_property(n)) for name in
                        ('allow_low_resource_relief', 'relief_duration', 'relief_cooldown', 'relief_attack_interval', 'melee_attacker_slots')}
                damage_delegate = pawn.get_narrative_ability_system_component().on_damage_resolved_as_target
                damage_callback = _incoming_damage_observer(self, _path(pawn))
                damage_delegate.add_callable(damage_callback)
                self.damage_binding = (damage_delegate, damage_callback)
                self.damage_pawn = _path(pawn)
                roster = self.roster()
                if self.resume_report:
                    initial = self.resume_report['initial']
                    assert _path(world) == initial['world'] and self.initial_pawn == initial['pawn']
                    assert self.e1.get_attempt_id().export_text() == initial['attempt'], 'Retained encounter attempt changed'
                    assert events == initial['journal'], 'Retained journal changed'
                    assert {p['id'] for p in roster} == {p['id'] for p in initial['roster']}
                    assert any(p['alive'] and p['health'] > 0. for p in roster), 'No remaining live combat'
                    self.report['retained_combat_retry'] = dict(previous_elapsed=self.resume_report['elapsed_seconds'],
                        previous_reason=self.resume_report['reason'], previous_status='failed')
                else:
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
                    journal=events, encounter=str(self.e1.get_encounter_state()), roster=self.roster(),
                    shield=_optional(lambda: pawn.get_component_by_class(unreal.SovShieldComponent).get_shield()),
                    relief=self.relief_active()))
            if now-self.last_write > 1.:
                self.last_write = now
                self.write()
            if self.phase == 'native_recovery' or not (pawn.is_alive() and pawn.get_health() > 0.):
                self.follow_native_recovery(now, world, pc, pawn)
                return
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


def start(output_directory=None, resume_report_path=None):
    """Use normal inputs; optional timeout retry requires the same retained encounter.

    A retry keeps its own report and never upgrades the earlier failure to a pass.
    It does not reset health, resources, targets, world, journal or encounter state.
    """
    global _RUN
    assert _RUN is None or _RUN.done, 'Continuation already running; call stop() first'
    assert _RUN is None or _RUN.handle is None, 'Previous callback must be retired'
    target = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (target / 'e1-input-continuation.json').exists(), 'Use a new output directory to preserve prior evidence'
    previous = None
    if resume_report_path:
        previous = json.loads(Path(resume_report_path).read_text(encoding='utf-8-sig'))
        assert previous.get('status') == 'failed' and previous.get('assets_unchanged') is True
        assert 'release_input' in previous and previous['release_input'] is None, 'Previous driver did not release input'
        assert 'AssertionError: Stage deadline: combat' in previous.get('reason', ''), 'Only retained combat timeout may resume'
        assert not previous.get('holds'), 'Cannot resume after progression interactions'
        assert Path(resume_report_path).resolve().parent != target.resolve(), 'Preserve failed evidence'
    _RUN = Run(target, previous)
    _RUN.write()
    _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    """Release injected buttons and stop recording; leave the current game untouched."""
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False, 'Stopped by operator')


if __name__ == '__main__':
    start()
