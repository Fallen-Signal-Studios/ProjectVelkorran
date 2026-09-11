"""Earned M13 entry -> contrary position, all native scenes/handoffs and departure.

Import is inert. Ordinary movement/look/Interact are the only gameplay inputs.
The native companion, cinematic, transit, campaign and storage owners remain
the only sources of movement, proof, evidence and checkpoint results.
"""
import json
import math
from pathlib import Path
import sys
import time
import traceback
import unreal
import continue_aurelion_e1_input as common
import continue_aurelion_e3_entry_input as entry
import continue_aurelion_e3_rescue_input as rescue
import continue_aurelion_e4_entry_input as gallery
import continue_aurelion_m13_entry_input as prior

_RUN = None
MISSION = 'M13_ContraryWitness'
INITIAL = list(prior.FINAL)
BEATS = ['ContraryPosition', 'TarrikIndependentAssent', 'HandoffToSeleneAssent',
    'SeleneIndependentAssent', 'MeridianContainment', 'FifthWitness', 'GrammarPropagation',
    'VoluntaryStay', 'CauldronRecorderReceived', 'HandoffToTarrikAftermath',
    'Record7283Received', 'ContainmentPact', 'SeparateDepartures']
FINAL = INITIAL + BEATS
HANDOFFS = {'HandoffToSeleneAssent': ('Tarrik','Selene','M13_SeleneAssent'),
            'HandoffToTarrikAftermath': ('Selene','Tarrik','M13_TarrikAftermath')}
SCENES = [b for b in BEATS if b != 'ContraryPosition' and b not in HANDOFFS]
F = 'Campaign.Aurelion.M13.Fact.'
V = 'Campaign.Aurelion.Value.'
H = 'Sov.Character.Player.'
FACTS = {
    'TarrikIndependentAssent': {F+'TarrikAssent': V+'Independent'},
    'SeleneIndependentAssent': {F+'SeleneAssent': V+'Independent'},
    'MeridianContainment': {F+'Meridian': V+'Complete', F+'Containment': V+'Stabilized',
        F+'TerminalAuthority': V+'Available', F+'PrisonRelease': 'Sov.Campaign.Value.Withheld',
        F+'PrisonBoundary': V+'Closed', F+'Crownmark': V+'Integrated',
        F+'LyricLife': V+'Alive', F+'LyricCorruption': V+'Unreversed'},
    'GrammarPropagation': {F+'ContainmentGrammar': V+'Propagated'},
    'VoluntaryStay': {F+'SharedStay': V+'Voluntary'},
    'CauldronRecorderReceived': {F+'CauldronRecorderRecipient': H+'Selene'},
    'Record7283Received': {F+'Record7283Recipient': H+'Tarrik'},
    'ContainmentPact': {F+'ContainmentPact': V+'DirectContraryConcurrenceRequired'},
    'SeparateDepartures': {F+'Departure': V+'SeparateDepartures'},
}
EVIDENCE = {'FifthWitness': ('Aurelion_FifthWitness','AurelionTerminal',['Selene','Tarrik']),
            'CauldronRecorderReceived': ('CauldronRecorder','Tarrik',[]),
            'Record7283Received': ('Record7283','Selene',[])}
_path, _xyz, _optional = common._path, common._xyz, common._optional


def journal(state):
    rows = entry.journal(state)
    for row, item in zip(rows, state.get_journal()):
        row.update(protagonist=item.protagonist.export_text(), handoff_to=item.handoff_to_protagonist.export_text(),
            coaction=item.co_action_request_id.export_text(), coaction_companion=str(item.co_action_companion_id),
            coaction_anchor=str(item.co_action_anchor_id), raw=item.export_text())
    return rows


def evidence(state):
    return [dict(id=str(e.evidence_id), definition=_path(e.definition), stage=str(e.stage),
        source=e.source_id.export_text(), event=e.critical_beat_event_id.export_text(),
        location=str(e.source_location_id), custodian=str(e.custodian_id),
        witnesses=sorted(str(w) for w in e.witness_ids), publicity=str(e.publicity),
        protagonist=e.protagonist.export_text(), mission=str(e.mission_id), beat=str(e.acquisition_beat),
        sequence=e.after_journal_sequence, supporting=str(e.supporting_evidence_id),
        copy_destination=str(e.copy_destination), knowledge=e.granted_knowledge.export_text(), raw=e.export_text())
        for e in state.get_evidence()]



def expected_dialogue_indices(state, cues):
    # Read the same saved choice as native PrioritySupport::ReadPriority; that
    # static function is not reflected. GetSelectedChoice is BlueprintPure.
    selected = str(state.get_selected_choice(unreal.Name('M12_FireAndFrost'), unreal.Name('ImmediateProtection')))
    priorities = unreal.SovAurelionRescuePriority
    priority = {'PriorityWestStretchers': priorities.WEST_STRETCHERS,
                'PriorityEastWalkers': priorities.EAST_WALKERS}.get(selected, priorities.UNSET)
    conditional = any(cue.required_priority != priorities.UNSET for cue in cues)
    assert not conditional or priority != priorities.UNSET, 'Conditional dialogue requires the actual persisted M12 priority'
    expected = {index for index, cue in enumerate(cues)
                if cue.required_priority == priorities.UNSET or cue.required_priority == priority}
    return expected, selected


