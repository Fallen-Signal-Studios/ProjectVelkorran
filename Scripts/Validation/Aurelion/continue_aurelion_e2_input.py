"""Current-PIE E2 continuation using ordinary Enhanced Input and native holds.

Stop other input drivers, then call start(new_output_directory) after the real
Selene handoff. Select Staccato in the visible wheel during its normal hold/release
cycle (35 seconds held, 10 released, bounded to 180 seconds). No automatic
equip, actor transform, resources, damage, death, director or campaign writes.
Import beside continue_aurelion_e1_input.py. Does nothing on import.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import re
import time
import traceback
import unreal
from aurelion_wheel_input import Selector
import continue_aurelion_e1_input as e1
import validate_owned_weapon_hud_readonly as hud_readonly

_RUN = None
MISSION = 'M12_FireAndFrost'
ENCOUNTER = 'M12_E2_RelayOverlook'
WEAPON = '/Game/Items/Weapons/WI_Staccato.WI_Staccato_C'
BEATS = ['TarrikArrival', 'PressureHall', 'SecureTarrikRoute', 'HandoffToSelene',
         'SeleneArrival', 'RelayOverlook']
RECEIVERS = ['M12_E2_ReceiverWest', 'M12_E2_ReceiverEast']
ROSTER = {'E2.Enforcer'+str(i) for i in range(1, 5)} | {'E2.Drone1', 'E2.Drone2'}


def _valid_guid(value):
    text = value.export_text()
    return bool(re.fullmatch(r'[0-9A-Fa-f]{32}', text)) and int(text, 16) != 0


def _distance_squared(a, b):
    return (a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2


def _events(state):
    return [dict(mission=str(e.mission_id), beat=str(e.beat_id),
                 id=e.event_id.export_text(), sequence=e.sequence,
                 handoff=e.handoff_request_id.export_text(), handoff_anchor=str(e.handoff_anchor_id),
                 encounter=str(e.encounter_id), attempt=e.encounter_attempt_id.export_text(),
                 receivers=sorted(str(v) for v in e.disabled_receiver_ids))
            for e in state.get_journal()]


def _admitted(value):
    # UE's bool + FText(out) wrapper returns Text on success and None on failure.
    # Also accept the explicit bool tuple shape without truth-testing error text.
    if value is None:
        return False
    if isinstance(value, bool):
        return value
    if isinstance(value, tuple):
        flags = [v for v in value if isinstance(v, bool)]
        assert len(flags) == 1, 'Unexpected CanInteract tuple: '+str(value)
        return flags[0]
    assert isinstance(value, unreal.Text), 'Unexpected CanInteract result: '+str(type(value))
    return True


# A snag on a complete native path gets bounded ordinary sidesteps before the stall deadline.
UNSTICK_AFTER_SECONDS = 6.
UNSTICK_SECONDS = .6
UNSTICK_LIMIT = 3

class Run(e1.Run):
    def __init__(self, output_directory):
        super().__init__(output_directory)
        self.actions['IA_WeaponWheel'] = unreal.load_asset(e1.ACTION_ROOT+'IA_WeaponWheel')
        assert self.actions['IA_WeaponWheel'], 'The authored weapon-wheel action is missing'
        self.report.update(scope='Selene approach, E2 combat and two native close receiver interactions',
            receiver_contract='Docs/AurelionLayoutContract-2026-09-07.md:65 and AurelionRelayReceiverProof.md',
            projectile_receiver_disable_claim=False, holds=[], receiver_snapshots=[],
            ability_activation_claim=False, inventory=[], initial_input_owner_scope='one engine-scoped local player; exact controller validated separately')
        self.controller_path = None
        self.wheel_selector = None
        self.asc_path = None
        self.ready_epoch = None
        self.receivers = {}
        self.receiver_index = 0
        self.hold_spec = None
        self.hold_positive = []
        self.hold_last_world_time = None
        self.e2 = None
        self.objective = None
        self.settings_snapshot = None
        self.combat_observers=[]
        self.roster_paths={}
        self.fire_control=dict(settle_since=None,key=None,shot=None,misses=0,flank_origin=None,flanks=0)
        self.report['shot_outcomes']=[]
        self.cover=None
        self.fire_until=self.next_shot_at=0.
        self.last_health_progress=self.started
        self.last_health_signature=None
        self.report.update(native_damage=[],firing_requests=[],aim_candidates=[],combat_tactics=[],reloads=[])
        self.script_files = [Path(__file__).resolve(), Path(e1.__file__).resolve(), Path(hud_readonly.__file__).resolve()]
        self.report['scripts_before'] = self.script_hashes()

    def owned_hud_gate(self, key):
        gate = self.report.setdefault('owned_hud_gates', {}).setdefault(key, {'started': time.monotonic(), 'samples': 0, 'passed': False})
        if gate['passed']: return True
        observed = hud_readonly.sample()
        gate['samples'] += 1; gate.setdefault('first_sample', observed); gate['last_sample'] = observed
        gate['elapsed'] = time.monotonic() - gate['started']
        assert observed['status'] != 'failed', 'Owned HUD structural failure: '+str(observed)
        assert gate['elapsed'] <= 1., 'Owned HUD did not settle within one second: '+str(observed)
        gate['passed'] = observed['status'] == 'passed' and observed.get('shield', {}).get('qualification') == 'rounded_value_matches'
        return gate['passed']

    def inject(self, *args, wheel=0., **kwargs):
        super().inject(*args, **kwargs)
        if self.owner:
            self.owner.inject_input_vector_for_action(self.actions['IA_WeaponWheel'], unreal.Vector(wheel, 0., 0.), [], [])
            if wheel:
                counts = self.report['input_frames']
                counts['IA_WeaponWheel'] = counts.get('IA_WeaponWheel', 0) + 1

    def script_hashes(self):
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in self.script_files}

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 3)
        temp = self.out / 'e2-input-continuation.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        e1.replace_report_with_retry(temp, self.out / 'e2-input-continuation.json')

    def finish(self, passed, reason):
        if self.done:
            return
        self.done = True
        self.report['release_input'] = e1._optional(lambda: self.inject())
        if self.wheel_selector is not None:
            e1._optional(self.wheel_selector.stop)
        for delegate,callback in self.combat_observers:
            e1._optional(lambda d=delegate,c=callback:d.remove_callable(c))
        self.combat_observers.clear()
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        after = self.hashes()
        scripts = self.script_hashes()
        self.report.update(status='passed' if passed else 'failed', reason=reason,
            assets_after=after, assets_unchanged=after == self.before,
            scripts_after=scripts, scripts_unchanged=scripts == self.report['scripts_before'], pie_left_running=True)
        if after != self.before or not self.report['scripts_unchanged']:
            self.report['status'] = 'failed'
        self.write()
        unreal.log('Aurelion E2 continuation: '+self.report['status']+': '+reason)
        # Future native mission travel must not retain this retired PIE world.
        self.release_world_references()
        self.e2=self.objective=self.cover=None
        self.hold_spec=self.wheel_selector=None
        self.receivers.clear();self.path_points.clear();self.waypoints.clear();self.actions.clear()
        self.path_target=None
        self.report['retained_gameplay_references_cleared']=True
        self.write()

    def settings(self):
        settings = unreal.GameUserSettings.get_game_user_settings()
        assert isinstance(settings, unreal.SovGameUserSettings), 'Expected actual project user settings'
        return settings.get_settings_snapshot()

    def weapon(self, pawn):
        weapons = [w for w in pawn.get_wielded_weapons() if w and e1._path(w.get_class()) == WEAPON]
        assert len(weapons) == 1, 'Select actual Staccato with the normal weapon-wheel UI before starting; no equip command was issued'
        assert weapons[0].is_equipped() and weapons[0].is_wielded(), 'Staccato lacks its ordinary equipped/wielded state'
        return weapons[0]

    def await_weapon(self, pawn):
        assert pawn.is_character_ready(), 'Selene readiness changed while waiting for actual UI selection'
        weapons = [w for w in pawn.get_wielded_weapons() if w and e1._path(w.get_class()) == WEAPON]
        assert len(weapons) <= 1, 'Multiple Staccato instances claim the current wield state'
        selected = len(weapons) == 1 and weapons[0].is_equipped() and weapons[0].is_wielded()
        elapsed = time.monotonic()-self.phase_at
        if self.wheel_selector is None:
            self.wheel_selector = Selector(WEAPON)
        held, wheel_report = self.wheel_selector.step(self.world)
        self.inject(wheel=1. if held else 0.)
        self.report['weapon_wheel'] = wheel_report
        assert not self.wheel_selector.done or wheel_report['status'] == 'passed', wheel_report.get('reason')
        self.report['selection_wait'] = dict(elapsed=elapsed,
            actual_wielded=[dict(item=e1._path(w), cls=e1._path(w.get_class())) for w in pawn.get_wielded_weapons() if w],
            required=WEAPON, method='Awaiting ordinary UI selection; only normal weapon-wheel input held/released')
        if not selected or not self.wheel_selector.done or wheel_report['status'] != 'passed':
            return
        weapon = self.weapon(pawn)
        assert weapon.get_ammo_in_clip() > 0 or weapon.get_spare_ammo() > 0, 'Actual selected Staccato has no ammunition'
        if weapon.get_ammo_in_clip()<=0:
            seconds=unreal.GameplayStatics.get_time_seconds(self.world)
            self.inject(reload=float(seconds%1.2<.15))
            self.report['pre_entry_reload']=dict(clip=weapon.get_ammo_in_clip(),reserve=weapon.get_spare_ammo(),ordinary_input=True)
            return
        if not self.owned_hud_gate('staccato_selected'):
            self.inject(); return
        asc = pawn.get_narrative_ability_system_component()
        abilities = []
        for handle in asc.get_all_abilities():
            result = unreal.AbilitySystemLibrary.get_gameplay_ability_from_spec_handle(asc, handle)
            candidates = result if isinstance(result, tuple) else (result,)
            ability = next((v for v in candidates if isinstance(v, unreal.GameplayAbility)), None)
            if ability:
                abilities.append(dict(cls=e1._path(ability.get_class()), input_tag=str(ability.get_editor_property('input_tag'))))
        self.report['actual_ability_grants'] = abilities
        required = [e1._path(cls) for cls in weapon.get_editor_property('weapon_abilities')]
        assert required and set(required).issubset({a['cls'] for a in abilities}), 'Selected Staccato lacks its ordinary weapon ability grants'
        self.report['required_weapon_abilities'] = required
        self.report['selection_complete'] = dict(elapsed=time.monotonic()-self.started, weapon=e1._path(weapon),
            clip=weapon.get_ammo_in_clip(), reserve=weapon.get_spare_ammo(), actual_ordinary_wield_observed=True)
        self.begin_route([(7000.,-18200.,100.), (7000.,-16400.,100.), (7000.,-14500.,100.), (7250.,-13380.,100.)], 'aim_arrival')

    def roster(self):
        rows=[]
        for participant in self.e2.participants:
            if not participant.required_for_victory:continue
            identity=str(participant.participant_id);actor=participant.character
            valid=unreal.SystemLibrary.is_valid(actor)
            expected=self.roster_paths.get(identity)
            if not valid:
                receipts=[r for r in self.report['native_damage'] if r['participant']==identity
                    and r['target']==expected and r['fatal'] and re.fullmatch(r'[0-9A-Fa-f]{32}',r['transaction'])
                    and int(r['transaction'],16)!=0]
                assert expected and receipts, 'Participant retired without its exact observed native fatal receipt: '+identity
                rows.append(dict(id=identity,actor=None,health=0.,alive=False,hidden=None,
                    retired_actor=expected,fatal_transactions=[r['transaction'] for r in receipts]))
                continue
            path=e1._path(actor)
            assert expected is None or path==expected, 'Participant actor identity changed: '+identity
            rows.append(dict(id=identity,actor=path,health=actor.get_health(),alive=actor.is_alive(),
                hidden=actor.get_editor_property('hidden')))
        return rows

    def receiver_snapshot(self):
        return {key: dict(actor=e1._path(actor), disabled=actor.is_disabled(),
            pending=actor.is_request_pending(), error=str(actor.last_error), position=e1._xyz(actor.get_actor_location()))
            for key, actor in self.receivers.items()}

    def begin_route(self, points, then):
        self.waypoints, self.route_then = list(points), then
        self.path_target = None
        self.path_points = []
        self.stage('walk_route', dict(points=points, then=then))

    def walk_route(self, pc, pawn):
        if not self.waypoints:
            self.inject()
            self.stage(self.route_then)
            return
        destination = self.waypoints[0]
        _, reached = self.local_move(pc, pawn, destination, stop=32.)
        if reached:
            self.waypoints.pop(0)
            self.path_target = None
            self.last_motion_at = time.monotonic()
            self.unstick_count = 0
            self.unstick_until = None
            self.inject()
            return
        now = time.monotonic()
        if getattr(self, 'unstick_until', None) is not None:
            if now < self.unstick_until:
                # Ordinary sidestep away from whatever holds the capsule, then re-path from the new position.
                self.inject(move=self.unstick_move)
                return
            self.unstick_until = None
            self.path_target = None
        if self.path_target != destination or now-self.last_path > .8:
            self.path_target, self.last_path = destination, now
            path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(
                self.world, pawn.get_actor_location(), unreal.Vector(*destination), pawn, None)
            valid = path is not None and path.is_valid() and not path.is_partial()
            self.path_points = list(path.path_points)[1:] if valid else []
            self.report['last_route_path'] = dict(destination=destination, complete=valid,
                points=[e1._xyz(p) for p in path.path_points] if path else [])
        movement = (0., 0.)
        while self.path_points:
            movement, done = self.local_move(pc, pawn, e1._xyz(self.path_points[0]), stop=25.)
            if not done:
                break
            self.path_points.pop(0)
        self.inject(move=movement)
        pos = pawn.get_actor_location()
        if self.last_position is None or math.hypot(pos.x-self.last_position[0], pos.y-self.last_position[1]) > 35.:
            self.last_position, self.last_motion_at = e1._xyz(pos), now
        stalled = now-self.last_motion_at
        if stalled > UNSTICK_AFTER_SECONDS and getattr(self, 'unstick_count', 0) < UNSTICK_LIMIT:
            self.unstick_count = getattr(self, 'unstick_count', 0) + 1
            side = 1. if self.unstick_count % 2 else -1.
            # local_move returns (right, forward); a right-only input steps perpendicular to the blocked lane.
            self.unstick_move = (side, 0.)
            self.unstick_until = now + UNSTICK_SECONDS
            self.last_motion_at = now
            self.report.setdefault('unstick_attempts', []).append(dict(elapsed=now-self.started, destination=destination,
                position=e1._xyz(pos), side=side, attempt=self.unstick_count,
                path=self.report.get('last_route_path')))
            return
        assert stalled < 18., 'Ordinary movement stalled; inspect recorded native path/collision'

    def prepare_hold(self, actor, identity, then):
        self.hold_spec = dict(actor=actor, identity=identity, then=then,
            component=actor.interactable, duration=float(actor.interactable.interaction_time))
        assert self.hold_spec['duration'] >= 0., 'Invalid authored hold duration'
        self.hold_positive = []
        self.hold_started = None
        self.stage('aim_interaction', dict(actor=e1._path(actor), identity=identity, duration=self.hold_spec['duration']))

    def aim_interaction(self, world, pc, pawn):
        spec = self.hold_spec
        actor, component = spec['actor'], spec['component']
        assert actor.get_world() == world and actor.interactable == component, 'Authored interaction identity changed'
        interaction = pc.get_interaction_component()
        look, angle = self.look(world, pc, actor.get_actor_location())
        admission = component.can_interact(pawn, interaction)
        focus = interaction.get_editor_property('viewed_interactable')
        self.report['last_interaction'] = dict(actor=e1._path(actor), focus=e1._path(focus),
            angle_error=angle, admission=str(admission), native_action_text=str(component.get_interactable_action_text(pawn, interaction)), admitted=_admitted(admission),
            distance=math.sqrt(_distance_squared(pawn.get_actor_location(), actor.get_actor_location())),
            error=e1._optional(lambda: str(actor.last_error)))
        self.inject(look=look)
        if angle < 3. and focus == component and _admitted(admission):
            self.hold_started = unreal.GameplayStatics.get_time_seconds(world)
            self.hold_last_world_time = self.hold_started
            self.stage('hold_interaction', dict(identity=spec['identity']))

    def hold_interaction(self, world, pc, state):
        spec = self.hold_spec
        actor = spec['actor']
        remaining = float(pc.get_interaction_component().get_editor_property('remaining_interact_time'))
        if 0. < remaining <= spec['duration']+.001:
            self.hold_positive.append(remaining)
        completed = (actor.is_disabled() if spec['identity'] in RECEIVERS
                     else any(str(e.beat_id) == spec['identity'] for e in state.get_journal()))
        if completed:
            self.inject()
            snapshot = self.settings()
            tap = bool(snapshot.tap_interactions)
            assert tap or spec['duration'] == 0. or self.hold_positive, 'Native receipt arrived but no positive hold countdown was sampled; duration proof remains unqualified'
            self.report['holds'].append(dict(identity=spec['identity'], actor=e1._path(actor),
                authored_seconds=spec['duration'], native_positive_countdown_samples=list(self.hold_positive),
                tap_accessibility=tap, began_world_seconds=self.hold_started,
                completed_world_seconds=unreal.GameplayStatics.get_time_seconds(world), journal=_events(state),
                receivers=self.receiver_snapshot()))
            self.stage(spec['then'])
            return
        assert time.monotonic()-self.phase_at < 14., 'Native interaction did not complete; inspect focus, action mapping, range, LastError and ownership'
        assert actor.interactable == spec['component'], 'Interaction component replaced during hold'
        self.inject(interact=1.)

    def begin_receiver(self, pc, pawn):
        if self.receiver_index >= len(RECEIVERS):
            self.stage('wait_final_receipt')
            return
        identity = RECEIVERS[self.receiver_index]
        actor = self.receivers[identity]
        assert not actor.is_disabled(), 'Receiver was disabled outside this recorded input sequence'
        position, forward = actor.get_actor_location(), actor.get_actor_forward_vector()
        approach = (position.x+forward.x*185., position.y+forward.y*185., position.z)
        self.begin_route([approach], 'aim_receiver')

    def trace_weapon_ray(self,world,pawn,target,point=None):
        camera=unreal.GameplayStatics.get_player_camera_manager(world,0)
        eye=camera.get_camera_location()
        if point is None:
            rotation=camera.get_camera_rotation()
            pitch,yaw=math.radians(rotation.pitch),math.radians(rotation.yaw)
            direction=(math.cos(pitch)*math.cos(yaw),math.cos(pitch)*math.sin(yaw),math.sin(pitch))
        else:
            delta=(point.x-eye.x,point.y-eye.y,point.z-eye.z)
            length=math.sqrt(sum(v*v for v in delta))
            assert length>1.
            direction=tuple(v/length for v in delta)
        # Exact CameraTowardsFocus line: project the pawn onto the camera ray.
        position=pawn.get_actor_location()
        projection=sum(v*d for v,d in zip((position.x-eye.x,position.y-eye.y,position.z-eye.z),direction))
        origin=unreal.Vector(*(v+projection*d for v,d in zip(e1._xyz(eye),direction)))
        end=unreal.Vector(*(v+15000.*d for v,d in zip(e1._xyz(origin),direction)))
        ignored=[pawn]
        for actor in list(pawn.get_all_child_actors())+list(pawn.get_attached_actors(reset_array=True,recursively_include_attached_actors=True)):
            if actor not in ignored:ignored.append(actor)
        hit=unreal.SystemLibrary.line_trace_single(world,origin,end,self.weapon_trace_type,True,
            ignored,unreal.DrawDebugTrace.NONE,True)
        parts=hit.to_tuple() if isinstance(hit,unreal.HitResult) else None
        if isinstance(hit,tuple):
            matches=[v for v in hit if isinstance(v,unreal.HitResult)]
            assert len(matches)==1
            parts=matches[0].to_tuple()
        actor=parts[9] if parts else None
        valid=bool(parts and parts[0] and (actor==target or (actor and actor.get_owner()==target)))
        row=dict(intended=e1._path(target),hit=e1._path(actor),blocking=bool(parts and parts[0]),
            actual_target_hit=valid,origin=e1._xyz(origin),direction=direction,
            impact=e1._xyz(parts[5]) if parts else None,bone=str(parts[11]) if parts else None,
            channel=str(self.weapon_trace_type),trace_complex=True,sweep_radius=0.,
            meaning='Conservative center ray; no target-data dispatch or damage application')
        return valid,row

    def aim_points(self,target):
        visual=target.get_character_visual()
        owners=[target]+([visual] if visual and visual!=target else [])
        points=[]
        for actor in owners:
            for mesh in actor.get_components_by_class(unreal.SkeletalMeshComponent):
                if not mesh.get_skeletal_mesh_asset():continue
                center,extent,radius=unreal.SystemLibrary.get_component_bounds(mesh)
                if radius<=1.:continue
                # Actual torso/head bones when present; drone body center is above
                # its actor origin and must come from the loaded mesh's bounds.
                if extent.z>extent.x:
                    for bone in ('spine_03','spine_02','head','Head'):
                        if mesh.does_socket_exist(unreal.Name(bone)):
                            points.append((mesh,bone,mesh.get_socket_location(unreal.Name(bone))))
                points.append((mesh,'loaded_mesh_bounds_center',center))
        assert points, 'A live participant has no initialized rendered skeletal geometry'
        return points

    def acquire_aim(self,world,pawn,candidates):
        ordered=sorted(candidates,key=lambda actor:(0 if actor==self.target else 1,
            actor.get_health(),_distance_squared(actor.get_actor_location(),pawn.get_actor_location())))
        fallback=None
        for target in ordered:
            for mesh,label,point in self.aim_points(target):
                clear,trace=self.trace_weapon_ray(world,pawn,target,point)
                row=dict(target=target,point=point,mesh=e1._path(mesh),label=label,clear=clear,trace=trace)
                if fallback is None:fallback=row
                if clear:return row
        return fallback

    def covered_movement(self,world,pc,pawn):
        center,extent=self.cover.get_actor_bounds(False)
        # Remain on the actual low cover's rear side, inside its lateral span.
        side=-1. if int(unreal.GameplayStatics.get_time_seconds(world)/1.8)%2 else 1.
        destination=(center.x+side*min(70.,extent.x-60.),center.y-extent.y-140.,pawn.get_actor_location().z)
        if self.path_target!=destination or time.monotonic()-self.last_path>.5:
            self.last_path=time.monotonic();self.path_target=destination
            path=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,
                pawn.get_actor_location(),unreal.Vector(*destination),pawn,None)
            valid=path is not None and path.is_valid() and not path.is_partial()
            self.path_points=list(path.path_points)[1:] if valid else []
            self.report['last_cover_path']=dict(destination=destination,complete=valid,
                points=[e1._xyz(v) for v in path.path_points] if path else [])
        while self.path_points:
            movement,reached=self.local_move(pc,pawn,e1._xyz(self.path_points[0]),stop=20.)
            if not reached:return movement
            self.path_points.pop(0)
        return (0.,0.)

    def combat(self, world, pc, pawn, weapon):
        status=self.e2.get_encounter_state()
        assert status not in (unreal.SovEncounterState.FAILED,unreal.SovEncounterState.RESTORING), 'E2 failed or was retried; no automatic restart was issued'
        assert self.e2.get_attempt_id().export_text()==self.attempt, 'E2 attempt changed'
        if status==unreal.SovEncounterState.SUCCEEDED:
            self.inject()
            assert self.e2.has_confirmed_victory(), 'Director lacks genuine confirmed defeats'
            self.report['combat_victory']=dict(attempt=self.attempt,roster=self.roster(),
                receivers=self.receiver_snapshot(),journal=_events(pc.get_campaign_state()))
            self.stage('begin_receiver');return
        assert status==unreal.SovEncounterState.ACTIVE, 'Unexpected E2 state'
        now=time.monotonic();seconds=unreal.GameplayStatics.get_time_seconds(world)
        position=pawn.get_actor_location();clip=weapon.get_ammo_in_clip();reserve=weapon.get_spare_ammo()
        control=self.fire_control
        pending=control['shot']
        if pending and seconds-pending['seconds']>=.3:
            consumed=max(0,pending['total_ammo']-clip-reserve)
            matched=[row for row in self.report['native_damage'][pending['receipt_index']:]
                if row['participant']==pending['target'] and row['target']==pending['actor']
                and row['health']+row['shield']+row['poise']>0.]
            if consumed or matched or seconds-pending['seconds']>=1.:
                self.report['shot_outcomes'].append(dict(request=pending,consumed=consumed,
                    native_transactions=[row['transaction'] for row in matched],seconds=seconds))
                control['misses']=0 if matched else control['misses']+consumed
                control['shot']=None
                if control['misses']>=2:
                    control['flanks']+=1
                    assert control['flanks']<=3, 'Three native-path flanks failed to produce damage; stop without wasting remaining ammunition'
                    control['flank_origin']=e1._xyz(position);control['settle_since']=None
                    control['misses']=0;self.path_target=None;self.path_points=[]
                    self.report['combat_tactics'].append(dict(kind='flank_after_two_consumed_rounds_without_damage',
                        position=e1._xyz(position),target=pending['target'],seconds=seconds))
        candidates=[p.character for p in self.e2.participants if p.required_for_victory
            and unreal.SystemLibrary.is_valid(p.character) and p.character.is_alive() and p.character.get_health()>0.
            and not p.character.get_editor_property('hidden')]
        if not candidates:self.inject();return
        selection=self.acquire_aim(world,pawn,candidates);target=selection['target']
        identity=str(self.e2.find_participant_id(target));target_path=e1._path(target)
        if target!=self.target:
            self.target=target;control['settle_since']=None;control['key']=None
            self.report['targets'].append(dict(elapsed=now-self.started,participant=identity,actor=target_path,health=target.get_health()))
        point=selection['point'];distance=math.hypot(point.x-position.x,point.y-position.y)
        look,angle=self.look(world,pc,point)
        # Track an ordinarily moving target promptly through the same look
        # action. A stationary-camera requirement would reject valid tracking.
        look=tuple(max(-.7,min(.7,value*2.5)) for value in look)
        clear=selection['clear'];actual_hit,actual_ray=self.trace_weapon_ray(world,pawn,target)
        in_range=distance<min(5500.,weapon.get_attack_range()*.8)
        forced=control['flank_origin'] is not None
        if forced and math.hypot(position.x-control['flank_origin'][0],position.y-control['flank_origin'][1])>=220.:
            control['flank_origin']=None;forced=False
        # Never reverse toward old cover when a flank first obtains a clear ray.
        movement=self.approach(world,pc,pawn,target) if forced or not clear or not in_range else (0.,0.)
        velocity=pawn.get_velocity();speed=math.sqrt(velocity.x**2+velocity.y**2+velocity.z**2)
        spread=float(weapon.get_weapon_spread())
        assert math.isfinite(spread) and spread>=0., 'Native current weapon spread is invalid'
        assert clip>0 or reserve>0, 'Actual Staccato ammunition exhausted during combat; no ammunition was granted'
        reloading=clip<=0
        reload_input=float(reloading and seconds%1.2<.15)
        key=(target_path,selection['mesh'],selection['label'])
        settled=(not reloading and not forced and not any(movement) and speed<5.
            and clear and actual_hit and in_range and angle<.75 and spread<=.05)
        if not settled or control['key']!=key:control['settle_since']=None
        control['key']=key
        if settled and control['settle_since'] is None:control['settle_since']=seconds
        stable=control['settle_since'] is not None and seconds-control['settle_since']>=.05
        if stable and control['shot'] is None and seconds>=self.next_shot_at:
            self.fire_until=seconds+.08;self.next_shot_at=seconds+.65
            control['shot']=dict(seconds=seconds,target=identity,actor=target_path,total_ammo=clip+reserve,
                clip=clip,reserve=reserve,receipt_index=len(self.report['native_damage']),position=e1._xyz(position))
            self.report['firing_requests'].append(dict(seconds=seconds,target=identity,clip=clip,reserve=reserve,
                point=e1._xyz(point),matcher=selection['label'],mesh=selection['mesh'],trace=actual_ray,
                speed=speed,weapon_spread=spread,stable_seconds=seconds-control['settle_since'],
                target_tracking_gain=2.5,minimum_continuous_actual_hit_seconds=.05,
                zero_move_and_look_during_pulse=True))
            control['settle_since']=None
        pulse=seconds<self.fire_until and not reloading
        self.report['last_combat']=dict(target=identity,distance=distance,angle_error=angle,clip=clip,reserve=reserve,
            sight=clear,in_range=in_range,attack=pulse,target_health=target.get_health(),mesh=selection['mesh'],matcher=selection['label'],
            aim_point=e1._xyz(point),desired_trace=selection['trace'],actual_ray=actual_ray,speed=speed,weapon_spread=spread,
            stable=stable,forced_flank=forced,missed_rounds_at_position=control['misses'])
        self.inject(move=(0.,0.) if pulse else movement,look=(0.,0.) if pulse else look,
            aim=0. if reloading else 1.,attack=float(pulse),reload=reload_input)
        signature=tuple((row['id'],row['health'],row['alive']) for row in self.roster())
        if signature!=self.last_health_signature:
            self.last_health_signature=signature;self.last_health_progress=now
        assert now-self.last_health_progress<35., 'No native target health progress; inspect actual weapon-channel hit diagnostics'

    def bind_damage_observers(self):
        def make_damage_observer(identity,target_path):
            # UE counts every positional parameter, including defaults. Capture
            # participant identity in a factory so the delegate has one argument.
            def damaged(result):
                if e1._path(result.source_actor)!=self.initial_pawn or e1._path(result.target_actor)!=target_path:return
                self.report['native_damage'].append(dict(participant=identity,target=target_path,
                    transaction=result.transaction_id.export_text(),health=result.applied_health_damage,
                    shield=result.applied_shield_damage,poise=result.applied_poise_damage,fatal=result.fatal,
                    hit_zone=str(result.hit_zone),elapsed=time.monotonic()-self.started))
            return damaged
        for participant in self.e2.participants:
            actor=participant.character
            identity=str(participant.participant_id)
            if not unreal.SystemLibrary.is_valid(actor) or not actor.is_alive():continue
            target_path=e1._path(actor)
            assert self.roster_paths.get(identity)==target_path, 'Damage observer target identity changed'
            damaged=make_damage_observer(identity,target_path)
            delegate=actor.get_narrative_ability_system_component().on_damage_resolved_as_target
            self.combat_observers.append((delegate,damaged));delegate.add_callable(damaged)

    def initialize(self, world, pc, pawn, state, events):
        assert [e['beat'] for e in events] == BEATS[:4], 'Requires exactly the completed first Selene handoff journal'
        native_events = list(state.get_journal())
        assert all(_valid_guid(e.event_id) for e in native_events) and len({e['id'] for e in events}) == 4
        handoff = native_events[-1]
        assert _valid_guid(handoff.handoff_request_id) and str(handoff.handoff_anchor_id) == 'M12_SeleneEntry'
        # Reflected GameplayTag wrappers do not implement native value equality.
        # Compare their exact serialized tag values, preserving the three-owner gate.
        assert handoff.handoff_to_protagonist.export_text() == pawn.get_protagonist_identity_tag().export_text() == state.get_active_protagonist().export_text()
        assert pawn.is_character_ready() and pawn.is_alive() and pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE
        self.world, self.initial_pawn, self.controller_path = world, e1._path(pawn), e1._path(pc)
        asc = pawn.get_narrative_ability_system_component()
        assert asc and asc.get_avatar_owner() == pawn, 'Actual ASC avatar does not own Selene'
        self.asc_path, self.ready_epoch = e1._path(asc), asc.get_character_ready_epoch()
        self.initial_events = events
        self.owner = self.get_input_owner(world)
        settings = self.settings()
        self.settings_snapshot = settings.export_text()
        assert not settings.toggle_aim, 'This bounded driver requires held-aim input; current toggle preference was not changed'
        directors = [d for d in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovEncounterDirector) if str(d.encounter_id) == ENCOUNTER]
        assert len(directors) == 1, 'Missing/ambiguous E2 director'
        self.e2 = directors[0]
        assert self.e2.get_encounter_state() == unreal.SovEncounterState.INACTIVE, 'E2 must be unstarted after the genuine handoff'
        roster = self.roster()
        assert {r['id'] for r in roster} == ROSTER and len(roster) == 6, 'E2 must contain exactly four enforcers and two existing drones'
        assert all(r['alive'] and r['health'] > 0. for r in roster), 'An actual E2 participant is not initialized/alive'
        self.roster_paths={row['id']:row['actor'] for row in roster}
        # Resolve the actual engine trace mapping by its metadata, as populated
        # by CollisionProfile. Visibility's absence of a hit is not a weapon hit.
        channel=unreal.ArsenalStatics.get_narrative_pro_settings().weapon_trace_channel
        display=str(channel.get_display_name())
        trace_types=[]
        for index in range(32):
            try:trace=unreal.TraceTypeQuery.cast(index)
            except (TypeError,ValueError):continue
            if str(trace.get_display_name())==display:trace_types.append(trace)
        assert len(trace_types)==1, 'Native weapon collision channel lacks an exact trace mapping: '+display
        self.weapon_trace_type=trace_types[0]
        self.report['weapon_trace_mapping']=dict(native_channel=str(channel),display_name=display,trace_type=str(self.weapon_trace_type),
            geometry='Complex, zero-radius center ray through actual current camera direction; native weapon remains sole damage owner')
        covers=[actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor)
            if actor.get_actor_label()=='Z04_LC_SouthApproach']
        assert len(covers)==1
        self.cover=covers[0]
        center,extent=self.cover.get_actor_bounds(False)
        assert extent.x>100. and 80.<center.z+extent.z<160.
        self.report['cover']=dict(actor=e1._path(self.cover),center=e1._xyz(center),extent=e1._xyz(extent),
            tactic='Ordinary path to rear of existing low cover; limited lateral movement while aiming')
        self.bind_damage_observers()
        objectives = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovCampaignEncounterObjective)
                      if a.encounter_director == self.e2 and str(a.completion_beat) == 'RelayOverlook']
        assert len(objectives) == 1
        self.objective = objectives[0]
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovCampaignRelayReceiver):
            identity = str(actor.receiver_id)
            if identity in RECEIVERS:
                assert identity not in self.receivers and actor.encounter_objective == self.objective
                self.receivers[identity] = actor
        assert set(self.receivers) == set(RECEIVERS) and {e1._path(a) for a in self.objective.required_receivers} == {e1._path(a) for a in self.receivers.values()}
        assert not any(actor.is_disabled() for actor in self.receivers.values()), 'Receivers already have unrecorded progress'
        inventory = pawn.get_inventory_component()
        self.report['inventory'] = [dict(item=e1._path(item), cls=e1._path(item.get_class()), quantity=item.get_quantity()) for item in inventory.get_items()]
        mappings = {name: [key.export_text() for key in self.owner.query_keys_mapped_to_action(action)] for name, action in self.actions.items()}
        assert all(mappings[name] for name in ('IA_Move', 'IA_Look', 'IA_Attack', 'IA_AltAttack', 'IA_Reload', 'IA_Interact', 'IA_WeaponWheel')), 'A required action lacks an effective local-player mapping'
        grenade = unreal.load_asset('/Game/Input/IA_Grenade')
        ability2 = unreal.load_asset(e1.ACTION_ROOT+'IA_Ability2')
        self.report['selene_echo_mapping'] = dict(grenade=e1._path(grenade), grenade_keys=[k.export_text() for k in self.owner.query_keys_mapped_to_action(grenade)] if grenade else [],
            ability2=e1._path(ability2), ability2_keys=[k.export_text() for k in self.owner.query_keys_mapped_to_action(ability2)] if ability2 else [], injected=False)
        self.report['initial'] = dict(world=e1._path(world), pawn=e1._path(pawn), controller=e1._path(pc),
            journal=events, roster=roster, receivers=self.receiver_snapshot(), settings=self.settings_snapshot,
            mappings=mappings)
        self.stage('await_staccato_selection', 'Select Staccato through the actual weapon wheel: 35 seconds held / 10 seconds released, up to 180 seconds')

    def tick(self, delta):
        if self.done:
            return
        try:
            now = time.monotonic()
            assert now-self.started < 900., 'E2 continuation deadline exceeded'
            assert now-self.phase_at < (420. if self.phase == 'combat' else 180. if self.phase == 'await_staccato_selection' else 120.), 'Stage deadline: '+self.phase
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world is not None and re.sub(r'UEDPIE_\d+_', '', e1._path(world).split('.')[0]) == '/Game/Aurelion/Maps/L_Aurelion_M12'
            assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), 'PIE ended'
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            assert isinstance(pc, unreal.SovPlayerController) and isinstance(pawn, unreal.SovSeleneCharacter)
            state = pc.get_campaign_state()
            assert state and state.get_active_mission() and str(state.get_active_mission().mission_id) == MISSION
            events = _events(state)
            assert [e['beat'] for e in events] == BEATS[:len(events)] and 4 <= len(events) <= 6
            assert all(e['mission'] == MISSION for e in events)
            if self.phase == 'initialize':
                self.initialize(world, pc, pawn, state, events)
            assert world == self.world and e1._path(pawn) == self.initial_pawn and e1._path(pc) == self.controller_path, 'Player/world ownership changed'
            asc = pawn.get_narrative_ability_system_component()
            assert asc and e1._path(asc) == self.asc_path and asc.get_avatar_owner() == pawn and asc.get_character_ready_epoch() == self.ready_epoch, 'Selene ASC/readiness ownership changed'
            assert state.get_active_protagonist().export_text() == pawn.get_protagonist_identity_tag().export_text()
            assert events[:4] == self.initial_events, 'Existing journal GUIDs/content changed'
            assert self.settings().export_text() == self.settings_snapshot, 'Local settings changed during input run'
            assert pawn.is_alive() and pawn.get_health() > 0., 'Selene died; no retry/heal/resurrection issued'
            if not self.owned_hud_gate('selene_handoff'):
                self.inject(); return
            if self.phase == 'await_staccato_selection':
                assert len(events) == 4 and pawn.is_character_ready(), 'Selection wait requires the unchanged four-beat handoff prefix and ready Selene'
            if now-self.last_sample > .5:
                self.last_sample = now
                self.report['samples'].append(dict(elapsed=now-self.started, phase=self.phase, position=e1._xyz(pawn.get_actor_location()),
                    health=pawn.get_health(), ready=pawn.is_character_ready(), encounter=str(self.e2.get_encounter_state()),
                    objective_error=str(self.objective.last_error), pre_entry_error=str(self.e2.pre_entry_hold_error),
                    roster=self.roster(), receivers=self.receiver_snapshot(), journal=events,
                    scanners=[dict(id=str(s.scanner_id), pending=s.has_pending_observation(), recipients=s.get_alerted_recipient_count())
                        for s in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionSweepScanner)]))
            if now-self.last_write > 1.:
                self.last_write = now
                self.write()
            # The wheel may pause the world; continue its hold/release cycle first.
            if self.phase == 'await_staccato_selection':
                assert state.is_state_valid() and pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE, 'Selection wait requires stable campaign ownership'
                self.await_weapon(pawn)
                return
            if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world):
                self.inject()
                return
            assert pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE, 'Unexpected transition during this same-pawn E2 continuation'
            if self.attempt:
                assert self.e2.get_attempt_id().export_text() == self.attempt, 'E2 attempt was replaced'
                assert self.e2.get_encounter_state() not in (unreal.SovEncounterState.FAILED, unreal.SovEncounterState.RESTORING)
            if self.phase == 'walk_route':
                self.walk_route(pc, pawn)
            elif self.phase == 'aim_arrival':
                found = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovCampaignInteractionTerminal) if str(a.terminal_id) == 'Aurelion_SeleneArrival']
                assert len(found) == 1
                self.prepare_hold(found[0], 'SeleneArrival', 'route_to_entry')
            elif self.phase == 'aim_interaction':
                self.aim_interaction(world, pc, pawn)
            elif self.phase == 'hold_interaction':
                self.hold_interaction(world, pc, state)
            elif self.phase == 'route_to_entry':
                assert [e['beat'] for e in events] == BEATS[:5]
                self.begin_route([(7000.,-12700.,100.)], 'wait_entry')
            elif self.phase == 'wait_entry':
                self.inject()
                assert self.e2.get_encounter_state() in (unreal.SovEncounterState.INACTIVE, unreal.SovEncounterState.ACTIVE)
                if self.e2.get_encounter_state() == unreal.SovEncounterState.ACTIVE:
                    assert _valid_guid(self.e2.get_attempt_id())
                    self.attempt = self.e2.get_attempt_id().export_text()
                    self.report['entry'] = dict(attempt=self.attempt, position=e1._xyz(pawn.get_actor_location()), roster=self.roster())
                    self.stage('combat')
            elif self.phase == 'combat':
                self.combat(world, pc, pawn, self.weapon(pawn))
            elif self.phase == 'begin_receiver':
                self.begin_receiver(pc, pawn)
            elif self.phase == 'aim_receiver':
                identity = RECEIVERS[self.receiver_index]
                self.prepare_hold(self.receivers[identity], identity, 'receiver_complete')
            elif self.phase == 'receiver_complete':
                self.receiver_index += 1
                self.stage('begin_receiver')
            elif self.phase == 'wait_final_receipt':
                self.inject()
                if len(events) == 6:
                    final = events[-1]
                    assert final['encounter'] == ENCOUNTER and final['attempt'] == self.attempt
                    assert final['receivers'] == sorted(RECEIVERS)
                    assert all(a.is_disabled() for a in self.receivers.values()) and self.e2.has_confirmed_victory()
                    assert {h['identity'] for h in self.report['holds']} == {'SeleneArrival', *RECEIVERS}
                    assert all(_valid_guid(e.event_id) for e in state.get_journal()) and len({e['id'] for e in events}) == 6
                    self.report['final'] = dict(journal=events, receivers=self.receiver_snapshot(), roster=self.roster(),
                        pawn=e1._path(pawn), health=pawn.get_health(), position=e1._xyz(pawn.get_actor_location()), attempt=self.attempt)
                    if not self.owned_hud_gate('e2_final_receipt'): return
                    self.finish(True, 'Ordinary Selene approach/combat and two native receiver interactions committed the exact E2 receipt. Later encounters and physical keyboard/rendered acceptance remain unqualified.')
        except Exception:
            self.report['error'] = traceback.format_exc()
            self.finish(False, self.report['error'])


def start(output_directory=None):
    global _RUN
    assert _RUN is None or _RUN.done, 'E2 continuation already running'
    assert e1._RUN is None or e1._RUN.done, 'Stop the E1 input driver first'
    output = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (output/'e2-input-continuation.json').exists(), 'Use a new output directory to retain prior evidence'
    _RUN = Run(output)
    _RUN.write()
    _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False, 'Stopped by operator; held input released')


if __name__ == '__main__':
    start()
