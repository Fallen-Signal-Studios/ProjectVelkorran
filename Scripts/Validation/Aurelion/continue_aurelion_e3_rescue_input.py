"""Retained M12 PIE: E3 initial wave -> rescue door -> victory -> two full scenes.

Only ordinary Enhanced Input changes gameplay. Read-only native state, traces and
navigation queries determine when to press/release. This does not equip, grant,
heal, retry, teleport, call a request owner, or write a progression receipt.
"""
import json
import math
import os
from pathlib import Path
import sys
import time
import traceback
import unreal
import continue_aurelion_e1_input as common
import continue_aurelion_e3_entry_input as entry

_RUN = None
MISSION = entry.MISSION
INITIAL = list(entry.ALLOWED)
BREACH, MARINE, LYRIC = 'BreachSharedJunction', 'FreeTrappedMarine', 'GroundLyric'
ALLOWED = INITIAL + [BREACH, MARINE, LYRIC]
INITIAL_HOSTILES = {'E3.Linkbound1', 'E3.Linkbound2', 'E3.Linkbound3', 'E3.WallRunner'}
RESERVED_HOSTILES = {'E3.Linkbound4', 'E3.Linkbound5', 'E3.Weaver'}
PROTECTED = {'E3.Protected.TrappedMarine', 'E3.Protected.Lyric'}
_path, _xyz, _optional = common._path, common._xyz, common._optional


def alive(actor):
    return bool(actor and unreal.SystemLibrary.is_valid(actor) and actor.is_alive() and actor.get_health() > 0.)


