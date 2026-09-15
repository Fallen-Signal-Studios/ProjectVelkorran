"""Retained M12 PIE: earned GroundLyric -> cages/share/west choice -> E4A entry.

Ordinary Enhanced Input only. Native scene, handoff, priority, checkpoint and
encounter owners supply every receipt. Importing the module performs no work.
"""
import json
import math
import os
from pathlib import Path
import re
import sys
import time
import traceback
import unreal
import continue_aurelion_e1_input as common
import continue_aurelion_e3_entry_input as entry
import continue_aurelion_e3_rescue_input as rescue

_RUN = None
MISSION = entry.MISSION
INITIAL = list(rescue.ALLOWED)
DOMINION = 'DestroyDominionResonator'
HANDOFF = 'HandoffToSeleneCage'
REFORMATION = 'DestroyReformationCage'
SHARING = 'ShareIsolatedThreatData'
WEST, EAST, PRIORITY = 'PriorityWestStretchers', 'PriorityEastWalkers', 'LocalPriorityCommitted'
# Both legal outcomes of the one local decision are playable; the default preserves the original West route.
CHOICE = os.environ.get('SOV_AURELION_PRIORITY', 'WestStretchers')
assert CHOICE in ('WestStretchers', 'EastWalkers'), 'SOV_AURELION_PRIORITY must be WestStretchers or EastWalkers'
SELECTED, REJECTED = (WEST, EAST) if CHOICE == 'WestStretchers' else (EAST, WEST)
FINAL = INITIAL + [DOMINION, HANDOFF, REFORMATION, SHARING, SELECTED, PRIORITY]
E4_ID = 'M12_E4_QuarantineCrucibleA'
E4_BEAT = 'SeverCrucibleLinks'
HOSTILES = {'E4.Linkbound1', 'E4.Linkbound2', 'E4.Weaver', 'E4.Elite', 'E4.WallRunner'}
PROTECTED = {'E4.Protected.'+name for name in ('Malik','Tharne','Lyessa','WestDominionStretcher',
    'WestReformationStretcher','EastDominionWalker','EastReformationWalker')}
FACTS = {
    DOMINION: {'Campaign.Aurelion.M12.Fact.DominionResonator':'Campaign.Aurelion.Value.Destroyed'},
    REFORMATION: {'Campaign.Aurelion.M12.Fact.ReformationCage':'Campaign.Aurelion.Value.Destroyed'},
    SHARING: {'Campaign.Aurelion.M12.Fact.ThreatSharing':'Campaign.Aurelion.Value.IsolatedThreatDataOnly',
              'Campaign.Aurelion.M12.Fact.CommandAuthority':'Campaign.Aurelion.Value.AuthorityRetained'},
}
_path, _xyz, _optional = common._path, common._xyz, common._optional


def tag_name(value):
    # Read the single typed tag's export; never construct or publish gameplay state.
    found = re.findall(r'[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+', value.export_text())
    assert len(found) == 1, 'Unexpected nonempty gameplay-tag export: '+value.export_text()
    return found[0]


SCENE_RETRY_LIMIT = 3
SCENE_RETRY_WAIT_SECONDS = 4.
SCENE_RETRY_GRACE_SECONDS = 12.


def _participant_applies_exit(participant):
    for name in ('apply_exit_transform', 'b_apply_exit_transform'):
        try:
            return bool(participant.get_editor_property(name))
        except Exception:
            continue
    return False


