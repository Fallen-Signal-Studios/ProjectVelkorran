"""Existing-PIE continuation: earned E2 -> Meeting -> shared handoff -> E3 entry.

Import beside continue_aurelion_e1_input.py; stop other input drivers; call start()
with a new evidence directory. This deliberately stops at E3's INITIAL release.
It does not qualify its rescue door, combat, ceiling assist, marine rescue or any
later beat. No gameplay mutation is issued except ordinary Enhanced Input.
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
from aurelion_route_arrival import reached_projected_destination
import continue_aurelion_e1_input as common
from probe_aurelion_carrier_clearance_readonly import inspect as inspect_carrier_clearance

_RUN = None
MISSION = 'M12_FireAndFrost'
INITIAL = ['TarrikArrival', 'PressureHall', 'SecureTarrikRoute', 'HandoffToSelene',
           'SeleneArrival', 'RelayOverlook']
MEETING = 'MeetingAndCarrierRescue'
HANDOFF = 'HandoffToTarrikRescue'
ALLOWED = INITIAL + [MEETING, HANDOFF]
RECEIVERS = {'M12_E2_ReceiverWest', 'M12_E2_ReceiverEast'}
_path, _xyz, _optional = common._path, common._xyz, common._optional


def journal(state):
    return [dict(mission=str(e.mission_id), beat=str(e.beat_id),
                 id=e.event_id.export_text(), sequence=e.sequence,
                 cinematic=e.cinematic_session_id.export_text(), skipped=e.presentation_skipped,
                 handoff=e.handoff_request_id.export_text(), anchor=str(e.handoff_anchor_id),
                 encounter=str(e.encounter_id), attempt=e.encounter_attempt_id.export_text(),
                 receivers=sorted(str(v) for v in e.disabled_receiver_ids))
            for e in state.get_journal()]


def valid_guid(value):
    # FGuid::ExportTextItem uses EGuidFormats::Digits, exactly 32 hexadecimal digits.
    return bool(re.fullmatch(r'[0-9A-Fa-f]{32}', value)) and int(value, 16) != 0


class Run(common.Run):
    def __init__(self, output_directory):
        super().__init__(output_directory)
        self.scene = self.scene_component = self.e2 = self.e3 = self.entry = None
        self.meeting_request = self.handoff_request = None
        self.scene_callback = self.request_callback = None
        self.scene_delegate = self.request_delegate = None
        self.request_result = None
        self.companion = None
        self.next_route_state = None
        self.initial_controller = None
        self.held_game_seconds = 0.
        self.release_at = 0.
        self.last_scene_phase = None
        self.cues_seen = set()
        self.carrier_probe_at = 0.
        self.report.update(scope='Earned E2 receipt through full Meeting, native shared handoff and E3 initial entry only',
            pending=['E3 rescue door', 'E3 combat and reinforcement release', 'FreeTrappedMarine',
                     'shared ceiling assist and every later route beat'],
            scene_phases=[], dialogue_cues=[], request_results=[], route_paths=[],
            entry_requires=['real capsule overlap', 'native Active attempt', 'four released / three reserved hostiles',
                            'two living protected actors', 'no E3 victory receipt'])

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 3)
        temp = self.out/'e3-entry-input-continuation.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        common.replace_report_with_retry(temp, self.out/'e3-entry-input-continuation.json')

    def finish(self, passed, reason):
        if self.done:
            return
        # Remove only listeners installed by this observer, never gameplay listeners.
        if self.scene_callback and self.scene_delegate:
            self.report['remove_scene_observer'] = _optional(
                lambda: self.scene_delegate.remove_callable(self.scene_callback))
            self.scene_callback = None
            self.scene_delegate = None
        self.unbind_request()
        super().finish(passed, reason)
        unreal.log('Aurelion E3 entry continuation: '+self.report['status']+': '+reason)

    def release_world_references(self):
        super().release_world_references()
        self.scene = self.scene_component = self.e2 = self.e3 = self.entry = None
        self.meeting_request = self.handoff_request = self.companion = None
        self.initial_controller = None

    def unbind_request(self):
        if self.request_callback and self.request_delegate:
            self.report['remove_request_observer'] = _optional(
                lambda: self.request_delegate.remove_callable(self.request_callback))
        self.request_callback = None
        self.request_delegate = None

    def unique(self, cls, field, value):
        matches = [a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, cls)
                   if str(a.get_editor_property(field)) == value]
        assert len(matches) == 1, 'Requires one actual '+field+'='+value
        return matches[0]

    def companion_state(self, pc, pawn, required):
        # Active and Leader are private/non-editor properties. Observe their public
        # contract after the committed native handoff instead of bypassing reflection.
        candidates = [a for a in unreal.GameplayStatics.get_all_actors_of_class(
            self.world, unreal.SovProtagonistCompanionCharacter) if a.get_owner() == pc
            and a.is_alive() and not a.get_editor_property('hidden')]
        if not required:
            assert not candidates, 'Owned visible companion exists before the canonical shared handoff'
            return None
        if not candidates:
            return None
        assert len(candidates) == 1, 'Multiple controller-owned protagonist companions'
        active = candidates[0]
        component = active.get_companion_component()
        assert component is not None
        assert active.get_owner() == pc, 'Active companion belongs to another controller'
        assert 'Selene' in active.get_companion_identity().export_text(), 'Shared companion must be actual Selene'
        assert active.is_alive() and active.get_health() > 0., 'Shared companion is not alive'
        assert not component.is_disabled(), 'Shared companion is disabled'
        # Pure admission checks actual leader equality, ASC avatar and mission permission.
        admission = component.can_request_command(pawn, unreal.SovCompanionCommand.REGROUP, pawn)
        assert admission is not None, 'Shared companion fails public native command/leader admission'
        return active

    def check_relay(self, events):
        assert self.e2.get_encounter_state() == unreal.SovEncounterState.SUCCEEDED
        assert self.e2.has_confirmed_victory(), 'E2 has no native confirmed victory'
        relay = next(e for e in events if e['beat'] == 'RelayOverlook')
        assert relay['encounter'] == 'M12_E2_RelayOverlook'
        assert relay['attempt'] == self.e2.get_attempt_id().export_text() and valid_guid(relay['attempt'])
        assert set(relay['receivers']) == RECEIVERS, 'Relay journal lacks the exact two physical receiver receipts'
        objective = self.unique(unreal.SovCampaignEncounterObjective, 'completion_beat', 'RelayOverlook')
        assert not objective.is_result_pending(), 'E2 campaign result is still pending'
        receivers = [a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.SovCampaignRelayReceiver)
                     if str(a.receiver_id) in RECEIVERS]
        assert len(receivers) == 2 and all(a.is_disabled() for a in receivers)
        return relay

    def begin_route(self, points, then):
        self.waypoints = [tuple(p) for p in points]
        self.next_route_state = then
        self.path_points, self.path_target = [], None
        self.stage('walk_route', dict(points=points, then=then))

    def walk(self, pc, pawn):
        if not self.waypoints:
            self.inject()
            self.stage(self.next_route_state)
            return
        target = self.waypoints[0]
        current = pawn.get_actor_location()
        if math.hypot(current.x-target[0], current.y-target[1]) < 40. and abs(current.z-target[2]) < 140.:
            self.waypoints.pop(0)
            self.path_target = None
            self.last_motion_at = time.monotonic()
            self.inject()
            return
        now = time.monotonic()
        if self.path_target != target or now-self.last_path > .8:
            self.path_target, self.last_path = target, now
            nav = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(
                self.world, current, unreal.Vector(*target), pawn, None)
            complete = nav is not None and nav.is_valid() and not nav.is_partial()
            self.path_points = list(nav.path_points)[1:] if complete else []
            row = dict(destination=target, complete=complete,
                       points=[_xyz(p) for p in nav.path_points] if nav else [], elapsed=now-self.started)
            self.report['last_route_path'] = row
            self.report['route_paths'].append(row)
        movement = (0., 0.)
        while self.path_points:
            movement, reached = self.local_move(pc, pawn, _xyz(self.path_points[0]), stop=25.)
            if not reached:
                break
            self.path_points.pop(0)
        # Unreal may project the requested point slightly onto a nearby polygon.
        # Once the full path is consumed, do not wait forever at that endpoint
        # because the unreachable raw waypoint is outside the 40cm arrival test.
        route=self.report.get('last_route_path',{})
        if (not self.path_points and route.get('destination')==target
                and reached_projected_destination(_xyz(current),target,route.get('points',[]),route.get('complete',False))):
            self.report.setdefault('projected_arrivals',[]).append(dict(requested=target,
                projected=route['points'][-1],actual=_xyz(current),elapsed=now-self.started))
            self.waypoints.pop(0); self.path_target=None; self.last_motion_at=now
            self.inject(); return
        self.inject(move=movement)
        if self.last_position is None or math.hypot(current.x-self.last_position[0], current.y-self.last_position[1]) > 35.:
            self.last_position, self.last_motion_at = _xyz(current), now
        assert now-self.last_motion_at < 15., 'Normal traversal stalled: '+json.dumps(self.report.get('last_route_path'))

    def aim_interaction(self, pc, pawn, actor, beat):
        interaction, component = pc.get_interaction_component(), actor.interactable
        look, error = self.look(self.world, pc, actor.get_actor_location())
        self.inject(look=look)
        admission = component.can_interact(pawn, interaction)
        focus = interaction.get_editor_property('viewed_interactable')
        self.report['last_interaction'] = dict(actor=_path(actor), focus=_path(focus),
            admission=str(admission), native_action_text=str(component.get_interactable_action_text(pawn, interaction)), admitted=admission is not None, angle_error=error,
            last_result=str(actor.last_result), distance=math.dist(_xyz(pawn.get_actor_location()), _xyz(actor.get_actor_location())))
        if beat == HANDOFF:
            anchor = actor.handoff_anchor
            position = pawn.get_actor_location()
            # GetPawnViewLocation is not reflected. The inherited APawn implementation
            # is exactly location + current BaseEyeHeight (no project override).
            eyes = unreal.Vector(position.x, position.y, position.z+float(pawn.base_eye_height))
            endpoint = anchor.get_actor_location()
            ignored = [pawn]+list(pawn.get_attached_actors())
            visual = pawn.get_character_visual()
            if visual and visual not in ignored:
                ignored.append(visual)
            result = unreal.SystemLibrary.line_trace_single(self.world, eyes, endpoint,
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignored, unreal.DrawDebugTrace.NONE, True)
            if result is None:
                hit = None  # Native bool/out wrapper returns None for an ordinary miss.
            else:
                hits = [v for v in result if isinstance(v, unreal.HitResult)] if isinstance(result, tuple) else [result]
                assert len(hits) == 1 and isinstance(hits[0], unreal.HitResult)
                hit = hits[0].to_tuple()
            self.report['handoff_geometry'] = dict(anchor=_path(anchor), request=_path(actor),
                origin=_xyz(eyes), endpoint=_xyz(endpoint), request_location=_xyz(actor.get_actor_location()),
                anchor_range=float(anchor.request_range), request_range=float(component.interaction_distance),
                anchor_distance=math.dist(_xyz(position),_xyz(endpoint)),
                request_body_extent=_xyz(actor.body.get_scaled_box_extent()),
                request_body_center=_xyz(actor.body.get_world_location()),
                observed_blocking=bool(hit and hit[0]), observed_hit=_path(hit[9]) if hit else None, impact=_xyz(hit[5]) if hit else None,
                native_rule='ECC_Visibility: unobstructed or hit actor equals the HandoffAnchor; query ignores player character',
                observation_query='Pawn and attached visual excluded; native CanInteract remains authoritative',
                request_admission=str(admission), native_action_text=self.report['last_interaction']['native_action_text'])
        if error < 3. and admission is not None and focus == component:
            assert abs(float(component.interaction_time)-.35) < .001, 'Authored hold must remain 0.35 seconds'
            self.unbind_request()
            self.hold_actor, self.hold_beat = actor, beat
            self.request_result = None
            self.saw_countdown = False
            self.held_game_seconds = 0.
            self.hold_started = unreal.GameplayStatics.get_time_seconds(self.world)
            def result(accepted, message):
                row = dict(beat=beat, accepted=accepted, message=str(message), elapsed=time.monotonic()-self.started)
                self.report['request_results'].append(row)
                self.request_result = row
            self.request_callback = result
            self.request_delegate = actor.on_request_result
            self.request_delegate.add_callable(result)
            self.stage('hold_request')

    def hold_request(self, pc):
        remaining = float(pc.get_interaction_component().get_editor_property('remaining_interact_time'))
        if 0. < remaining <= .35:
            self.saw_countdown = True
        self.held_game_seconds = unreal.GameplayStatics.get_time_seconds(self.world)-self.hold_started
        began_scene = self.hold_beat == MEETING and self.scene_component.get_phase() != unreal.SovCinematicPhase.IDLE
        began_handoff = self.hold_beat == HANDOFF and (
            pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.SWITCHING
            or pc.get_campaign_state().is_beat_complete(unreal.Name(MISSION), unreal.Name(HANDOFF)))
        if self.request_result is not None or self.hold_actor.is_request_pending() or began_scene or began_handoff:
            self.inject()
            assert self.saw_countdown, 'Actual normal hold countdown was not observed'
            if self.request_result is not None:
                assert self.request_result['accepted'], 'Native request rejected: '+self.request_result['message']
            self.report['holds'].append(dict(beat=self.hold_beat, native_countdown=True,
                                            input_game_seconds=self.held_game_seconds))
            self.stage('wait_scene' if self.hold_beat == MEETING else 'wait_handoff')
            return
        assert time.monotonic()-self.phase_at < 8., 'Hold failed: '+json.dumps(self.report['last_interaction'])
        self.inject(interact=1.)

    def observe_scene(self):
        phase = self.scene_component.get_phase()
        if phase != self.last_scene_phase:
            self.last_scene_phase = phase
            self.report['scene_phases'].append(dict(phase=str(phase), elapsed=time.monotonic()-self.started, observed='poll'))
        index = self.scene.get_current_dialogue_cue_index()
        if index >= 0 and index not in self.cues_seen:
            self.cues_seen.add(index)
            cue = self.scene.dialogue_cues[index]
            self.report['dialogue_cues'].append(dict(index=index, speaker=str(cue.speaker), text=str(cue.text),
                                                    elapsed=time.monotonic()-self.started))
        assert phase != unreal.SovCinematicPhase.FAILED, 'Native Meeting scene failed: '+json.dumps(self.report['scene_phases'][-5:])
        return phase

    def initialize(self, world, pc, pawn, state, events):
        assert isinstance(pawn, unreal.SovSeleneCharacter) and pawn.is_character_ready() and pawn.is_alive()
        assert pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE
        assert [e['beat'] for e in events] == INITIAL, 'Start only after the real E2 victory and two receiver receipts'
        self.world, self.initial_pawn, self.initial_controller = world, _path(pawn), pc
        self.owner = self.get_input_owner(world)
        snapshot = unreal.GameUserSettings.get_game_user_settings().get_settings_snapshot()
        assert not snapshot.tap_interactions and abs(snapshot.interaction_hold_scale-1.) < .001, 'Requires existing standard holds; settings are not changed'
        self.report['settings'] = snapshot.export_text()
        self.initial_events = events
        self.e2 = self.unique(unreal.SovEncounterDirector, 'encounter_id', 'M12_E2_RelayOverlook')
        self.e3 = self.unique(unreal.SovEncounterDirector, 'encounter_id', 'M12_E3_SharedBreach')
        self.entry = self.unique(unreal.SovCampaignEncounterObjective, 'completion_beat', 'BreachSharedJunction')
        assert self.e3.get_encounter_state() == unreal.SovEncounterState.INACTIVE
        relay = self.check_relay(events)
        self.companion_state(pc, pawn, False)
        self.meeting_request = self.unique(unreal.SovAurelionRequestActor, 'beat_id', MEETING)
        self.handoff_request = self.unique(unreal.SovAurelionRequestActor, 'beat_id', HANDOFF)
        assert self.meeting_request.operation == unreal.SovAurelionRequest.PLAY_SCENE
        assert self.handoff_request.operation == unreal.SovAurelionRequest.HANDOFF
        self.scene = self.meeting_request.story
        assert self.scene is not None
        self.scene_component = self.scene.campaign_cinematic
        assert str(self.scene_component.beat_id) == MEETING and str(self.scene_component.mission_id) == MISSION
        assert self.scene_component.get_phase() == unreal.SovCinematicPhase.IDLE
        def phase_changed(phase, reason):
            self.report['scene_phases'].append(dict(phase=str(phase), reason=reason,
                elapsed=time.monotonic()-self.started, observed='native_delegate'))
        self.scene_callback = phase_changed
        self.scene_delegate = self.scene_component.on_phase_changed
        self.scene_delegate.add_callable(phase_changed)
        self.report['initial'] = dict(pawn=self.initial_pawn, world=_path(world), journal=events, relay=relay,
            meeting_request=_path(self.meeting_request), shared_handoff=_path(self.handoff_request),
            bindings={n:[k.export_text() for k in self.owner.query_keys_mapped_to_action(a)] for n,a in self.actions.items()})
        self.stage('wait_relay_gate')

    def tick(self, delta):
        if self.done:
            return
        try:
            now = time.monotonic()
            assert now-self.started < 600., 'E2-to-E3 entry continuation exceeded ten-minute bound'
            assert now-self.phase_at < (240. if self.phase == 'walk_route' else 90.), 'Stage deadline: '+self.phase
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world and '/Aurelion/Maps/UEDPIE_' in world.get_path_name() and 'L_Aurelion_M12' in world.get_name(), 'Requires actual existing M12 PIE'
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            assert isinstance(pc, unreal.SovPlayerController)
            state = pc.get_campaign_state()
            assert state and state.get_active_mission() and str(state.get_active_mission().mission_id) == MISSION
            events = journal(state)
            assert [e['beat'] for e in events] == ALLOWED[:len(events)] and len(events) <= len(ALLOWED), 'Unexpected journal evolution'
            assert all(e['mission'] == MISSION for e in events)
            if self.phase == 'initialize':
                self.initialize(world, pc, pawn, state, events)
            assert world == self.world and pc == self.initial_controller, 'Unexpected world/controller replacement'
            assert events[:len(self.initial_events)] == self.initial_events, 'Existing receipt identities changed'
            assert pc.get_campaign_transition_state() != unreal.SovCampaignTransitionState.FAILED, 'Native campaign handoff failed'
            if self.request_result is not None:
                assert self.request_result['accepted'], 'Native request rejected: '+self.request_result['message']
            if self.scene_component:
                self.observe_scene()
            if not pawn:
                assert self.phase in ('hold_request','wait_handoff'), 'Unexpected absent pawn'
                self.inject()
                return
            assert isinstance(pawn, unreal.SovPlayerCharacterBase)
            assert pawn.is_alive() and pawn.get_health() > 0., 'Player died; no healing/retry was issued'
            if now-self.last_sample > .5:
                self.last_sample = now
                self.report['samples'].append(dict(elapsed=now-self.started, phase=self.phase, pawn=_path(pawn),
                    position=_xyz(pawn.get_actor_location()), health=pawn.get_health(), ready=pawn.is_character_ready(),
                    transition=str(pc.get_campaign_transition_state()), journal=events,
                    scene=str(self.scene_component.get_phase()), e3=str(self.e3.get_encounter_state())))
            if now-self.last_write > 1.:
                self.last_write = now
                self.write()
            if self.phase == 'hold_request':
                self.hold_request(pc)
                return
            if self.phase == 'wait_scene':
                self.inject()
                if self.scene_component.get_phase() == unreal.SovCinematicPhase.COMPLETED:
                    assert len(events) == 7 and events[-1]['beat'] == MEETING
                    assert valid_guid(events[-1]['cinematic']) and not events[-1]['skipped']
                    assert self.cues_seen == set(range(len(self.scene.dialogue_cues))), 'Full native dialogue timeline was not observed'
                    if 'meeting' in self.report:
                        assert self.report['meeting']['journal'] == events[-1], 'Meeting receipt changed while waiting for carrier clearance'
                    self.report['meeting'] = dict(journal=events[-1], cue_count=len(self.cues_seen), completed=True, skipped=False)
                    assert isinstance(pawn, unreal.SovSeleneCharacter) and _path(pawn) == self.initial_pawn, 'Pre-handoff Meeting pawn identity changed'
                    if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world):
                        self.report['carrier_clearance_wait'] = 'Current Meeting pawn/state must be ready and unpaused'
                        return
                    if pc.get_campaign_transition_state() != unreal.SovCampaignTransitionState.IDLE:
                        self.report['carrier_clearance_wait'] = 'Current campaign transition must be Idle'
                        return
                    # The native journal gates may tick after the scene callback. Observe
                    # their real receipt-driven state within the original wait_scene deadline.
                    gates = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionJournalGate)
                    observed_gates = []
                    for label, expected in (('Aurelion_CarrierApproachPresentation', False),
                                            ('Aurelion_CarrierRescuedPresentation', True)):
                        matches = [g for g in gates if g.get_actor_label() == label]
                        assert len(matches) == 1, 'Carrier journal gate must be unique: '+label
                        observed_gates.append(dict(label=label, blocking=matches[0].is_blocking_route(), expected=expected))
                    self.report['carrier_gate_observation'] = observed_gates
                    if any(g['blocking'] != g['expected'] for g in observed_gates):
                        self.report['carrier_clearance_wait'] = 'Native carrier gates have not applied the genuine Meeting receipt'
                        return
                    if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
                        self.report['carrier_clearance_wait'] = 'Navigation build remains pending'
                        return
                    if now-self.carrier_probe_at < 1.:
                        return
                    self.carrier_probe_at = now
                    attempts = self.report.setdefault('carrier_clearance_attempts', [])
                    evidence = self.out/('carrier-after-meeting-readonly-%03d.json'%(len(attempts)+1))
                    clearance = inspect_carrier_clearance(evidence, 'after_meeting')
                    row = dict(status=clearance['status'], unchanged=clearance['unchanged'], path=str(evidence),
                               sha256=hashlib.sha256(evidence.read_bytes()).hexdigest(), elapsed=now-self.started)
                    attempts.append(row)
                    self.report['carrier_clearance_after_meeting'] = row
                    self.report['carrier_clearance_wait'] = clearance['status']
                    self.write()
                    assert clearance['unchanged'], 'Carrier inspection changed the journal, pawn position or protected packages'
                    if clearance['status'] == 'pending_navigation_build':
                        return
                    assert clearance['status'] == 'passed_readonly_carrier_clearance', 'Post-Meeting carrier clearance failed: '+str(clearance['errors'])
                    self.begin_route([(1120.,-840.,90.)], 'aim_handoff')
                return
            if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world):
                self.inject()
                return
            if pc.get_campaign_transition_state() != unreal.SovCampaignTransitionState.IDLE:
                self.inject()
                return
            if self.phase == 'wait_relay_gate':
                self.inject()
                gate = self.unique(unreal.SovAurelionJournalGate, 'beat_id', 'RelayOverlook')
                if not gate.is_blocking_route():
                    # Same authored east dogleg; bounded full-path legs avoid the observed long-query cutoff.
                    self.begin_route([(7000.,-8700.,90.),(7000.,-6000.,90.),(7000.,-3500.,90.),(7000.,-2000.,90.),(5000.,-2000.,90.),
                        (5000.,0.,90.),(3300.,0.,90.),(2300.,0.,90.),(1500.,0.,90.),
                        (0.,-2000.,90.),(1140.,-1270.,90.)], 'aim_meeting')
            elif self.phase == 'walk_route':
                if self.next_route_state == 'wait_e3_entry' and self.e3.get_encounter_state() == unreal.SovEncounterState.ACTIVE:
                    self.inject()
                    self.stage('wait_e3_entry')
                else:
                    self.walk(pc, pawn)
            elif self.phase == 'aim_meeting':
                self.aim_interaction(pc, pawn, self.meeting_request, MEETING)
            elif self.phase == 'aim_handoff':
                self.aim_interaction(pc, pawn, self.handoff_request, HANDOFF)
            elif self.phase == 'wait_handoff':
                self.inject()
                if len(events) == 8 and isinstance(pawn, unreal.SovTarrikCharacter) and _path(pawn) != self.initial_pawn:
                    assert valid_guid(events[-1]['handoff']) and events[-1]['anchor'] == 'M12_TarrikSharedBreach'
                    companion = self.companion_state(pc, pawn, True)
                    if companion is None:
                        return
                    self.companion = companion
                    self.report['shared_handoff'] = dict(pawn=_path(pawn), companion=_path(companion),
                        companion_identity=companion.get_companion_identity().export_text(),
                        ownership_observation='Committed native handoff and Idle; unique controller-owned visible proxy; public contextual command admission validates current leader',
                        command_state=str(companion.get_companion_component().get_command_state()), journal=events[-1])
                    self.begin_route([(0.,1500.,90.),(0.,2300.,90.),(0.,3200.,90.),
                                      (0.,3900.,0.),(0.,4800.,-210.),(0.,5900.,-510.),(0.,6770.,-510.)], 'wait_e3_entry')
            elif self.phase == 'wait_e3_entry':
                self.inject()
                encounter = self.e3.get_encounter_state()
                assert encounter not in (unreal.SovEncounterState.FAILED, unreal.SovEncounterState.RESTORING), 'E3 failed/restored before initial entry qualified'
                if encounter != unreal.SovEncounterState.ACTIVE:
                    return
                assert len(events) == 8 and not state.is_beat_complete(unreal.Name(MISSION), unreal.Name('BreachSharedJunction'))
                assert not self.e3.has_confirmed_victory()
                assert self.e3.has_encounter_player(pawn)
                assert self.entry.start_volume.is_overlapping_component(pawn.get_editor_property('capsule_component')), 'Native E3 activation did not coincide with physical capsule entry'
                required = [p for p in self.e3.participants if p.required_for_victory]
                protected = [p for p in self.e3.participants if not p.required_for_victory]
                assert len(required) == 7 and len(protected) == 2
                assert all(p.character and p.character.is_alive() and p.character.get_health() > 0. for p in required+protected)
                assert sum(not p.character.get_editor_property('hidden') for p in required) == 4, 'Initial E3 must release three Linkbound and one Wall-runner'
                assert self.companion_state(pc, pawn, True) == self.companion, 'Companion ownership changed during entry'
                self.report['e3_entry'] = dict(attempt=self.e3.get_attempt_id().export_text(), journal=events,
                    physical_overlap=True, player=_path(pawn), companion=_path(self.companion),
                    roster=[dict(id=str(p.participant_id), actor=_path(p.character), hidden=p.character.get_editor_property('hidden'),
                        health=p.character.get_health(), required=p.required_for_victory) for p in required+protected])
                self.finish(True, 'Actual E2 receipt preserved; full Meeting, ordinary shared handoff and native E3 initial entry observed. Door, combat, rescue and later route remain unqualified.')
        except Exception:
            self.report['error'] = traceback.format_exc()
            self.finish(False, self.report['error'])


def start(output_directory=None):
    """Existing PIE only. Requires completed E2, Selene and unchanged 0.35s holds."""
    global _RUN
    assert _RUN is None or _RUN.done, 'E3 driver already running'
    assert common._RUN is None or common._RUN.done, 'Stop the E1 input driver first'
    target = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (target/'e3-entry-input-continuation.json').exists(), 'Use a new evidence directory'
    _RUN = Run(target)
    _RUN.write()
    _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False, 'Stopped by operator')


if __name__ == '__main__':
    start()