class Run(entry.Run):
    def __init__(self, output_directory):
        super().__init__(output_directory)
        self.door = self.coordination = None
        self.door_delegate = self.door_callback = None
        self.wave_delegate = self.wave_callback = None
        self.requests = {}
        self.scene_beat = None
        self.hold_seconds = 0.
        self.door_origin = None
        self.initial_actor_paths = {}
        self.last_combat_progress = self.started
        self.last_combat_signature = None
        self.wave_released = False
        self.report.update(scope='Actual E3 initial release through its rescue door, native victory, FreeTrappedMarine and GroundLyric',
            pending=['Every beat after GroundLyric', 'Physical keyboard validation', 'Rendered cinematic review',
                     'Separate ceiling-assist gameplay or finished rescue animation/art'],
            entry_requires=['unchanged eight-beat prefix', 'ready living Tarrik with actual Selene companion',
                            'same active E3 attempt with four shown / three reserved hostiles and two living protected actors',
                            'actual Cinderline already wielded with existing ammunition', 'unchanged standard hold settings'],
            door_states=[], waves=[], completed_scenes={}, scene_phases=[], dialogue_cues=[],
            request_results=[], route_paths=[], native_combat_victory=False)

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 3)
        temp = self.out/'e3-rescue-input-continuation.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        common.replace_report_with_retry(temp, self.out/'e3-rescue-input-continuation.json')

    def unbind_scene(self):
        if self.scene_delegate is not None and self.scene_callback is not None:
            self.report['remove_scene_observer'] = _optional(lambda: self.scene_delegate.remove_callable(self.scene_callback))
        self.scene_delegate = self.scene_callback = None

    def finish(self, passed, reason):
        if self.done:
            return
        self.unbind_scene()
        self.unbind_request()
        if self.door_delegate is not None and self.door_callback is not None:
            self.report['remove_door_observer'] = _optional(lambda: self.door_delegate.remove_callable(self.door_callback))
        if self.wave_delegate is not None and self.wave_callback is not None:
            self.report['remove_wave_observer'] = _optional(lambda: self.wave_delegate.remove_callable(self.wave_callback))
        self.door_delegate = self.door_callback = self.wave_delegate = self.wave_callback = None
        common.Run.finish(self, passed, reason)
        # A later root-owned route may travel. Finished observers must not pin its PIE world.
        for field in ('world', 'owner', 'initial_controller', 'companion', 'e1', 'e2', 'e3', 'entry',
                      'door', 'coordination', 'scene', 'scene_component', 'hold_actor', 'target',
                      'meeting_request', 'handoff_request', 'path_target'):
            setattr(self, field, None)
        self.requests.clear()
        self.path_points.clear()
        unreal.log('Aurelion E3 rescue continuation: '+self.report['status']+': '+reason)

    def roster(self):
        result = []
        for participant in self.e3.participants:
            actor = participant.character
            valid = actor is not None and unreal.SystemLibrary.is_valid(actor)
            result.append(dict(id=str(participant.participant_id), actor=_path(actor) if valid else None,
                alive=alive(actor), health=actor.get_health() if valid else None,
                hidden=bool(actor.get_editor_property('hidden')) if valid else None,
                position=_xyz(actor.get_actor_location()) if valid else None,
                required=participant.required_for_victory))
        return result

    def door_state(self):
        return dict(state=str(self.door.get_transit_state()),
            location=_xyz(self.door.get_actor_location()), body=_xyz(self.door.moving_body.get_world_location()),
            body_relative=_xyz(self.door.moving_body.get_editor_property('relative_location')),
            entry_center=_xyz(self.door.entry_bounds.get_world_location()),
            entry_extent=_xyz(self.door.entry_bounds.get_scaled_box_extent()),
            health=self.door.structural_health, powered=self.door.powered, lock=str(self.door.lock_reason),
            wave=self.coordination.get_current_wave())

    def check_ownership(self, pc, pawn):
        assert pc == self.initial_controller and _path(pawn) == self.initial_pawn, 'Unexpected controller/pawn replacement'
        assert isinstance(pawn, unreal.SovTarrikCharacter) and alive(pawn), 'Tarrik died or changed; no retry/heal was issued'
        assert self.e3.get_attempt_id().export_text() == self.attempt, 'Encounter attempt changed'
        assert self.e3.get_encounter_state() not in (unreal.SovEncounterState.INACTIVE,
            unreal.SovEncounterState.FAILED, unreal.SovEncounterState.RESTORING), 'E3 retired, failed or retried'
        assert alive(self.companion) and self.companion.get_owner() == pc, 'Actual Selene companion died or changed ownership'
        assert not self.companion.get_editor_property('hidden'), 'Actual Selene companion became hidden'
        assert not self.companion.get_companion_component().is_disabled(), 'Actual Selene companion became disabled'
        rows = self.roster()
        assert {p['id'] for p in rows} == INITIAL_HOSTILES | RESERVED_HOSTILES | PROTECTED, 'E3 roster identities changed'
        protected = [p for p in rows if p['id'] in PROTECTED]
        assert all(p['alive'] and not p['required'] and p['actor'] == self.initial_actor_paths[p['id']]
                   for p in protected), 'A protected survivor died, retired or changed identity'
        for p in rows:
            if p['actor'] is not None:
                assert p['actor'] == self.initial_actor_paths[p['id']], 'An E3 actor was replaced inside the current attempt'
        assert self.door.get_transit_state() not in (unreal.SovWorldTransitState.BLOCKED,
            unreal.SovWorldTransitState.BROKEN), 'Rescue door blocked or broken; no endpoint was forced'
        assert self.door.structural_health > 0., 'Rescue door was destroyed'
        return rows

    def initialize(self, world, pc, pawn, state, events):
        assert [e['beat'] for e in events] == INITIAL, 'Start exactly after the genuine shared handoff and initial E3 release'
        assert isinstance(pawn, unreal.SovTarrikCharacter) and pawn.is_character_ready() and alive(pawn)
        assert pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE
        self.world, self.initial_pawn, self.initial_controller = world, _path(pawn), pc
        self.owner = self.get_input_owner(world)
        snapshot = unreal.GameUserSettings.get_game_user_settings().get_settings_snapshot()
        assert not snapshot.tap_interactions and abs(snapshot.interaction_hold_scale-1.) < .001, 'Requires existing standard holds; no settings change is issued'
        self.initial_events = events
        self.e3 = self.unique(unreal.SovEncounterDirector, 'encounter_id', 'M12_E3_SharedBreach')
        self.entry = self.unique(unreal.SovCampaignEncounterObjective, 'completion_beat', BREACH)
        self.coordination = self.e3.get_coordination_component()
        assert self.coordination and self.coordination.get_current_wave() == 0
        assert self.e3.get_encounter_state() == unreal.SovEncounterState.ACTIVE and self.e3.has_encounter_player(pawn)
        assert not self.e3.has_confirmed_victory() and not self.entry.is_result_pending()
        self.attempt = self.e3.get_attempt_id().export_text()
        assert entry.valid_guid(self.attempt)
        self.companion = self.companion_state(pc, pawn, True)
        assert self.companion is not None
        assert entry.valid_guid(events[-1]['handoff']) and events[-1]['anchor'] == 'M12_TarrikSharedBreach'
        meeting = next(e for e in events if e['beat'] == entry.MEETING)
        assert entry.valid_guid(meeting['cinematic']) and not meeting['skipped']
        rows = self.roster()
        assert len(rows) == 9 and all(p['alive'] for p in rows)
        assert {p['id'] for p in rows if p['required'] and not p['hidden']} == INITIAL_HOSTILES
        assert {p['id'] for p in rows if p['required'] and p['hidden']} == RESERVED_HOSTILES
        assert {p['id'] for p in rows if not p['required']} == PROTECTED
        self.initial_actor_paths = {p['id']:p['actor'] for p in rows}
        self.door = self.unique(unreal.SovWorldTransitActor, 'transit_id', 'M12_E3_RescueApproach')
        assert self.door.kind == unreal.SovWorldTransitKind.DOOR
        assert self.door.get_transit_state() == unreal.SovWorldTransitState.AT_ORIGIN
        assert str(self.door.required_mission) == MISSION and not self.door.irreversible_transition
        assert abs(float(self.door.interactable.interaction_time)-.25) < .001, 'Rescue door should use its actual native 0.25-second hold'
        assert abs(self.door.travel_seconds-1.5) < .001 and math.dist(_xyz(self.door.destination_offset),[0.,0.,330.]) < .01
        self.door_origin = _xyz(self.door.moving_body.get_editor_property('relative_location'))
        rules = list(self.coordination.wave_release_rules)
        assert len(rules) == 1 and rules[0].wave == 1 and rules[0].transit_door == self.door
        assert rules[0].condition == unreal.SovEncounterWaveCondition.TRANSIT_DOOR_OPEN and rules[0].maximum_living_released_hostiles == 3
        for beat in (MARINE, LYRIC):
            actor = self.unique(unreal.SovAurelionRequestActor, 'beat_id', beat)
            assert actor.operation == unreal.SovAurelionRequest.PLAY_SCENE and actor.story is not None
            component = actor.story.campaign_cinematic
            assert str(component.mission_id) == MISSION and str(component.beat_id) == beat
            assert component.get_phase() == unreal.SovCinematicPhase.IDLE
            assert abs(float(actor.interactable.interaction_time)-.35) < .001
            self.requests[beat] = actor
        def transit_changed(transit_state, message):
            self.report['door_states'].append(dict(state=str(transit_state), message=str(message),
                elapsed=time.monotonic()-self.started, wave=self.coordination.get_current_wave()))
        self.door_callback, self.door_delegate = transit_changed, self.door.on_transit_changed
        self.door_delegate.add_callable(self.door_callback)
        def wave_changed(wave):
            self.report['waves'].append(dict(wave=wave, elapsed=time.monotonic()-self.started,
                door=str(self.door.get_transit_state()), roster=self.roster()))
        self.wave_callback, self.wave_delegate = wave_changed, self.coordination.on_wave_changed
        self.wave_delegate.add_callable(self.wave_callback)
        weapon = self.weapon(pawn)
        self.report['settings'] = snapshot.export_text()
        self.report['initial'] = dict(world=_path(world), pawn=_path(pawn), companion=_path(self.companion),
            attempt=self.attempt, journal=events, roster=rows, weapon=_path(weapon),
            clip=weapon.get_ammo_in_clip(), reserve=weapon.get_spare_ammo(), door=self.door_state(),
            scenes={beat:dict(request=_path(a), request_position=_xyz(a.get_actor_location()),
                station=_xyz(a.story.get_actor_location()), scene=_path(a.story),
                hold=float(a.interactable.interaction_time)) for beat,a in self.requests.items()},
            mappings={n:[k.export_text() for k in self.owner.query_keys_mapped_to_action(a)] for n,a in self.actions.items()})
        self.stage('combat_initial')

    def combat(self, pc, pawn, rows):
        first_wave = self.phase == 'combat_initial'
        current_wave = self.coordination.get_current_wave()
        if first_wave:
            assert current_wave == 0 and self.door.get_transit_state() == unreal.SovWorldTransitState.AT_ORIGIN
            assert all(p['alive'] and p['hidden'] for p in rows if p['id'] in RESERVED_HOSTILES), 'Reserved E3 wave changed before the physical door opened'
            if not any(p['alive'] for p in rows if p['id'] in INITIAL_HOSTILES):
                self.inject()
                self.report['initial_wave_cleared'] = dict(attempt=self.attempt, roster=rows,
                    journal=entry.journal(pc.get_campaign_state()), native_victory=self.e3.has_confirmed_victory())
                assert not self.e3.has_confirmed_victory(), 'Reserved hostiles must prevent premature victory'
                self.begin_route([(1100.,8160.,-510.),(1100.,8320.,-510.)], 'aim_door')
                return
        else:
            assert self.wave_released and current_wave == 1
            assert self.door.get_transit_state() == unreal.SovWorldTransitState.AT_DESTINATION
            if self.e3.get_encounter_state() == unreal.SovEncounterState.SUCCEEDED:
                self.inject()
                assert self.e3.has_confirmed_victory(), 'Director success lacks actual defeat and survivor proof'
                self.stage('wait_breach_receipt')
                return
        assert self.e3.get_encounter_state() == unreal.SovEncounterState.ACTIVE
        candidates = [p.character for p in self.e3.participants if p.required_for_victory and alive(p.character)
                      and not p.character.get_editor_property('hidden')]
        if not candidates:
            self.inject()
            return
        weapon = self.weapon(pawn)
        position = pawn.get_actor_location()
        def ordering(actor):
            # The released Weaver supports the two later Linkbound. Shoot its actual body first.
            return (str(self.e3.find_participant_id(actor)) != 'E3.Weaver',
                    math.dist(_xyz(position), _xyz(actor.get_actor_location())))
        target = min(candidates, key=ordering)
        if self.target != target:
            self.target = target
            self.report['targets'].append(dict(elapsed=time.monotonic()-self.started,
                participant=str(self.e3.find_participant_id(target)), actor=_path(target), health=target.get_health()))
        location = target.get_actor_location()
        distance = math.hypot(location.x-position.x, location.y-position.y)
        look, error = self.look(self.world, pc, location)
        clear = self.clear_sight(self.world, pawn, target)
        in_range = distance < min(2400., max(500., weapon.get_attack_range()*.8))
        movement = self.approach(self.world, pc, pawn, target) if (not clear or not in_range) and distance > 450. else (0.,0.)
        clip, reserve = weapon.get_ammo_in_clip(), weapon.get_spare_ammo()
        # The same bounded ordinary pickup approach used in E1. Enemy-authored
        # native drops must actually exist, match Cinderline and have a full path.
        pickup_move = self.ammo_movement(self.world, pc, pawn, weapon)
        assert clip > 0 or reserve > 0 or pickup_move is not None, 'Existing Cinderline ammunition exhausted with no reachable matching pickup; no grant/equip/resource reset was issued'
        if pickup_move is not None:
            movement = pickup_move
        seconds = unreal.GameplayStatics.get_time_seconds(self.world)
        reloading = clip <= 0
        reload_input = 1. if reloading and seconds % 1.2 < .15 else 0.
        attack = 1. if not reloading and clear and in_range and error < 1.5 and seconds % .6 < .4 else 0.
        signature = tuple((r['id'],round(r['health'] or 0.,1),r['alive']) for r in rows if r['required'])
        if signature != self.last_combat_signature:
            self.last_combat_signature, self.last_combat_progress = signature, time.monotonic()
        assert time.monotonic()-self.last_combat_progress < 75., 'Combat made no measured hostile-health/defeat progress for 75 seconds; inspect last trace/path and native defenses'
        self.report['last_combat'] = dict(target=str(self.e3.find_participant_id(target)), distance=distance,
            angle_error=error, clip=clip, reserve=reserve, visible_line=clear, in_range=in_range,
            primary_pressed=bool(attack), target_health=target.get_health(), wave=current_wave,
            seeking_ammo=pickup_move is not None)
        self.inject(move=movement, look=look, aim=0. if reloading else 1., attack=attack, reload=reload_input)

    def select_scene(self, beat):
        self.unbind_scene()
        self.unbind_request()
        self.scene_beat = beat
        self.scene = self.requests[beat].story
        self.scene_component = self.scene.campaign_cinematic
        self.cues_seen = set()
        self.last_scene_phase = None
        assert self.scene_component.get_phase() == unreal.SovCinematicPhase.IDLE
        def phase_changed(phase, reason):
            self.report['scene_phases'].append(dict(beat=beat, phase=str(phase), reason=str(reason),
                elapsed=time.monotonic()-self.started, observed='native_delegate'))
        self.scene_callback, self.scene_delegate = phase_changed, self.scene_component.on_phase_changed
        self.scene_delegate.add_callable(self.scene_callback)
        station = self.scene.get_actor_location()
        # Navigation queries choose real movement; these points are authored walking marks, not teleports.
        approach = (station.x+40., station.y-70., station.z)
        points = [(1100.,8650.,-510.),approach] if beat == MARINE else [(900.,9200.,station.z),approach]
        self.begin_route(points, 'aim_scene')

    def observe_scene(self):
        phase = self.scene_component.get_phase()
        if phase != self.last_scene_phase:
            self.last_scene_phase = phase
            self.report['scene_phases'].append(dict(beat=self.scene_beat, phase=str(phase),
                elapsed=time.monotonic()-self.started, observed='poll'))
        index = self.scene.get_current_dialogue_cue_index()
        if index >= 0 and index not in self.cues_seen:
            self.cues_seen.add(index)
            cue = self.scene.dialogue_cues[index]
            self.report['dialogue_cues'].append(dict(beat=self.scene_beat, index=index,
                speaker=str(cue.speaker), text=str(cue.text), elapsed=time.monotonic()-self.started))
        if phase == unreal.SovCinematicPhase.FAILED:
            self.scene_failed(phase)
        return phase

    def scene_failed(self, phase):
        assert phase != unreal.SovCinematicPhase.FAILED, 'Native scene failed: '+json.dumps(self.report['scene_phases'][-6:])

    def aim_use(self, pc, pawn, actor, kind):
        interaction, component = pc.get_interaction_component(), actor.interactable
        look, error = self.look(self.world, pc, actor.get_actor_location())
        admission = component.can_interact(pawn, interaction)
        focus = interaction.get_editor_property('viewed_interactable')
        self.report['last_interaction'] = dict(actor=_path(actor), kind=kind, focus=_path(focus),
            admitted=admission is not None, admission=str(admission), native_action_text=str(component.get_interactable_action_text(pawn, interaction)), angle_error=error,
            player=_xyz(pawn.get_actor_location()), location=_xyz(actor.get_actor_location()),
            distance=math.dist(_xyz(pawn.get_actor_location()),_xyz(actor.get_actor_location())),
            native_interaction_range=float(component.interaction_distance),
            native_hold=float(component.interaction_time),
            last_result=str(actor.last_result) if kind == 'scene' else self.door_state())
        self.inject(look=look)
        if kind == 'scene':
            retry = self.report.setdefault('focus_repositions', {}).setdefault(_path(actor),
                dict(attempts=0, blocked_since=None, routes=[]))
            focus_owner = focus.get_owner() if focus is not None else None
            npc_blocked = admission is not None and error < 1. and focus != component and isinstance(focus_owner, unreal.NarrativeNPCCharacter)
            if npc_blocked and retry['attempts'] < 2:
                now = time.monotonic()
                if retry['blocked_since'] is None:
                    retry['blocked_since'] = now
                if now-retry['blocked_since'] >= 2.:
                    point, position = actor.get_actor_location(), pawn.get_actor_location()
                    points = ([(point.x-130., point.y-180., position.z), (point.x, point.y-160., position.z)]
                        if retry['attempts'] == 0 else [(point.x+180., point.y-130., position.z), (point.x+160., point.y, position.z)])
                    retry['attempts'] += 1
                    retry['blocked_since'] = None
                    retry['routes'].append(dict(focus=_path(focus), npc=_path(focus_owner),
                        points=points, elapsed=now-self.started, reason='Native request admitted but nearby NPC owns interaction focus'))
                    self.inject()
                    self.begin_route(points, 'aim_scene')
                    return
            else:
                retry['blocked_since'] = None
        if error >= 3. or admission is None or focus != component:
            return
        self.unbind_request()
        self.hold_actor, self.hold_beat = actor, self.scene_beat if kind == 'scene' else 'RescueDoor'
        self.hold_seconds = float(component.interaction_time)
        self.hold_started = unreal.GameplayStatics.get_time_seconds(self.world)
        self.saw_countdown, self.request_result = False, None
        if kind == 'scene':
            beat = self.scene_beat
            def result(accepted, message):
                row = dict(beat=beat, accepted=accepted, message=str(message), elapsed=time.monotonic()-self.started)
                self.report['request_results'].append(row)
                self.request_result = row
            self.request_callback, self.request_delegate = result, actor.on_request_result
            self.request_delegate.add_callable(self.request_callback)
        self.stage('hold_door' if kind == 'door' else 'hold_scene')

    def hold_use(self, pc):
        remaining = float(pc.get_interaction_component().get_editor_property('remaining_interact_time'))
        if 0. < remaining <= self.hold_seconds+.001:
            self.saw_countdown = True
        is_door = self.phase == 'hold_door'
        accepted = (self.door.get_transit_state() != unreal.SovWorldTransitState.AT_ORIGIN if is_door else
                    self.hold_actor.is_request_pending() or self.request_result is not None
                    or self.scene_component.get_phase() != unreal.SovCinematicPhase.IDLE)
        if accepted:
            self.inject()
            assert self.saw_countdown, 'Native ordinary hold countdown was not observed'
            if self.request_result is not None:
                assert self.request_result['accepted'], 'Scene rejected: '+self.request_result['message']
            self.report['holds'].append(dict(beat=self.hold_beat, seconds=self.hold_seconds, native_countdown=True,
                input_game_seconds=unreal.GameplayStatics.get_time_seconds(self.world)-self.hold_started,
                actor=_path(self.hold_actor)))
            self.stage('wait_door' if is_door else 'wait_scene')
        else:
            assert time.monotonic()-self.phase_at < 8., 'Ordinary hold failed: '+json.dumps(self.report['last_interaction'])
            self.inject(interact=1.)

    def finish_scene(self, events):
        if self.scene_component.get_phase() != unreal.SovCinematicPhase.COMPLETED:
            return
        assert events[-1]['beat'] == self.scene_beat
        assert len(events) == ALLOWED.index(self.scene_beat)+1
        receipt = events[-1]
        assert entry.valid_guid(receipt['cinematic']) and not receipt['skipped'], 'Full scene needs its own non-skipped native cinematic receipt'
        assert self.cues_seen == set(range(len(self.scene.dialogue_cues))), 'Entire readable native dialogue timeline was not observed'
        self.report['completed_scenes'][self.scene_beat] = dict(receipt=receipt, cue_count=len(self.cues_seen),
            completed=True, skipped=False, protected=[r for r in self.roster() if not r['required']])
        if self.scene_beat == MARINE:
            self.select_scene(LYRIC)
        else:
            self.inject()
            self.stage('wait_grounding_gate')

    def tick(self, delta):
        if self.done:
            return
        try:
            now = time.monotonic()
            assert now-self.started < 1200., 'E3 continuation exceeded its twenty-minute bound'
            limit = 420. if self.phase.startswith('combat_') else (180. if self.phase == 'walk_route' else 100.)
            assert now-self.phase_at < limit, 'Stage deadline: '+self.phase
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world and '/Aurelion/Maps/UEDPIE_' in world.get_path_name() and 'L_Aurelion_M12' in world.get_name(), 'Requires existing M12 PIE'
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            assert isinstance(pc, unreal.SovPlayerController) and isinstance(pawn, unreal.SovTarrikCharacter)
            state = pc.get_campaign_state()
            assert state and state.get_active_mission() and str(state.get_active_mission().mission_id) == MISSION
            events = entry.journal(state)
            assert len(events) <= len(ALLOWED) and [e['beat'] for e in events] == ALLOWED[:len(events)], 'Unexpected journal evolution'
            assert all(e['mission'] == MISSION for e in events)
            if self.phase == 'initialize':
                self.initialize(world, pc, pawn, state, events)
            assert world == self.world and events[:len(self.initial_events)] == self.initial_events, 'World or original receipt identities changed'
            rows = self.check_ownership(pc, pawn)
            if self.scene_component is not None:
                self.observe_scene()
            if self.request_result is not None:
                assert self.request_result['accepted'], 'Native request rejected: '+self.request_result['message']
            if now-self.last_sample > .5:
                self.last_sample = now
                self.report['samples'].append(dict(elapsed=now-self.started, phase=self.phase, pawn=_path(pawn),
                    position=_xyz(pawn.get_actor_location()), health=pawn.get_health(), ready=pawn.is_character_ready(),
                    transition=str(pc.get_campaign_transition_state()), journal=events, encounter=str(self.e3.get_encounter_state()),
                    roster=rows, door=self.door_state(), scene=str(self.scene_component.get_phase()) if self.scene_component else None))
            if now-self.last_write > 1.:
                self.last_write = now
                self.write()
            assert pc.get_campaign_transition_state() != unreal.SovCampaignTransitionState.FAILED
            if self.phase in ('hold_door', 'hold_scene'):
                self.hold_use(pc)
                return
            if self.phase == 'wait_scene':
                self.inject()
                self.finish_scene(events)
                return
            if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world):
                self.inject()
                return
            if pc.get_campaign_transition_state() != unreal.SovCampaignTransitionState.IDLE:
                self.inject()
                return
            if self.phase.startswith('combat_'):
                self.combat(pc, pawn, rows)
            elif self.phase == 'walk_route':
                self.walk(pc, pawn)
            elif self.phase == 'aim_door':
                self.aim_use(pc, pawn, self.door, 'door')
            elif self.phase == 'wait_door':
                self.inject()
                if self.door.get_transit_state() == unreal.SovWorldTransitState.AT_DESTINATION:
                    assert math.dist(_xyz(self.door.moving_body.get_editor_property('relative_location')),_xyz(self.door.destination_offset)) < .2
                    if self.coordination.get_current_wave() == 1:
                        assert all(p['alive'] and not p['hidden'] for p in rows if p['id'] in RESERVED_HOSTILES), 'Native reinforcement release did not expose all three living actors'
                        self.wave_released = True
                        self.report['door_and_wave'] = dict(attempt=self.attempt, door=self.door_state(), roster=rows,
                            native_wave=1, release_observed=True, initial_hostiles_remaining=sum(p['alive'] for p in rows if p['id'] in INITIAL_HOSTILES))
                        self.last_combat_progress, self.last_combat_signature = now, None
                        self.stage('combat_reinforcements')
            elif self.phase == 'wait_breach_receipt':
                self.inject()
                if state.is_beat_complete(unreal.Name(MISSION), unreal.Name(BREACH)) and not self.entry.is_result_pending():
                    assert len(events) == 9 and events[-1]['beat'] == BREACH
                    assert events[-1]['encounter'] == 'M12_E3_SharedBreach' and events[-1]['attempt'] == self.attempt
                    assert self.e3.has_confirmed_victory() and self.wave_released
                    self.report['native_combat_victory'] = True
                    self.report['breach'] = dict(receipt=events[-1], roster=rows, door=self.door_state())
                    self.select_scene(MARINE)
            elif self.phase == 'aim_scene':
                self.aim_use(pc, pawn, self.requests[self.scene_beat], 'scene')
            elif self.phase == 'wait_grounding_gate':
                self.inject()
                gate = self.unique(unreal.SovAurelionJournalGate, 'beat_id', LYRIC)
                if not gate.is_blocking_route():
                    assert [e['beat'] for e in events] == ALLOWED and self.e3.has_confirmed_victory()
                    self.report['final'] = dict(journal=events, pawn=_path(pawn), ready=pawn.is_character_ready(),
                        position=_xyz(pawn.get_actor_location()), health=pawn.get_health(),
                        companion=_path(self.companion), roster=rows, grounding_gate_open=True)
                    self.finish(True, 'Ordinary Cinderline combat and the real rescue-door hold earned native E3 victory; full FreeTrappedMarine and GroundLyric playback committed genuine receipts with both protected actors alive. Later beats remain unqualified.')
        except Exception:
            self.report['error'] = traceback.format_exc()
            self.finish(False, self.report['error'])


def start(output_directory=None):
    """Start only in retained E3 initial-entry PIE, with existing Cinderline and normal holds."""
    global _RUN
    assert _RUN is None or _RUN.done, 'E3 rescue input driver is already running'
    for name in ('continue_aurelion_e1_input','continue_aurelion_e2_input','continue_aurelion_e3_entry_input'):
        module = sys.modules.get(name)
        run = getattr(module, '_RUN', None) if module else None
        assert run is None or run.done, 'Stop the other input driver first: '+name
    output = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (output/'e3-rescue-input-continuation.json').exists(), 'Use a new evidence directory'
    _RUN = Run(output)
    _RUN.write()
    _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    """Release every injected action and remove only this observer's delegates/tick callback."""
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False, 'Stopped by operator; no retry or state repair was issued')


if __name__ == '__main__':
    start()