class Run(rescue.Run):
    def __init__(self, output_directory):
        super().__init__(output_directory)
        self.e4 = self.e4b = self.e4_entry = self.support = self.priority = self.saves = None
        self.save_delegate = self.save_callback = self.encounter_delegate = self.encounter_callback = None
        self.current_pawn_path = None
        self.current_companion_path = None
        self.e4_paths = {}
        self.cage_gates = {}
        self.held_kind = None
        self.report.update(scope='GroundLyric through three full scenes, native Selene handoff, exclusive '+CHOICE+' priority and E4A initial entry only',
            pending=['E4A link severing and combat', 'E4 WallRunner reinforcement', 'Crucible handoff and Thermal Fracture',
                     'Every later route beat', 'Physical keyboard operation and rendered scene quality'],
            entry_requires=['exact eleven-beat prefix through GroundLyric with actual E3 and cinematic receipts',
                            'ready living Tarrik and actual Selene companion', 'unselected exclusive priority',
                            'inactive E4A/E4B with five living hostiles and all seven protected people',
                            'existing normal hold settings and native storage owner'],
            saves=[], e4_states=[], scene_phases=[], dialogue_cues=[], request_results=[], route_paths=[],
            completed_scenes={}, physical_cage_views={}, choice=None)
        for unused in ('door_states','waves','native_combat_victory'):
            self.report.pop(unused,None)

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 3)
        temp = self.out/'e4-entry-input-continuation.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        common.replace_report_with_retry(temp, self.out/'e4-entry-input-continuation.json')

    def finish(self, passed, reason):
        if self.done:
            return
        if self.save_delegate is not None and self.save_callback is not None:
            self.report['remove_save_observer'] = _optional(lambda: self.save_delegate.remove_callable(self.save_callback))
        if self.encounter_delegate is not None and self.encounter_callback is not None:
            self.report['remove_encounter_observer'] = _optional(lambda: self.encounter_delegate.remove_callable(self.encounter_callback))
        self.save_delegate = self.save_callback = self.encounter_delegate = self.encounter_callback = None
        super().finish(passed, reason)
        for field in ('e4','e4b','e4_entry','support','priority','saves'):
            setattr(self, field, None)
        self.cage_gates.clear()
        unreal.log('Aurelion E4 entry continuation: '+self.report['status']+': '+reason)

    def roster_for(self, director):
        result = []
        for p in director.participants:
            actor = p.character
            valid = actor is not None and unreal.SystemLibrary.is_valid(actor)
            result.append(dict(id=str(p.participant_id), actor=_path(actor) if valid else None,
                required=p.required_for_victory, alive=rescue.alive(actor),
                health=actor.get_health() if valid else None,
                hidden=bool(actor.get_editor_property('hidden')) if valid else None,
                position=_xyz(actor.get_actor_location()) if valid else None))
        return result

    def owned_companion(self, pc, pawn, identity):
        actors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world,
            unreal.SovProtagonistCompanionCharacter) if a.get_owner() == pc and rescue.alive(a)
            and not a.get_editor_property('hidden')]
        if not actors:
            return None
        assert len(actors) == 1, 'Companion ownership is ambiguous'
        actor = actors[0]
        assert identity in actor.get_companion_identity().export_text(), 'The shared handoff exposed the wrong companion identity'
        component = actor.get_companion_component()
        assert component and not component.is_disabled()
        # BlueprintPure admission checks the actual current leader and native context.
        assert component.can_request_command(pawn, unreal.SovCompanionCommand.REGROUP, pawn) is not None
        return actor

    def selected_choice(self, state):
        return str(state.get_selected_choice(unreal.Name(MISSION), unreal.Name('ImmediateProtection')))

    def support_state(self):
        return dict(priority=str(self.support.get_priority()),
            west_accessible=self.support.is_west_cache_accessible(), east_open=self.support.is_east_flank_open(),
            west_collision=str(self.support.west_cache_barrier.get_collision_enabled()),
            east_collision=str(self.support.east_flank_barrier.get_collision_enabled()),
            aftermath_id=str(self.support.get_aftermath_consequence_id()))

    def facts_for(self, state, beat):
        matches = [b for b in state.get_active_mission().beats if str(b.beat_id) == beat]
        assert len(matches) == 1
        rows = [dict(key=tag_name(w.key), expected=tag_name(w.value),
                     actual=tag_name(state.get_state_value(w.key)), protected=w.canon_protected)
                for w in matches[0].state_writes]
        assert {r['key']:r['expected'] for r in rows} == FACTS[beat], 'Authored canonical fact contract changed'
        assert all(r['actual'] == r['expected'] and r['protected'] for r in rows), 'Native scene receipt did not retain its protected facts'
        return rows

    def initialize(self, world, pc, pawn, state, events):
        assert [e['beat'] for e in events] == INITIAL, 'Start only after the actual GroundLyric receipt'
        assert isinstance(pawn, unreal.SovTarrikCharacter) and pawn.is_character_ready() and rescue.alive(pawn)
        assert pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.IDLE
        self.world, self.initial_controller, self.initial_pawn = world, pc, _path(pawn)
        self.current_pawn_path = self.initial_pawn
        self.owner = self.get_input_owner(world)
        settings = unreal.GameUserSettings.get_game_user_settings().get_settings_snapshot()
        assert not settings.tap_interactions and abs(settings.interaction_hold_scale-1.) < .001, 'Normal holds required; no settings are changed'
        self.initial_events = events
        self.e3 = self.unique(unreal.SovEncounterDirector, 'encounter_id', 'M12_E3_SharedBreach')
        assert self.e3.get_encounter_state() == unreal.SovEncounterState.SUCCEEDED and self.e3.has_confirmed_victory()
        breach = next(e for e in events if e['beat'] == rescue.BREACH)
        assert breach['encounter'] == 'M12_E3_SharedBreach' and breach['attempt'] == self.e3.get_attempt_id().export_text()
        assert entry.valid_guid(breach['attempt'])
        for beat in (rescue.MARINE, rescue.LYRIC):
            receipt = next(e for e in events if e['beat'] == beat)
            assert entry.valid_guid(receipt['cinematic']) and not receipt['skipped']
        self.companion = self.owned_companion(pc,pawn,'Selene')
        assert self.companion is not None
        self.current_companion_path = _path(self.companion)
        self.e4 = self.unique(unreal.SovAurelionLinkPhaseDirector,'encounter_id',E4_ID)
        self.e4b = self.unique(unreal.SovAurelionThermalPhaseDirector,'encounter_id','M12_E4_QuarantineCrucibleB')
        self.e4_entry = self.unique(unreal.SovCampaignEncounterObjective,'completion_beat',E4_BEAT)
        assert self.e4_entry.encounter_director == self.e4 and str(self.e4_entry.mission_id) == MISSION
        assert self.e4.get_encounter_state() == self.e4b.get_encounter_state() == unreal.SovEncounterState.INACTIVE
        assert not self.e4.has_confirmed_victory() and not list(self.e4b.participants)
        rows = self.roster_for(self.e4)
        assert {r['id'] for r in rows if r['required']} == HOSTILES
        assert {r['id'] for r in rows if not r['required']} == PROTECTED and all(r['alive'] for r in rows)
        self.e4_paths = {r['id']:r['actor'] for r in rows}
        self.coordination = self.e4.get_coordination_component()
        assert self.coordination is not None
        rules = list(self.coordination.wave_release_rules)
        assert len(rules) == 1 and rules[0].wave == 1
        assert rules[0].condition == unreal.SovEncounterWaveCondition.ACCEPTED_CRUCIBLE_LINK
        assert rules[0].maximum_living_released_hostiles == 4
        assert len(list(self.e4.required_links)) == 2 and not self.e4.auto_request_handoff
        for beat in (DOMINION, REFORMATION, SHARING):
            actor = self.unique(unreal.SovAurelionRequestActor,'beat_id',beat)
            assert actor.operation == unreal.SovAurelionRequest.PLAY_SCENE and actor.story
            assert str(actor.story.campaign_cinematic.mission_id) == MISSION
            assert str(actor.story.campaign_cinematic.beat_id) == beat
            assert actor.story.campaign_cinematic.get_phase() == unreal.SovCinematicPhase.IDLE
            assert abs(float(actor.interactable.interaction_time)-.35) < .001
            self.requests[beat] = actor
        self.handoff_request = self.unique(unreal.SovAurelionRequestActor,'beat_id',HANDOFF)
        assert self.handoff_request.operation == unreal.SovAurelionRequest.HANDOFF
        assert str(self.handoff_request.handoff_anchor.anchor_id) == 'M12_SeleneCage'
        assert abs(float(self.handoff_request.interactable.interaction_time)-.35) < .001
        for beat in (DOMINION, REFORMATION):
            gate = self.unique(unreal.SovAurelionJournalGate,'beat_id',beat)
            assert not gate.use_gate_body and list(gate.bound_visual_actors) and gate.is_blocking_route()
            self.cage_gates[beat] = gate
        supports = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionPrioritySupport)
        assert len(supports) == 1
        self.support = supports[0]
        terminals = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionPriorityTerminal)
        assert len(terminals) == 2 and {a.priority for a in terminals} == {
            unreal.SovAurelionRescuePriority.WEST_STRETCHERS,unreal.SovAurelionRescuePriority.EAST_WALKERS}
        selected_priority = unreal.SovAurelionRescuePriority.WEST_STRETCHERS if CHOICE == 'WestStretchers' else unreal.SovAurelionRescuePriority.EAST_WALKERS
        self.priority = next(a for a in terminals if a.priority == selected_priority)
        assert abs(float(self.priority.interactable.interaction_time)-.35) < .001
        assert self.selected_choice(state) == 'None' and self.support.get_priority() == unreal.SovAurelionRescuePriority.UNSET
        assert not self.support.is_west_cache_accessible() and not self.support.is_east_flank_open()
        instance = unreal.GameplayStatics.get_game_instance(world)
        saves = [s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == instance]
        assert len(saves) == 1 and saves[0].is_platform_storage_owner_available()
        self.saves = saves[0]
        assert not self.saves.is_awaiting_failure_decision() and not self.saves.is_load_pending()
        def saved(result, slot, message):
            # Copy values now: the native delegate's reflected struct is only borrowed.
            self.report['saves'].append(dict(result=str(result), succeeded=result == unreal.SovSaveResult.SUCCESS,
                boundary=str(slot.boundary_id), kind=str(slot.kind), boundary_kind=str(slot.boundary_kind),
                checkpoint=slot.kind==unreal.SovSaveSlotKind.CHECKPOINT,slot_index=slot.slot_index,
                explicit_boundary=slot.boundary_kind==unreal.SovSaveBoundary.EXPLICIT_CHECKPOINT,
                mission=str(slot.mission_id), map=str(slot.map_package), generation=slot.generation,
                protagonist=slot.active_protagonist.export_text(), message=str(message),
                journal_beats=[str(e.beat_id) for e in pc.get_campaign_state().get_journal()],
                elapsed=time.monotonic()-self.started))
        self.save_callback, self.save_delegate = saved, self.saves.on_save_completed
        self.save_delegate.add_callable(self.save_callback)
        def changed(previous,current):
            self.report['e4_states'].append(dict(previous=str(previous),current=str(current),
                attempt=self.e4.get_attempt_id().export_text(),elapsed=time.monotonic()-self.started))
        self.encounter_callback, self.encounter_delegate = changed, self.e4.on_encounter_state_changed
        self.encounter_delegate.add_callable(self.encounter_callback)
        self.report['settings'] = settings.export_text()
        self.report['initial'] = dict(world=_path(world),pawn=_path(pawn),companion=_path(self.companion),
            journal=events,e3_proof=breach,e4=rows,support=self.support_state(),
            scenes={b:dict(request=_path(a),location=_xyz(a.get_actor_location()),station=_xyz(a.story.get_actor_location())) for b,a in self.requests.items()},
            handoff=_path(self.handoff_request),priority=_path(self.priority),
            mappings={n:[k.export_text() for k in self.owner.query_keys_mapped_to_action(a)] for n,a in self.actions.items()})
        self.stage('wait_grounding_exit')

    def select_scene(self, beat, route=None):
        self.unbind_scene(); self.unbind_request()
        self.request_result = None
        self.scene_beat = beat
        self.scene = self.requests[beat].story
        self.scene_component = self.scene.campaign_cinematic
        self.cues_seen, self.last_scene_phase = set(), None
        self.scene_retry = dict(attempts=0, failed_at=None, staged_at=None, failures=[])
        assert self.scene_component.get_phase() == unreal.SovCinematicPhase.IDLE
        def changed(phase,reason):
            self.report['scene_phases'].append(dict(beat=beat,phase=str(phase),reason=str(reason),
                elapsed=time.monotonic()-self.started,observed='native_delegate'))
        self.scene_callback,self.scene_delegate = changed,self.scene_component.on_phase_changed
        self.scene_delegate.add_callable(self.scene_callback)
        p = self.scene.get_actor_location()
        self.begin_route(list(route or [])+[(p.x+40.,p.y-70.,p.z)],'aim_scene')

    def scene_reason(self):
        for row in reversed(self.report['scene_phases']):
            if row.get('beat') == self.scene_beat and 'FAILED' in str(row.get('phase')) and row.get('reason'):
                return str(row['reason'])
        return ''

    def exit_census(self):
        # Read-only: who stands on each authored exit mark when the native exit check refused the scene.
        rows = []
        for participant in self.scene_component.participants:
            if not _participant_applies_exit(participant):
                continue
            mark = _xyz(participant.exit_transform.translation)
            occupants = []
            for actor in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.Character):
                distance = math.dist(_xyz(actor.get_actor_location()), mark)
                if distance < 250.:
                    occupants.append(dict(actor=_path(actor), name=str(actor.get_name()), distance=round(distance, 1),
                        alive=rescue.alive(actor), collision=actor.get_actor_enable_collision()))
            rows.append(dict(binding=str(participant.binding_tag), mark=mark,
                occupants=sorted(occupants, key=lambda row: row['distance'])))
        return rows

    def scene_failed(self, phase):
        # An ordinary player would use the station again once the mark clears; every attempt is recorded.
        retry = self.scene_retry
        now = time.monotonic()
        if retry['staged_at'] is not None and now-retry['staged_at'] < SCENE_RETRY_GRACE_SECONDS:
            return
        if retry['failed_at'] is None:
            retry['failed_at'] = now
            retry['failures'].append(dict(beat=self.scene_beat, reason=self.scene_reason(), elapsed=now-self.started,
                attempt=retry['attempts'], census=self.exit_census()))
            self.report['scene_retries'] = retry
        reason = self.scene_reason()
        assert 'overlaps blocking geometry' in reason, 'Native scene failed: '+reason+' '+json.dumps(self.report['scene_phases'][-6:])
        assert retry['attempts'] < SCENE_RETRY_LIMIT, 'The native scene exit stayed blocked across ordinary replays: '+json.dumps(retry['failures'])
        if now-retry['failed_at'] < SCENE_RETRY_WAIT_SECONDS:
            self.inject()
            return
        retry['attempts'] += 1
        retry['failed_at'], retry['staged_at'] = None, now
        point = self.requests[self.scene_beat].get_actor_location()
        self.inject()
        self.begin_route([(point.x+40., point.y-70., point.z)], 'aim_scene')

    def finish_scene(self, state, events):
        if self.scene_component.get_phase() != unreal.SovCinematicPhase.COMPLETED:
            return
        assert len(events) == FINAL.index(self.scene_beat)+1 and events[-1]['beat'] == self.scene_beat
        assert entry.valid_guid(events[-1]['cinematic']) and not events[-1]['skipped']
        assert self.cues_seen == set(range(len(self.scene.dialogue_cues))), 'A native dialogue cue was not observed'
        self.report['completed_scenes'][self.scene_beat] = dict(receipt=events[-1],cue_count=len(self.cues_seen),
            completed=True,skipped=False,facts=self.facts_for(state,self.scene_beat),protected=self.roster_for(self.e4))
        self.stage('wait_scene_view')

    def cage_view(self, beat):
        gate = self.cage_gates[beat]
        actors = list(gate.bound_visual_actors)
        assert actors and all(a and unreal.SystemLibrary.is_valid(a) for a in actors), 'A bound cage presentation actor retired unexpectedly'
        rows = [dict(actor=_path(a),hidden=bool(a.get_editor_property('hidden')),collision=a.get_actor_enable_collision()) for a in actors]
        self.report['physical_cage_views'][beat] = dict(blocking=gate.is_blocking_route(),actors=rows)
        return not gate.is_blocking_route() and all(r['hidden'] and not r['collision'] for r in rows)

    def clear_scene_retry(self):
        self.scene_retry['staged_at'] = self.scene_retry['failed_at'] = None

    def leave_scene(self):
        self.unbind_scene(); self.unbind_request()
        self.scene = self.scene_component = None
        self.request_result = None

    def aim_special(self, pc, pawn, kind):
        actor = self.handoff_request if kind == 'handoff' else self.priority
        interaction, component = pc.get_interaction_component(),actor.interactable
        look,error = self.look(self.world,pc,actor.get_actor_location())
        admission = component.can_interact(pawn,interaction)
        focus = interaction.get_editor_property('viewed_interactable')
        self.report['last_interaction'] = dict(kind=kind,actor=_path(actor),focus=_path(focus),
            admitted=admission is not None,admission=str(admission),native_action_text=str(component.get_interactable_action_text(pawn,interaction)),angle_error=error,last_result=str(actor.last_result),
            player=_xyz(pawn.get_actor_location()),position=_xyz(actor.get_actor_location()),
            interaction_range=float(component.interaction_distance),hold=float(component.interaction_time))
        if kind == 'handoff':
            anchor = actor.handoff_anchor
            p = pawn.get_actor_location()
            self.report['handoff_geometry'] = dict(request=_xyz(actor.get_actor_location()),
                body_extent=_xyz(actor.body.get_scaled_box_extent()),anchor=_xyz(anchor.get_actor_location()),
                eyes=[p.x,p.y,p.z+float(pawn.base_eye_height)],range=float(anchor.request_range),
                distance=math.dist(_xyz(p),_xyz(anchor.get_actor_location())),
                native_admission=str(admission),native_action_text=self.report['last_interaction']['native_action_text'],source='Native CanInteract retains the exact eye-to-anchor LOS/range gate')
        self.inject(look=look)
        if error >= 3. or admission is None or focus != component:
            return
        self.unbind_request()
        self.hold_actor,self.held_kind = actor,kind
        self.hold_seconds,self.hold_started = float(component.interaction_time),unreal.GameplayStatics.get_time_seconds(self.world)
        self.saw_countdown,self.request_result = False,None
        if kind == 'handoff':
            def result(accepted,message):
                row = dict(beat=HANDOFF,accepted=accepted,message=str(message),elapsed=time.monotonic()-self.started)
                self.report['request_results'].append(row);self.request_result=row
            self.request_callback,self.request_delegate=result,actor.on_request_result
            self.request_delegate.add_callable(self.request_callback)
        self.stage('hold_'+kind)

    def hold_special(self, pc, state):
        remaining = float(pc.get_interaction_component().get_editor_property('remaining_interact_time'))
        if 0. < remaining <= self.hold_seconds+.001:
            self.saw_countdown = True
        if self.held_kind == 'handoff':
            accepted = (self.hold_actor.is_request_pending() or self.request_result is not None
                or pc.get_campaign_transition_state() == unreal.SovCampaignTransitionState.SWITCHING
                or state.is_beat_complete(unreal.Name(MISSION),unreal.Name(HANDOFF)))
        else:
            accepted = self.priority.is_request_pending() or self.selected_choice(state) != 'None'
        if accepted:
            self.inject()
            assert self.saw_countdown, 'The actual ordinary hold countdown was not observed'
            if self.request_result is not None:
                assert self.request_result['accepted'], 'Native handoff request rejected: '+self.request_result['message']
            self.report['holds'].append(dict(beat=HANDOFF if self.held_kind=='handoff' else SELECTED,
                native_countdown=True,seconds=self.hold_seconds,
                input_game_seconds=unreal.GameplayStatics.get_time_seconds(self.world)-self.hold_started,actor=_path(self.hold_actor)))
            self.stage('wait_'+self.held_kind)
        else:
            assert time.monotonic()-self.phase_at < 8., 'Hold did not reach its native request owner: '+json.dumps(self.report['last_interaction'])
            self.inject(interact=1.)

    def confirm_priority(self, state, events):
        assert self.selected_choice(state) in ('None',SELECTED), 'The other exclusive route was selected'
        if self.selected_choice(state) != SELECTED or not state.is_beat_complete(unreal.Name(MISSION),unreal.Name(PRIORITY)):
            return False
        assert [e['beat'] for e in events] == FINAL and not state.is_beat_complete(unreal.Name(MISSION),unreal.Name(REJECTED))
        if self.priority.is_request_pending():
            return False
        # WriteCheckpoint also queues an autosave with the same boundary ID. Its
        # later callback is a separate bank kind, not an explicit-choice replay.
        relevant = [s for s in self.report['saves'] if s['checkpoint'] and s['boundary'] in ('Aurelion.CP4b','Aurelion.CP5')]
        assert all(s['succeeded'] for s in relevant), 'Native priority checkpoint failed: '+json.dumps(relevant)
        before = [s for s in relevant if s['boundary']=='Aurelion.CP4b']
        after = [s for s in relevant if s['boundary']=='Aurelion.CP5']
        if not before or not after:
            return False
        assert len(before) == len(after) == 1, 'Priority checkpoint replay occurred unexpectedly'
        assert before[0]['journal_beats'] == FINAL[:-2] and after[0]['journal_beats'] == FINAL
        assert before[0]['generation'] < after[0]['generation']
        assert all(s['mission'] == MISSION and s['map']=='/Game/Aurelion/Maps/L_Aurelion_M12' for s in relevant)
        assert all(s['slot_index']==0 and s['explicit_boundary'] for s in relevant)
        records = [c for e in state.get_journal() if str(e.beat_id)==SELECTED for c in e.consequences]
        assert len(records)==1
        r,d=records[0],records[0].definition
        assert str(d.consequence_id)=='M12_'+CHOICE+'Prioritized' and str(r.resolved_instigator_id)=='Selene'
        assert {str(v) for v in d.subject_ids}=={'Aurelion_'+CHOICE}
        assert {str(v) for v in d.witness_ids}=={'Tarrik','Selene'} and d.publicity==unreal.SovRecordPublicity.SHARED
        assert {str(v) for v in d.consumer_ids}=={'M12_PriorityEvacuation','M13_PriorityAftermath'}
        assert tag_name(d.choice_tag)=='Campaign.Aurelion.Choice.EvacuationPriority'
        assert tag_name(d.outcome_tag)=='Campaign.Aurelion.Outcome.'+CHOICE+'First'
        west = CHOICE == 'WestStretchers'
        opened, closed = (self.support.west_cache_barrier, self.support.east_flank_barrier) if west else (self.support.east_flank_barrier, self.support.west_cache_barrier)
        if not (self.support.get_priority()==(unreal.SovAurelionRescuePriority.WEST_STRETCHERS if west else unreal.SovAurelionRescuePriority.EAST_WALKERS)
            and self.support.is_west_cache_accessible()==west and self.support.is_east_flank_open()==(not west)
            and opened.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            and closed.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS):
            return False
        assert str(self.support.get_aftermath_consequence_id())=='M12_'+CHOICE+'Prioritized'
        self.report['choice']=dict(selected=SELECTED,rejected_alternative_not_committed=REJECTED,
            receipts=events[-2:],consequence=r.export_text(),native_save_callbacks=relevant,
            support=self.support_state(),cache_used=False,readback='Native successful write callbacks; no reload was requested')
        return True

    def tick(self, delta):
        if self.done:
            return
        try:
            now=time.monotonic()
            assert now-self.started<900.,'Gallery/E4 entry continuation exceeded fifteen-minute bound'
            assert now-self.phase_at<(240. if self.phase=='walk_route' else 100.),'Stage deadline: '+self.phase
            world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world and '/Aurelion/Maps/UEDPIE_' in world.get_path_name() and 'L_Aurelion_M12' in world.get_name()
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            assert isinstance(pc,unreal.SovPlayerController)
            state=pc.get_campaign_state()
            assert state and state.get_active_mission() and str(state.get_active_mission().mission_id)==MISSION
            events=entry.journal(state)
            assert len(events)<=len(FINAL) and [e['beat'] for e in events]==FINAL[:len(events)],'Unexpected journal order or alternative choice'
            assert all(e['mission']==MISSION for e in events)
            if self.phase=='initialize':
                self.initialize(world,pc,pawn,state,events)
            assert world==self.world and pc==self.initial_controller,'World/controller ownership changed'
            assert events[:len(self.initial_events)]==self.initial_events,'Existing receipt GUIDs/content changed'
            assert self.e3.get_encounter_state()==unreal.SovEncounterState.SUCCEEDED and self.e3.has_confirmed_victory()
            assert self.e4b.get_encounter_state()==unreal.SovEncounterState.INACTIVE and not list(self.e4b.participants)
            assert self.e4.get_encounter_state() in (unreal.SovEncounterState.INACTIVE,unreal.SovEncounterState.ACTIVE)
            if self.e4.get_encounter_state()==unreal.SovEncounterState.ACTIVE:
                assert self.phase=='wait_e4_entry' or (self.phase=='walk_route' and self.next_route_state=='wait_e4_entry'), 'E4A activated before the final approach'
            assert not self.e4.has_confirmed_victory() and not state.is_beat_complete(unreal.Name(MISSION),unreal.Name(E4_BEAT))
            rows=self.roster_for(self.e4)
            assert len(rows)==12 and all(r['alive'] and r['actor']==self.e4_paths[r['id']] for r in rows),'E4 roster retired, changed identity or suffered a pre-entry death'
            assert not self.saves.is_awaiting_failure_decision(),'A native save needs a failure decision; no acknowledgment/retry was issued'
            assert not self.saves.is_load_pending() and not self.saves.is_mission_travel_pending()
            assert pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.FAILED,'Native handoff failed'
            if self.scene_component is not None:
                if self.observe_scene() in (unreal.SovCinematicPhase.PLAYING, unreal.SovCinematicPhase.COMPLETED):
                    self.clear_scene_retry()
            if self.request_result is not None:
                assert self.request_result['accepted'],'Native request rejected: '+self.request_result['message']
            if not pawn:
                assert self.phase in ('hold_handoff','wait_handoff'),'Unexpected absent pawn'
                self.inject();return
            assert isinstance(pawn,unreal.SovPlayerCharacterBase) and rescue.alive(pawn),'The controlled player died; no recovery was issued'
            if self.phase not in ('hold_handoff','wait_handoff'):
                assert _path(pawn)==self.current_pawn_path,'Controlled pawn changed outside the native handoff'
                assert rescue.alive(self.companion) and _path(self.companion)==self.current_companion_path
                assert self.companion.get_owner()==pc and not self.companion.get_editor_property('hidden')
                assert not self.companion.get_companion_component().is_disabled()
            if now-self.last_sample>.5:
                self.last_sample=now
                self.report['samples'].append(dict(elapsed=now-self.started,phase=self.phase,pawn=_path(pawn),
                    position=_xyz(pawn.get_actor_location()),health=pawn.get_health(),ready=pawn.is_character_ready(),
                    transition=str(pc.get_campaign_transition_state()),journal=events,e4=str(self.e4.get_encounter_state()),
                    e4_roster=rows,selected_choice=self.selected_choice(state),support=self.support_state()))
            if now-self.last_write>1.:
                self.last_write=now;self.write()
            if self.phase=='hold_scene':
                self.hold_use(pc);return
            if self.phase in ('hold_handoff','hold_priority'):
                self.hold_special(pc,state);return
            if self.phase=='wait_scene':
                self.inject();self.finish_scene(state,events);return
            if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world):
                self.inject();return
            if pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.IDLE:
                self.inject();return
            if self.phase=='wait_grounding_exit':
                self.inject()
                if not self.unique(unreal.SovAurelionJournalGate,'beat_id',rescue.LYRIC).is_blocking_route():
                    self.select_scene(DOMINION,[(0.,11200.,-510.),(0.,11620.,-515.),(0.,12200.,-660.),
                        (0.,12850.,-810.),(0.,13800.,-810.),(0.,14200.,-810.)])
            elif self.phase=='walk_route':
                if self.next_route_state=='wait_e4_entry' and self.e4.get_encounter_state()==unreal.SovEncounterState.ACTIVE:
                    self.inject();self.stage('wait_e4_entry')
                else:
                    self.walk(pc,pawn)
            elif self.phase=='aim_scene':
                self.aim_use(pc,pawn,self.requests[self.scene_beat],'scene')
            elif self.phase=='wait_scene_view':
                self.inject()
                beat=self.scene_beat
                if beat in self.cage_gates and not self.cage_view(beat):
                    return
                self.leave_scene()
                if beat==DOMINION:
                    p=self.handoff_request.get_actor_location()
                    self.begin_route([(p.x+20.,p.y-190.,p.z+20.)],'aim_handoff')
                elif beat==REFORMATION:
                    self.select_scene(SHARING)
                else:
                    p=self.priority.get_actor_location()
                    self.begin_route([(p.x+20.,p.y-190.,p.z+29.)],'aim_priority')
            elif self.phase=='aim_handoff':
                self.aim_special(pc,pawn,'handoff')
            elif self.phase=='wait_handoff':
                self.inject()
                if len(events)==FINAL.index(HANDOFF)+1 and isinstance(pawn,unreal.SovSeleneCharacter) and _path(pawn)!=self.initial_pawn:
                    assert entry.valid_guid(events[-1]['handoff']) and events[-1]['anchor']=='M12_SeleneCage'
                    companion=self.owned_companion(pc,pawn,'Tarrik')
                    if companion is None:
                        return
                    assert _path(companion)!=self.current_companion_path
                    self.companion=companion
                    self.current_pawn_path,self.current_companion_path=_path(pawn),_path(companion)
                    self.report['handoff']=dict(receipt=events[-1],pawn=_path(pawn),companion=_path(companion),
                        position=_xyz(pawn.get_actor_location()),ready=True,health=pawn.get_health(),
                        public_current_leader_admission=True)
                    self.select_scene(REFORMATION)
            elif self.phase=='aim_priority':
                self.aim_special(pc,pawn,'priority')
            elif self.phase=='wait_priority':
                self.inject()
                self.report['last_priority']=dict(pending=self.priority.is_request_pending(),
                    message=str(self.priority.last_result),selected=self.selected_choice(state))
                if self.confirm_priority(state,events):
                    self.stage('wait_priority_gate')
                elif now-self.phase_at>2. and not self.priority.is_request_pending() and str(self.priority.last_result):
                    raise AssertionError('Native priority transaction did not supply every required checkpoint/choice/support proof: '+json.dumps(self.report['last_priority']))
            elif self.phase=='wait_priority_gate':
                self.inject()
                if not self.unique(unreal.SovAurelionJournalGate,'beat_id',PRIORITY).is_blocking_route():
                    self.begin_route([(0.,15900.,-810.),(0.,16120.,-815.),(0.,16700.,-960.),
                        (0.,17350.,-1110.),(0.,17900.,-1110.),(0.,18450.,-1110.),(0.,18870.,-1110.)],'wait_e4_entry')
            elif self.phase=='wait_e4_entry':
                self.inject()
                if self.e4.get_encounter_state()!=unreal.SovEncounterState.ACTIVE:
                    return
                attempt=self.e4.get_attempt_id().export_text()
                assert entry.valid_guid(attempt) and self.e4.has_encounter_player(pawn)
                assert self.e4_entry.start_volume.is_overlapping_component(pawn.get_editor_property('capsule_component'))
                assert self.coordination.get_current_wave()==0
                assert {r['id'] for r in rows if r['required'] and not r['hidden']}==HOSTILES-{'E4.WallRunner'}
                assert {r['id'] for r in rows if r['required'] and r['hidden']}=={'E4.WallRunner'}
                assert {r['id'] for r in rows if not r['required']}==PROTECTED and all(r['alive'] for r in rows)
                assert [e['beat'] for e in events]==FINAL and self.selected_choice(state)==SELECTED
                assert not self.e4_entry.is_result_pending() and self.report['choice'] is not None
                self.report['e4_entry']=dict(attempt=attempt,physical_overlap=True,native_wave=0,roster=rows,
                    player=_path(pawn),companion=_path(self.companion),journal=events,support=self.support_state(),
                    no_sever_or_thermal_journal_receipt=True,phase_b_inactive=True)
                self.finish(True,'Three complete native scenes, the real Selene handoff and exclusive '+CHOICE+' priority with CP4b/CP5 success led to genuine E4A initial entry. All seven protected people are alive; four enemies are released and WallRunner remains reserved. E4 combat and every later beat remain unqualified.')
        except Exception:
            self.report['error']=traceback.format_exc()
            self.finish(False,self.report['error'])


def start(output_directory=None):
    """Run only after the real GroundLyric completion and all earlier input drivers stop."""
    global _RUN
    assert _RUN is None or _RUN.done,'E4 entry driver is already running'
    for name in ('continue_aurelion_e1_input','continue_aurelion_e2_input','continue_aurelion_e3_entry_input','continue_aurelion_e3_rescue_input'):
        module=sys.modules.get(name)
        run=getattr(module,'_RUN',None) if module else None
        assert run is None or run.done,'Stop the earlier input driver first: '+name
    output=Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (output/'e4-entry-input-continuation.json').exists(),'Use a new evidence directory'
    _RUN=Run(output);_RUN.write()
    _RUN.handle=unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False,'Stopped by operator; no save acknowledgment, retry or progression repair was issued')


if __name__=='__main__':
    start()