def movement_base(actor):
    movement = actor.get_component_by_class(unreal.CharacterMovementComponent)
    assert movement is not None
    # CharacterMovementComponent.h: BlueprintCallable, delegates to Character's actual base.
    return movement.get_movement_base()


def feet(actor):
    p = actor.get_actor_location()
    capsule = actor.get_component_by_class(unreal.CapsuleComponent)
    assert capsule is not None
    return [p.x,p.y,p.z-capsule.get_scaled_capsule_half_height()]


class Run(rescue.Run):
    def __init__(self, output_directory):
        super().__init__(output_directory)
        self.saves = self.game_instance = self.lift = None
        self.save_delegate = self.save_callback = None
        self.checkpoints, self.gates, self.evidence_seen = {}, {}, {}
        self.player_identity, self.companion_identity = 'Tarrik','Selene'
        self.current_player = self.current_companion = None
        self.current_companion_guid = self.player_state_guid = None
        self.held_kind = self.held_beat = None
        self.handoff_before = self.lift_origin = None
        self.observed_journal = []
        self.boarded_since = self.departure_since = self.lift_completed_at = None
        self.report.update(scope='Actual travelled M13: thirteen native receipts, physical paired lift, three automatic checkpoints and separate departures',
            entry_requires=['actual prior M12-to-M13 normal-input travel result passed',
                'same travelled M13 world/controller, ready living Tarrik and actual Selene companion',
                'unchanged 22 M12 native journal receipts; no pre-existing M13 receipts/evidence',
                'standard native hold settings and available platform storage owner',
                'authored physical co-action nav-feet mark, complete navigation and paired lift boarding'],
            pending=['Explicit final checkpoint reload and old checkpoint compatibility',
                'Physical keyboard operation and rendered dialogue/camera quality',
                'Private protagonist ledger byte equality; public identities, resource profiles, inventory and evidence are observed'],
            scene_phases=[], dialogue_cues=[], request_results=[], completed_scenes={}, facts={},
            saves=[], handoffs=[], evidence=[], identity_epochs=[], coaction=[], lift=[], checkpoints={})
        for unused in ('door_states','waves','native_combat_victory'):
            self.report.pop(unused,None)

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started,3)
        temp = self.out/'m13-input-continuation.tmp'
        temp.write_text(json.dumps(self.report,indent=2,default=str),encoding='utf8')
        common.replace_report_with_retry(temp, self.out/'m13-input-continuation.json')

    def finish(self, passed, reason):
        if self.done: return
        if self.save_delegate is not None and self.save_callback is not None:
            _optional(lambda:self.save_delegate.remove_callable(self.save_callback))
        self.save_delegate = self.save_callback = None
        super().finish(passed,reason)
        self.saves = self.game_instance = self.lift = None
        self.current_player = self.current_companion = None
        self.checkpoints.clear(); self.gates.clear()
        self.report['retained_gameplay_references_cleared'] = True
        self.write()

    def companion_for(self,pc,pawn,identity,admit=False):
        values = [a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world,unreal.SovProtagonistCompanionCharacter)
            if a.get_owner()==pc and rescue.alive(a) and not a.get_editor_property('hidden')]
        if not values: return None
        assert len(values)==1, 'Ambiguous actual controller-owned companion'
        actor = values[0]
        assert gallery.tag_name(actor.get_companion_identity())==H+identity
        component = actor.get_companion_component()
        assert component and not component.is_disabled()
        if admit and component.can_request_command(pawn,unreal.SovCompanionCommand.REGROUP,pawn) is None:
            return None
        return actor

    def profile(self,pc,pawn,companion):
        profile = dict(player_state_guid=prior.stable_id(pc.player_state),
            player=dict(actor=_path(pawn),identity=gallery.tag_name(pawn.get_protagonist_identity_tag()),
                cls=_path(pawn.get_class()),resources=prior.resources(pawn),items=prior.items(pawn)),
            companion=dict(actor=_path(companion),identity=gallery.tag_name(companion.get_companion_identity()),
                guid=prior.stable_id(companion),cls=_path(companion.get_class()),resources=prior.resources(companion),items=prior.items(companion)))
        for key,actor in (('player',pawn),('companion',companion)):
            asc=actor.get_narrative_ability_system_component()
            assert asc and asc.get_avatar_owner()==actor
            profile[key]['asc']=dict(actor=_path(asc),avatar=_path(asc.get_avatar_owner()),ready_epoch=asc.get_character_ready_epoch())
        return profile

    def facts_for(self,state,beat):
        definition = next(b for b in state.get_active_mission().beats if str(b.beat_id)==beat)
        rows = [dict(key=gallery.tag_name(w.key),expected=gallery.tag_name(w.value),
            actual=gallery.tag_name(state.get_state_value(w.key)),protected=bool(w.canon_protected)) for w in definition.state_writes]
        assert {r['key']:r['expected'] for r in rows}==FACTS.get(beat,{}), 'Authored canonical facts changed: '+beat
        assert all(r['expected']==r['actual'] and r['protected'] for r in rows)
        return rows

    def check_evidence(self,state,events):
        rows = evidence(state)
        assert len({r['source'] for r in rows})==len(rows)
        for row in rows:
            assert entry.valid_guid(row['source'])
            if row['source'] in self.evidence_seen:
                assert row==self.evidence_seen[row['source']], 'An existing provenance record was changed'
            else:
                assert row['mission']==MISSION and row['beat'] in EVIDENCE, 'Unexpected new evidence'
                eid,custodian,witnesses = EVIDENCE[row['beat']]
                receipt = next(e for e in events if e['mission']==MISSION and e['beat']==row['beat'])
                definition = next(b for b in state.get_active_mission().beats if str(b.beat_id)==row['beat']).critical_evidence[0]
                assert row['id']==eid and row['definition']=='/Game/Aurelion/Evidence/DA_'+eid+'.DA_'+eid
                assert str(definition.original_custodian)==custodian and str(definition.canonical_content_id)==eid
                assert row['stage']==str(unreal.SovEvidenceStage.OBSERVED)
                assert row['event']==receipt['id'] and row['sequence']==receipt['sequence']
                assert row['location']==row['beat'] and row['custodian']==custodian and row['witnesses']==witnesses
                assert row['protagonist']==receipt['protagonist']
                assert row['supporting']=='None' and row['copy_destination']=='None'
                assert row['publicity']==str(unreal.SovRecordPublicity.SHARED if witnesses else unreal.SovRecordPublicity.PRIVATE)
                self.evidence_seen[row['source']] = row
        assert set(self.evidence_seen)=={r['source'] for r in rows}, 'Previously observed evidence disappeared'
        for beat,(eid,_,witnesses) in EVIDENCE.items():
            acquired = any(e['mission']==MISSION and e['beat']==beat for e in events)
            found = [r for r in rows if r['mission']==MISSION and r['beat']==beat]
            assert len(found)==int(acquired), 'Evidence and earned cinematic receipt disagree'
            if acquired:
                receipt = next(e for e in state.get_journal() if str(e.mission_id)==MISSION and str(e.beat_id)==beat)
                assert state.knows_evidence(unreal.Name(eid),receipt.protagonist)
                assert all(state.observer_knows_evidence(unreal.Name(eid),unreal.Name(w)) for w in witnesses)
        self.report['evidence'] = rows

    def initialize(self,world,pc,pawn,state,events):
        previous = prior._RUN
        assert previous and previous.done and previous.report['status']=='passed', 'Require the actual prior M12-to-M13 travel driver result'
        qualified = previous.report['native_travel']
        assert [_path(world),_path(pc),_path(pawn)]==[qualified['world'],qualified['controller'],qualified['pawn']]
        assert entry.journal(state)==qualified['journal'] and [e['beat'] for e in events]==INITIAL
        assert isinstance(pawn,unreal.SovTarrikCharacter) and pawn.is_character_ready() and rescue.alive(pawn)
        assert state.is_state_valid() and state.is_mission_complete(unreal.Name(prior.MISSION)) and not state.is_mission_complete(unreal.Name(MISSION))
        assert pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.IDLE
        self.world,self.initial_controller,self.initial_pawn = world,pc,_path(pawn)
        self.owner = self.get_input_owner(world)
        self.game_instance = unreal.GameplayStatics.get_game_instance(world)
        self.companion = self.companion_for(pc,pawn,'Selene',True)
        assert self.companion and _path(self.companion)==qualified['companion']
        settings = unreal.GameUserSettings.get_game_user_settings().get_settings_snapshot()
        assert not settings.tap_interactions and abs(settings.interaction_hold_scale-1.)<.001
        self.initial_events = events
        self.observed_journal = list(events)
        self.current_player,self.current_companion = _path(pawn),_path(self.companion)
        self.player_state_guid,self.current_companion_guid = prior.stable_id(pc.player_state),prior.stable_id(self.companion)
        source = previous.report['snapshot_comparisons']['after']
        actual = self.profile(pc,pawn,self.companion)
        assert actual['player_state_guid']==source['player_state_guid'] and actual['companion']['guid']==source['companion']['guid']
        assert actual['player']['items']==source['player']['items'] and actual['companion']['items']==source['companion']['items']
        assert abs(actual['player']['resources']['echo']-source['player']['resources']['echo'])<.01
        assert abs(actual['companion']['resources']['echo']-source['companion']['resources']['echo'])<.01
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionRequestActor):
            if str(actor.mission_id)!=MISSION: continue
            beat = str(actor.beat_id)
            assert beat in BEATS and beat not in self.requests
            self.requests[beat] = actor
            expected = (unreal.SovAurelionRequest.HANDOFF if beat in HANDOFFS else
                unreal.SovAurelionRequest.CO_ACTION if beat=='ContraryPosition' else unreal.SovAurelionRequest.PLAY_SCENE)
            assert actor.operation==expected and abs(actor.interactable.interaction_time-.35)<.001
        assert set(self.requests)==set(BEATS)
        assert [str(b.beat_id) for b in state.get_active_mission().beats]==BEATS
        assert not state.get_active_mission().completes_campaign
        anchor = self.requests['ContraryPosition'].co_action_anchor
        assert anchor and str(anchor.anchor_id)=='M13_SeleneContraryPosition' and str(anchor.required_companion_id)=='Selene'
        assert anchor.hidden_fallback_anchor is None, 'A hidden correction cannot qualify the physical companion path'
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionCheckpoint):
            boundary = str(actor.get_boundary_id())
            if boundary in ('Aurelion.CP7','Aurelion.CP8','Aurelion.CP9'):
                assert boundary not in self.checkpoints and actor.capture_on_overlap
                self.checkpoints[boundary] = actor
        assert len(self.checkpoints)==3
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionJournalGate):
            if str(actor.mission_id)==MISSION:
                beat = str(actor.beat_id)
                assert beat in ('GrammarPropagation','ContainmentPact') and beat not in self.gates
                self.gates[beat] = actor
        assert len(self.gates)==2
        self.lift = self.unique(unreal.SovWorldTransitActor,'transit_id','M13_AurelionExitLift')
        assert self.lift.kind==unreal.SovWorldTransitKind.LIFT and self.lift.require_mission_companion_aboard
        assert not self.lift.irreversible_transition and abs(self.lift.travel_seconds-8.)<.001
        assert _xyz(self.lift.destination_offset)==[0.,0.,1800.]
        assert self.lift.get_transit_state()==unreal.SovWorldTransitState.AT_ORIGIN
        saves = [s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==self.game_instance]
        assert len(saves)==1 and saves[0].is_platform_storage_owner_available()
        self.saves = saves[0]
        def saved(result,slot,message):
            current = pc.get_campaign_state()
            self.report['saves'].append(dict(result=str(result),succeeded=result==unreal.SovSaveResult.SUCCESS,
                checkpoint=slot.kind==unreal.SovSaveSlotKind.CHECKPOINT,boundary=str(slot.boundary_id),
                boundary_kind=str(slot.boundary_kind),generation=slot.generation,slot_index=slot.slot_index,
                mission=str(slot.mission_id),map=str(slot.map_package),message=str(message),
                journal=journal(current),elapsed=time.monotonic()-self.started))
        self.save_callback,self.save_delegate = saved,self.saves.on_save_completed
        self.save_delegate.add_callable(saved)
        self.evidence_seen = {r['source']:r for r in evidence(state)}
        assert not any(r['mission']==MISSION for r in self.evidence_seen.values()), 'M13 evidence predates actual M13 play'
        self.report['initial'] = dict(world=_path(world),controller=_path(pc),journal=events,profile=actual,
            prior_travel_token=qualified['request_token'],settings=settings.export_text(),evidence=evidence(state))
        self.report['identity_epochs'].append(actual)
        # Intersection of request's range and the actual co-action mark's range.
        self.begin_route([(0.,32100.,-1710.),(0.,33800.,-1710.),(-20.,34330.,-1710.)],'aim_coaction')

    def select_scene(self,beat,route=None):
        gallery.Run.select_scene(self,beat,route)
        if beat=='SeleneIndependentAssent':
            # Replace only this queued QA approach before any input frame.
            # The original south offset clips the authored dais edge.
            p=self.scene.get_actor_location()
            self.begin_route(list(route or [])+[(p.x-40.,p.y,p.z)],'aim_scene')

    def leave_scene(self):
        gallery.Run.leave_scene(self)

    def aim_request(self,pc,pawn,beat,kind):
        actor = self.lift if kind=='lift' else self.requests[beat]
        interaction,component = pc.get_interaction_component(),actor.interactable
        look,error = self.look(self.world,pc,actor.get_actor_location())
        admitted = component.can_interact(pawn,interaction)
        focus = interaction.get_editor_property('viewed_interactable')
        self.report['last_interaction'] = dict(kind=kind,beat=beat,actor=_path(actor),focus=_path(focus),
            admitted=admitted is not None,admission=str(admitted),native_action_text=str(component.get_interactable_action_text(pawn,interaction)),angle_error=error,
            player=_xyz(pawn.get_actor_location()),target=_xyz(actor.get_actor_location()),
            range=float(component.interaction_distance),hold=float(component.interaction_time))
        self.inject(look=look)
        # An admitted request can lose the registry score to the nearby companion.
        # Reposition through the existing complete-path walker; never force focus.
        # Lift admission retains both actual riders on the moving body.
        if kind in ('coaction','handoff'):
            retry = self.report.setdefault('focus_repositions', {}).setdefault(_path(actor),
                dict(attempts=0, blocked_since=None, routes=[]))
            focus_owner = focus.get_owner() if focus is not None and unreal.SystemLibrary.is_valid(focus) else None
            npc_blocked = admitted is not None and error < 1. and focus != component and isinstance(focus_owner, unreal.NarrativeNPCCharacter)
            if npc_blocked and retry['attempts'] < 2:
                now = time.monotonic()
                if retry['blocked_since'] is None: retry['blocked_since'] = now
                if now-retry['blocked_since'] >= 2.:
                    point, position = actor.get_actor_location(), pawn.get_actor_location()
                    points = ([(point.x-130.,point.y-180.,position.z),(point.x,point.y-160.,position.z)]
                        if retry['attempts']==0 else [(point.x+180.,point.y-130.,position.z),(point.x+160.,point.y,position.z)])
                    retry['attempts'] += 1
                    retry['blocked_since'] = None
                    retry['routes'].append(dict(focus=_path(focus),npc=_path(focus_owner),points=points,
                        elapsed=now-self.started,reason='Native request admitted but nearby NPC owns interaction focus'))
                    resume_phase = self.phase
                    self.inject()
                    self.begin_route(points,resume_phase)
                    return
            else:
                retry['blocked_since'] = None
        if error>=3. or admitted is None or focus!=component: return
        self.unbind_request()
        self.hold_actor,self.held_kind,self.held_beat = actor,kind,beat
        self.hold_seconds,self.hold_started = float(component.interaction_time),unreal.GameplayStatics.get_time_seconds(self.world)
        self.saw_countdown,self.request_result = False,None
        if kind!='lift':
            def result(accepted,message):
                row = dict(beat=beat,accepted=accepted,message=str(message),elapsed=time.monotonic()-self.started)
                self.report['request_results'].append(row); self.request_result=row
            self.request_callback,self.request_delegate = result,actor.on_request_result
            self.request_delegate.add_callable(result)
        if kind=='handoff':
            self.handoff_before = self.profile(pc,pawn,self.companion)
        elif kind=='lift':
            assert movement_base(pawn)==self.lift.moving_body and movement_base(self.companion)==self.lift.moving_body
            self.lift_origin = dict(body=_xyz(self.lift.moving_body.get_world_location()),
                player=_xyz(pawn.get_actor_location()),companion=_xyz(self.companion.get_actor_location()))
        self.stage('hold_request')

    def hold_request(self,pc,state):
        remaining = float(pc.get_interaction_component().get_editor_property('remaining_interact_time'))
        if 0.<remaining<=self.hold_seconds+.001: self.saw_countdown=True
        kind = self.held_kind
        accepted = (self.lift.get_transit_state()!=unreal.SovWorldTransitState.AT_ORIGIN if kind=='lift' else
            self.hold_actor.is_request_pending() or self.request_result is not None
            or state.is_beat_complete(unreal.Name(MISSION),unreal.Name(self.held_beat))
            or (kind=='handoff' and pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.SWITCHING))
        if accepted:
            self.inject()
            assert self.saw_countdown, 'Actual native Interact hold countdown was not observed'
            if self.request_result is not None: assert self.request_result['accepted'], self.request_result['message']
            self.report['holds'].append(dict(beat=self.held_beat,kind=kind,native_countdown=True,seconds=self.hold_seconds,
                input_game_seconds=unreal.GameplayStatics.get_time_seconds(self.world)-self.hold_started))
            self.stage('wait_'+kind)
        else:
            assert time.monotonic()-self.phase_at<8., 'Native hold did not dispatch: '+json.dumps(self.report['last_interaction'])
            self.inject(interact=1.)

    def confirm_coaction(self,state,events):
        anchor = self.requests['ContraryPosition'].co_action_anchor
        component = self.companion.get_companion_component()
        at = _xyz(anchor.companion_mark.get_world_location())
        row = dict(elapsed=time.monotonic()-self.started,feet=feet(self.companion),mark=at,
            distance=math.dist(feet(self.companion),at),reach=anchor.reach_radius,
            command=str(component.get_command_state()),actor=_path(self.companion))
        self.report['coaction'].append(row)
        assert component.get_command_state()!=unreal.SovCompanionCommandState.FAILED, 'Native contrary-position movement failed'
        self.inject()
        if not state.is_beat_complete(unreal.Name(MISSION),unreal.Name('ContraryPosition')): return
        receipt=events[-1]
        assert receipt['beat']=='ContraryPosition' and len(events)==23 and entry.valid_guid(receipt['coaction'])
        assert receipt['coaction_companion']=='Selene' and receipt['coaction_anchor']=='M13_SeleneContraryPosition'
        assert row['distance']<=row['reach']+.5, 'Receipt was not observed at the physical nav-feet mark'
        assert self.request_result and self.request_result['accepted']
        self.report['native_coaction'] = dict(receipt=receipt,arrival=row)
        self.select_scene('TarrikIndependentAssent')

    def finish_scene(self,state,events):
        if self.scene_component.get_phase()!=unreal.SovCinematicPhase.COMPLETED: return
        beat=self.scene_beat
        assert len(events)==FINAL.index(beat)+1 and events[-1]['beat']==beat
        receipt=events[-1]
        assert entry.valid_guid(receipt['cinematic']) and not receipt['skipped']
        expected, selected = expected_dialogue_indices(state, self.scene.dialogue_cues)
        assert self.cues_seen==expected, 'Full native selected-branch dialogue cue timeline was not observed'
        self.report['completed_scenes'][beat]=dict(receipt=receipt,cue_count=len(self.cues_seen),completed=True,skipped=False,
            expected_cue_indices=sorted(expected),observed_cue_indices=sorted(self.cues_seen),selected_priority_choice=selected)
        self.report['facts'][beat]=self.facts_for(state,beat)
        self.check_evidence(state,events)
        self.leave_scene()
        if beat=='TarrikIndependentAssent':
            self.begin_route([(-530.,34630.,-1710.)],'aim_assent_handoff')
        elif beat=='SeleneIndependentAssent': self.select_scene('MeridianContainment')
        elif beat=='MeridianContainment': self.select_scene('FifthWitness')
        elif beat=='FifthWitness': self.select_scene('GrammarPropagation')
        elif beat=='GrammarPropagation': self.begin_route([(0.,36200.,-1710.),(0.,37200.,-1710.)],'wait_cp7')
        elif beat=='VoluntaryStay': self.begin_route([(-350.,42700.,90.)],'wait_cp8')
        elif beat=='CauldronRecorderReceived': self.begin_route([(230.,42830.,90.)],'aim_aftermath_handoff')
        elif beat=='Record7283Received': self.select_scene('ContainmentPact')
        elif beat=='ContainmentPact': self.stage('wait_departure_gate')
        elif beat=='SeparateDepartures': self.stage('wait_departure_checkpoint')
        else: raise AssertionError('Unexpected scene')

    def confirm_handoff(self,pc,pawn,state,events):
        self.inject()
        beat=self.held_beat
        if not state.is_beat_complete(unreal.Name(MISSION),unreal.Name(beat)): return
        if pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.IDLE or not pawn or not pawn.is_character_ready(): return
        outgoing,incoming,anchor=HANDOFFS[beat]
        assert isinstance(pawn,unreal.SovSeleneCharacter if incoming=='Selene' else unreal.SovTarrikCharacter) and rescue.alive(pawn)
        companion=self.companion_for(pc,pawn,outgoing,True)
        if not companion: return
        receipt=events[-1]
        assert receipt['beat']==beat and len(events)==FINAL.index(beat)+1
        assert entry.valid_guid(receipt['handoff']) and receipt['anchor']==anchor
        actual_entry=state.get_journal()[-1]
        assert gallery.tag_name(actual_entry.protagonist)==H+outgoing and gallery.tag_name(actual_entry.handoff_to_protagonist)==H+incoming
        after=self.profile(pc,pawn,companion); before=self.handoff_before
        assert after['player_state_guid']==self.player_state_guid
        assert after['player']['actor']!=before['player']['actor'] and after['companion']['actor']!=before['companion']['actor']
        for new,old in ((after['player'],before['companion']),(after['companion'],before['player'])):
            assert new['identity']==old['identity']
            for name,value in old['resources'].items():
                if name=='echo' or name.startswith('max_'): assert abs(new['resources'][name]-value)<.01
        if incoming=='Tarrik':
            assert after['player']['items']==self.report['initial']['profile']['player']['items'], 'Actual retained Tarrik inventory/ammo changed without any combat input'
        self.player_identity,self.companion_identity=incoming,outgoing
        self.current_player,self.current_companion=after['player']['actor'],after['companion']['actor']
        self.current_companion_guid=after['companion']['guid']; self.companion=companion
        self.report['handoffs'].append(dict(beat=beat,receipt=receipt,before=before,after=after,
            policy='Native player kit ledger retained; live incoming/outgoing resources cross roles. New native proxy GUIDs are recorded, not equated to retired proxies.'))
        self.report['identity_epochs'].append(after)
        self.unbind_request(); self.request_result=None
        self.select_scene('SeleneIndependentAssent' if incoming=='Selene' else 'Record7283Received')

    def checkpoint_ready(self,boundary,events):
        rows=[r for r in self.report['saves'] if r['checkpoint'] and r['boundary']==boundary]
        assert all(r['succeeded'] for r in rows), 'Native checkpoint failed: '+json.dumps(rows)
        if not rows: return False
        assert all(r['mission']==MISSION and r['journal']==events for r in rows), 'Checkpoint captured a different canonical boundary'
        assert all(r['generation']>0 and 'L_Aurelion_M13' in r['map'] for r in rows)
        self.report['checkpoints'][boundary]=rows
        return True

    def observe_lift(self,pawn):
        row=dict(elapsed=time.monotonic()-self.started,state=str(self.lift.get_transit_state()),
            body=_xyz(self.lift.moving_body.get_world_location()),player=_xyz(pawn.get_actor_location()),
            companion=_xyz(self.companion.get_actor_location()),player_base=_path(movement_base(pawn)),
            companion_base=_path(movement_base(self.companion)),moving_body=_path(self.lift.moving_body))
        self.report['lift'].append(row)
        return row

    def final_checks(self,pc,pawn,state,events):
        assert [e['beat'] for e in events]==FINAL and len(events)==35
        assert state.is_mission_complete(unreal.Name(MISSION)) and state.is_mission_complete(unreal.Name(prior.MISSION))
        assert not state.get_active_mission().completes_campaign
        assert set(self.report['completed_scenes'])==set(SCENES) and len(self.report['handoffs'])==2
        assert set(self.report['checkpoints'])=={'Aurelion.CP7','Aurelion.CP8','Aurelion.CP9'}
        assert self.lift.get_transit_state()==unreal.SovWorldTransitState.AT_DESTINATION
        assert not self.gates['ContainmentPact'].is_blocking_route()
        for beat in FACTS: self.facts_for(state,beat)
        self.check_evidence(state,events)
        assert self.player_identity=='Tarrik' and self.companion_identity=='Selene'
        player,partner=_xyz(pawn.get_actor_location()),_xyz(self.companion.get_actor_location())
        assert math.dist(player[:2],[-1350.,48000.])<110. and math.dist(partner[:2],[1350.,48000.])<110., 'Separate native exits were not physically retained'
        assert math.dist(player,partner)>2400.
        if self.departure_since is None: self.departure_since=time.monotonic()
        if time.monotonic()-self.departure_since<2.: return
        assert not self.saves.is_load_pending() and not self.saves.is_mission_travel_pending()
        self.report['native_departure']=dict(world=_path(self.world),controller=_path(pc),profile=self.profile(pc,pawn,self.companion),
            journal=events,evidence=evidence(state),player_exit=player,companion_exit=partner,
            m12_complete=True,m13_complete=True,campaign_completion_claim=False,
            same_travelled_world=True,paired_physical_lift=True,ordinary_threshold_checkpoints=True)
        self.finish(True,'All thirteen M13 receipts earned through ordinary controls: independent assents and handoffs, native co-action arrival, paired physical lift, full scenes, exact evidence provenance and CP7/8/9. The 22 M12 receipts are unchanged; actual separate exits persist. Explicit checkpoint reload and rendered quality remain unqualified.')

    def tick(self,delta):
        if self.done: return
        try:
            now=time.monotonic()
            assert now-self.started<1500., 'M13 continuation exceeded 25 minutes'
            assert now-self.phase_at<(240. if self.phase=='walk_route' else 100.), 'Stage deadline: '+self.phase
            world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world and '/Aurelion/Maps/UEDPIE_' in _path(world) and 'L_Aurelion_M13' in world.get_name(), 'Require the existing travelled M13 world'
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            assert isinstance(pc,unreal.SovPlayerController)
            state=pc.get_campaign_state()
            assert state and state.get_active_mission() and str(state.get_active_mission().mission_id)==MISSION and state.is_state_valid()
            events=journal(state)
            assert [e['beat'] for e in events]==FINAL[:len(events)] and len(events)<=35
            assert all(e['mission']==prior.MISSION for e in events[:22]) and all(e['mission']==MISSION for e in events[22:])
            assert all(entry.valid_guid(e['id']) for e in events) and len({e['id'] for e in events})==len(events)
            assert all(a['sequence']<b['sequence'] for a,b in zip(events,events[1:]))
            if self.phase=='initialize': self.initialize(world,pc,pawn,state,events)
            assert world==self.world and pc==self.initial_controller and events[:22]==self.initial_events
            assert events[:len(self.observed_journal)]==self.observed_journal, 'An already observed native receipt changed or disappeared'
            self.observed_journal=list(events)
            for item in state.get_journal()[22:]:
                index=BEATS.index(str(item.beat_id))
                hero='Tarrik' if index<3 or index>=10 else 'Selene'
                assert gallery.tag_name(item.protagonist)==H+hero, 'Native receipt belongs to the wrong protagonist'
            assert prior.stable_id(pc.player_state)==self.player_state_guid
            assert pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.FAILED
            assert not self.saves.is_load_pending() and not self.saves.is_mission_travel_pending()
            if self.request_result is not None: assert self.request_result['accepted'], self.request_result['message']
            self.check_evidence(state,events)
            for beat in self.report['facts']: self.facts_for(state,beat)
            if self.scene_component is not None: self.observe_scene()
            if now-self.last_sample>.5:
                self.last_sample=now
                self.report['samples'].append(dict(elapsed=now-self.started,phase=self.phase,pawn=_path(pawn),
                    ready=pawn.is_character_ready() if pawn else False,position=_xyz(pawn.get_actor_location()) if pawn else None,
                    transition=str(pc.get_campaign_transition_state()),journal=events,
                    actionable=[str(v) for v in state.get_actionable_objective_ids()]))
            if now-self.last_write>1.: self.last_write=now; self.write()
            if unreal.GameplayStatics.is_game_paused(world): self.inject(); return
            # Native handoff may replace the pawn/companion within one input frame.
            if self.phase=='hold_request' and self.held_kind=='handoff': self.hold_request(pc,state); return
            if self.phase=='wait_handoff': self.confirm_handoff(pc,pawn,state,events); return
            assert pawn and pawn.is_character_ready() and rescue.alive(pawn)
            assert _path(pawn)==self.current_player and gallery.tag_name(pawn.get_protagonist_identity_tag())==H+self.player_identity
            companion=self.companion_for(pc,pawn,self.companion_identity)
            assert companion and _path(companion)==self.current_companion and prior.stable_id(companion)==self.current_companion_guid
            self.companion=companion
            if self.phase=='walk_route': self.walk(pc,pawn)
            elif self.phase=='aim_coaction': self.aim_request(pc,pawn,'ContraryPosition','coaction')
            elif self.phase=='hold_request': self.hold_request(pc,state)
            elif self.phase=='wait_coaction': self.confirm_coaction(state,events)
            elif self.phase=='aim_scene': self.aim_use(pc,pawn,self.requests[self.scene_beat],'scene')
            elif self.phase=='hold_scene': self.hold_use(pc)
            elif self.phase=='wait_scene': self.inject(); self.finish_scene(state,events)
            elif self.phase=='aim_assent_handoff': self.aim_request(pc,pawn,'HandoffToSeleneAssent','handoff')
            elif self.phase=='aim_aftermath_handoff': self.aim_request(pc,pawn,'HandoffToTarrikAftermath','handoff')
            elif self.phase=='wait_cp7':
                self.inject()
                if self.checkpoint_ready('Aurelion.CP7',events) and not self.gates['GrammarPropagation'].is_blocking_route():
                    self.begin_route([(0.,37850.,-1710.),(0.,38500.,-1710.),(120.,39000.,-1710.)],'wait_boarding')
            elif self.phase=='wait_boarding':
                self.inject(); row=self.observe_lift(pawn)
                assert self.lift.get_transit_state()==unreal.SovWorldTransitState.AT_ORIGIN
                both=row['player_base']==row['moving_body'] and row['companion_base']==row['moving_body']
                if not both: self.boarded_since=None
                elif self.boarded_since is None: self.boarded_since=now
                elif now-self.boarded_since>.5: self.stage('aim_lift')
            elif self.phase=='aim_lift': self.aim_request(pc,pawn,'M13_AurelionExitLift','lift')
            elif self.phase=='wait_lift':
                self.inject(); row=self.observe_lift(pawn)
                assert self.lift.get_transit_state() in (unreal.SovWorldTransitState.MOVING,unreal.SovWorldTransitState.AT_DESTINATION)
                assert row['player_base']==row['moving_body'] and row['companion_base']==row['moving_body'], 'A real rider left the moving platform'
                if self.lift.get_transit_state()==unreal.SovWorldTransitState.AT_DESTINATION:
                    if self.lift_completed_at is None: self.lift_completed_at=now
                    # Read after ordinary based-movement ticks have consumed the final platform transform.
                    if now-self.lift_completed_at<.25: return
                    assert any(r['state']==str(unreal.SovWorldTransitState.MOVING) for r in self.report['lift'])
                    for label in ('body','player','companion'): assert abs(row[label][2]-self.lift_origin[label][2]-1800.)<6.
                    assert unreal.GameplayStatics.get_time_seconds(world)-self.hold_started>=7.8
                    self.report['native_lift']=dict(before=self.lift_origin,after=row,actual_riders=True)
                    self.select_scene('VoluntaryStay',[(0.,39450.,90.),(0.,41600.,90.),(-100.,42300.,90.)])
            elif self.phase=='wait_cp8':
                self.inject()
                if self.checkpoint_ready('Aurelion.CP8',events): self.select_scene('CauldronRecorderReceived')
            elif self.phase=='wait_departure_gate':
                self.inject()
                if not self.gates['ContainmentPact'].is_blocking_route():
                    self.select_scene('SeparateDepartures',[(0.,44200.,90.),(0.,44900.,90.),(-900.,47000.,90.)])
            elif self.phase=='wait_departure_checkpoint':
                self.inject()
                if self.checkpoint_ready('Aurelion.CP9',events): self.final_checks(pc,pawn,state,events)
            else: raise AssertionError('Unknown phase: '+self.phase)
        except Exception:
            self.report['traceback']=traceback.format_exc()
            self.finish(False,self.report['traceback'])


def start(output_directory):
    global _RUN
    assert _RUN is None or _RUN.done, 'This input continuation is already active'
    for name,module in list(sys.modules.items()):
        running=getattr(module,'_RUN',None) if name.startswith('continue_aurelion_') else None
        assert not (running and hasattr(running,'inject') and not running.done), 'Another ordinary input driver is active: '+name
    out=Path(output_directory)
    assert not (out/'m13-input-continuation.json').exists(), 'Use a new output directory'
    _RUN=Run(out)
    _RUN.handle=unreal.register_slate_post_tick_callback(_RUN.tick)
    _RUN.write()
    return _RUN


def stop():
    if _RUN and not _RUN.done: _RUN.finish(False,'Stopped by operator; inputs released and PIE retained')
