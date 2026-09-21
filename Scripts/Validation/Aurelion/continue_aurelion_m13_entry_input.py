"""Retained E4B victory -> full quarantine/recognition -> actual M13 travel.

Import is inert. Only ordinary movement/look/Interact input is supplied. Native
scenes, threshold, save owner and TravelToMission retain all mutation authority.
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
import continue_aurelion_e3_rescue_input as rescue
import continue_aurelion_e4_entry_input as gallery
import continue_aurelion_e4b_input as phase_b

_RUN = None
MISSION = entry.MISSION
DESTINATION = 'M13_ContraryWitness'
DESTINATION_MAP = '/Game/Aurelion/Maps/L_Aurelion_M13'
QUARANTINE, RECOGNITION = 'SurvivorsClearAndQuarantine', 'ContraryWitnessRecognized'
INITIAL = list(phase_b.FINAL)
FINAL = INITIAL + [QUARANTINE, RECOGNITION]
FACTS = {
    QUARANTINE: {'Campaign.Aurelion.M12.Fact.WestSurvivors':'Campaign.Aurelion.Value.Alive',
                 'Campaign.Aurelion.M12.Fact.EastSurvivors':'Campaign.Aurelion.Value.Alive'},
    RECOGNITION: {'Campaign.Aurelion.M12.Fact.ContraryWitnessesRecognized':'Campaign.Aurelion.Value.Recognized'},
}
_path, _xyz, _optional = common._path, common._xyz, common._optional


def stable_id(actor):
    # NarrativeStableActor declares this exact read-only BlueprintNativeEvent.
    value = actor.call_method('GetActorGUID').export_text()
    assert entry.valid_guid(value), 'Native stable identity is invalid: '+_path(actor)
    return value


def resources(actor):
    result = dict(health=actor.get_health(), max_health=actor.get_max_health(),
                  stamina=actor.get_stamina(), max_stamina=actor.get_max_stamina())
    for label, cls in [('shield',unreal.SovShieldComponent),('poise',unreal.SovPoiseComponent),
                       ('echo',unreal.SovEchoComponent)]:
        components = actor.get_components_by_class(cls)
        assert len(components) == 1, 'Canonical resource component is ambiguous: '+label
        component = components[0]
        result[label] = getattr(component,'get_'+label)()
        result['max_'+label] = getattr(component,'get_max_'+label)()
    assert all(math.isfinite(v) for v in result.values())
    return result


def items(actor):
    """Saved inventory/magazine evidence; independent of lazy runtime ammo caches."""
    inventory = actor.get_inventory_component()
    assert inventory is not None
    actual_items = list(inventory.get_items())
    rows = []
    for item in actual_items:
        assert item and unreal.SystemLibrary.is_valid(item)
        row = dict(cls=_path(item.get_class()),quantity=item.get_quantity())
        if isinstance(item,unreal.WeaponItem):
            clip = item.get_editor_property('weapon_clip_state')
            loaded = int(clip.ammo_in_clip)
            ammo_guid = clip.ammo_item_guid.export_text()
            assert loaded >= 0
            source = inventory.find_item_by_guid(clip.ammo_item_guid) if entry.valid_guid(ammo_guid) else None
            assert source is None or source in actual_items
            assert loaded == 0 or source is not None, 'Saved loaded magazine lost its actual inventory source'
            row.update(stored_clip=loaded,ammo_item_guid=ammo_guid,
                ammo_source_class=_path(source.get_class()) if source is not None else None,
                ammo_source_quantity=source.get_quantity() if source is not None else 0)
        rows.append(row)
    return sorted(rows,key=lambda r:json.dumps(r,sort_keys=True))


def runtime_weapons(actor):
    """Observe functional cache state separately; ordinary wield owns its initialization."""
    rows = []
    for item in actor.get_inventory_component().get_items():
        if isinstance(item,unreal.WeaponItem):
            clip = item.get_editor_property('weapon_clip_state')
            rows.append(dict(cls=_path(item.get_class()),wielded=bool(item.is_wielded()),
                clip=item.get_ammo_in_clip(),reserve=item.get_spare_ammo(),stored_clip=int(clip.ammo_in_clip),
                cached_source_bound=clip.ammo_item_source is not None))
    return sorted(rows,key=lambda r:r['cls'])


class Run(rescue.Run):
    def __init__(self,output_directory):
        super().__init__(output_directory)
        self.saves = self.game_instance = self.director = self.checkpoint = self.travel_request = None
        self.save_delegate = self.save_callback = self.load_delegate = self.load_callback = None
        self.gates = {}; self.protected = {}; self.travel_armed = False
        self.travel_departed = False; self.destination_stable_at = None
        self.restored_victory = None
        self.report.update(scope='Earned E4B victory through two full M12 scenes, physical CP6 and durable ordinary M13 travel only',
            pending=['M13 ContraryPosition and all subsequent M13 gameplay',
                     'Explicit reload/retry, physical keyboard input and rendered presentation review',
                     'Byte-for-byte private protagonist ledger inspection; public stable identities/resources/inventory are observed'],
            completed_scenes={}, scene_phases=[], dialogue_cues=[], request_results=[], saves=[], loads=[],
            travel_states=[], snapshot_comparisons={}, native_checkpoint=None, native_travel=None)

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started,3)
        temporary = self.out/'m13-entry-input-continuation.tmp'
        temporary.write_text(json.dumps(self.report,indent=2,default=str),encoding='utf8')
        common.replace_report_with_retry(temporary, self.out/'m13-entry-input-continuation.json')

    def drop_world_references(self):
        self.unbind_scene(); self.unbind_request()
        for field in ('world','owner','initial_controller','companion','e1','e2','e3','entry',
                      'door','coordination','scene','scene_component','hold_actor','target',
                      'meeting_request','handoff_request','path_target','director','checkpoint','travel_request'):
            setattr(self,field,None)
        self.requests.clear(); self.gates.clear(); self.protected.clear(); self.path_points.clear()

    def finish(self,passed,reason):
        if self.done:
            return
        for delegate,callback in ((self.save_delegate,self.save_callback),(self.load_delegate,self.load_callback)):
            if delegate is not None and callback is not None:
                _optional(lambda d=delegate,c=callback:d.remove_callable(c))
        self.save_delegate = self.save_callback = self.load_delegate = self.load_callback = None
        super().finish(passed,reason)
        self.drop_world_references()
        self.saves = self.game_instance = None
        self.report['retained_gameplay_references_cleared'] = True
        self.write()

    def companion_for(self,world,pc,pawn):
        candidates = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovProtagonistCompanionCharacter)
                      if a.get_owner()==pc and rescue.alive(a) and not a.get_editor_property('hidden')]
        if not candidates:
            return None
        assert len(candidates)==1
        companion = candidates[0]
        assert 'Selene' in companion.get_companion_identity().export_text()
        component = companion.get_companion_component()
        assert component and not component.is_disabled()
        assert component.can_request_command(pawn,unreal.SovCompanionCommand.REGROUP,pawn) is not None
        return companion

    def profiles(self,pc,pawn,companion):
        return dict(player_state_guid=stable_id(pc.player_state),
            player=dict(actor=_path(pawn),cls=_path(pawn.get_class()),resources=resources(pawn),items=items(pawn),runtime_weapons=runtime_weapons(pawn)),
            companion=dict(actor=_path(companion),cls=_path(companion.get_class()),guid=stable_id(companion),
                identity=companion.get_companion_identity().export_text(),resources=resources(companion),items=items(companion),runtime_weapons=runtime_weapons(companion)))

    def facts(self,state,beat):
        definitions = [b for b in state.get_active_mission().beats if str(b.beat_id)==beat]
        assert len(definitions)==1
        writes = definitions[0].state_writes
        assert {gallery.tag_name(w.key):gallery.tag_name(w.value) for w in writes}==FACTS[beat]
        rows = []
        for write in writes:
            assert write.canon_protected and gallery.tag_name(state.get_state_value(write.key))==gallery.tag_name(write.value)
            rows.append(dict(key=gallery.tag_name(write.key),value=gallery.tag_name(write.value),protected=True))
        return rows

    def initialize(self,world,pc,pawn,state,events):
        assert [e['beat'] for e in events]==INITIAL and len(INITIAL)==20
        if self.restored_victory is None:
            previous = phase_b._RUN
            assert previous and previous.done and previous.report['status']=='passed', 'Requires actual successful E4B observer, not a fresh map'
            earned = previous.report['native_victory']
            assert earned['player']==_path(pawn)
        else:
            restored = self.restored_victory
            assert restored['load_result']==str(unreal.SovSaveResult.SUCCESS)
            assert restored['boundary']==phase_b.ENCOUNTER and restored['boundary_kind']==str(unreal.SovSaveBoundary.ARENA_EXIT)
            assert restored['mission']==MISSION and restored['map']=='/Game/Aurelion/Maps/L_Aurelion_M12'
            assert restored['source_report']['status']=='passed' and restored['source_report']['assets_unchanged']
            earned = restored['source_report']['native_victory']
            assert all(not r['alive'] if r['required'] else r['alive'] for r in earned['roster'])
            self.report['restored_victory_admission'] = {k:v for k,v in restored.items() if k!='source_report'}
            self.report['scope'] = 'Public reload of earned E4B ArenaExit, then ordinary inputs through M12 aftermath and M13 travel'
        assert earned['journal']==events
        assert isinstance(pawn,unreal.SovTarrikCharacter) and pawn.is_character_ready() and rescue.alive(pawn)
        assert pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.IDLE
        self.world,self.initial_controller,self.initial_pawn = world,pc,_path(pawn)
        self.initial_world_path = _path(world); self.initial_controller_path = _path(pc)
        self.game_instance = unreal.GameplayStatics.get_game_instance(world)
        self.owner = self.get_input_owner(world)
        settings = unreal.GameUserSettings.get_game_user_settings().get_settings_snapshot()
        assert not settings.tap_interactions and abs(settings.interaction_hold_scale-1.)<.001
        self.initial_events = events
        self.companion = self.companion_for(world,pc,pawn)
        assert self.companion
        if self.restored_victory is None:
            assert _path(self.companion)==earned['companion']
        self.initial_companion_path = _path(self.companion)
        self.director = self.unique(unreal.SovAurelionThermalPhaseDirector,'encounter_id',phase_b.ENCOUNTER)
        assert self.director.get_encounter_state()==unreal.SovEncounterState.SUCCEEDED and self.director.has_confirmed_victory()
        self.attempt = self.director.get_attempt_id().export_text()
        assert self.attempt==earned['attempt']==events[-1]['attempt'] and events[-1]['encounter']==phase_b.ENCOUNTER
        objective = self.unique(unreal.SovCampaignEncounterObjective,'completion_beat','ThermalFracture')
        assert not objective.is_result_pending()
        for participant in self.director.participants:
            identity = str(participant.participant_id)
            if not participant.required_for_victory:
                assert rescue.alive(participant.character)
                self.protected[identity] = participant.character
        assert set(self.protected)==gallery.PROTECTED
        for beat in (QUARANTINE,RECOGNITION):
            candidates = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionRequestActor)
                if str(a.beat_id)==beat and a.operation==unreal.SovAurelionRequest.PLAY_SCENE]
            assert len(candidates)==1
            actor = candidates[0]
            assert str(actor.mission_id)==MISSION and actor.story and abs(actor.interactable.interaction_time-.35)<.001
            assert actor.story.campaign_cinematic.get_phase()==unreal.SovCinematicPhase.IDLE
            self.requests[beat] = actor
        travellers = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionRequestActor)
            if a.operation==unreal.SovAurelionRequest.TRAVEL_TO_MISSION]
        assert len(travellers)==1
        self.travel_request = travellers[0]
        assert str(self.travel_request.mission_id)==MISSION and str(self.travel_request.beat_id)==RECOGNITION
        assert str(self.travel_request.destination_mission.mission_id)==DESTINATION
        assert abs(self.travel_request.interactable.interaction_time-.35)<.001
        self.checkpoint = self.unique(unreal.SovAurelionCheckpoint,'checkpoint',str(unreal.SovAurelionCheckpointBoundary.QUARANTINE_CP6))
        assert str(self.checkpoint.get_boundary_id())=='Aurelion.CP6' and self.checkpoint.capture_on_overlap
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionJournalGate):
            if str(actor.mission_id)==MISSION and str(actor.beat_id) in ('ThermalFracture',QUARANTINE):
                key = (str(actor.beat_id),bool(actor.block_after_completion))
                assert key not in self.gates
                self.gates[key] = actor
        assert set(self.gates)=={('ThermalFracture',False),(QUARANTINE,False),(QUARANTINE,True)}
        saves = [s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==self.game_instance]
        assert len(saves)==1 and saves[0].is_platform_storage_owner_available()
        self.saves = saves[0]
        def saved(result,slot,message):
            current_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            current_pc = unreal.GameplayStatics.get_player_controller(current_world,0) if current_world else None
            current_state = current_pc.get_campaign_state() if current_pc else None
            self.report['saves'].append(dict(result=str(result),succeeded=result==unreal.SovSaveResult.SUCCESS,
                checkpoint=slot.kind==unreal.SovSaveSlotKind.CHECKPOINT,boundary=str(slot.boundary_id),
                boundary_kind=str(slot.boundary_kind),generation=slot.generation,slot_index=slot.slot_index,
                mission=str(slot.mission_id),map=str(slot.map_package),message=str(message),
                journal=entry.journal(current_state) if current_state else None,elapsed=time.monotonic()-self.started))
        def loaded(result,slot,message):
            # Normal mission travel has no OnLoadCompleted success callback. A
            # failure/recovery callback here is a distinct transaction, never a pass.
            self.report['loads'].append(dict(result=str(result),mission=str(slot.mission_id),message=str(message)))
        self.save_callback,self.save_delegate = saved,self.saves.on_save_completed
        self.load_callback,self.load_delegate = loaded,self.saves.on_load_completed
        self.save_delegate.add_callable(saved); self.load_delegate.add_callable(loaded)
        self.report['initial'] = dict(world=_path(world),controller=_path(pc),journal=events,
            attempt=self.attempt,profiles=self.profiles(pc,pawn,self.companion),settings=settings.export_text(),
            protected={k:_path(a) for k,a in self.protected.items()})
        self.stage('wait_evacuation_gate')

    def select_scene(self,beat,points):
        self.unbind_scene(); self.unbind_request(); self.request_result = None
        self.scene_beat = beat; self.scene = self.requests[beat].story
        self.scene_component = self.scene.campaign_cinematic
        self.cues_seen = set(); self.last_scene_phase = None
        assert self.scene_component.get_phase()==unreal.SovCinematicPhase.IDLE
        def changed(phase,reason):
            self.report['scene_phases'].append(dict(beat=beat,phase=str(phase),reason=str(reason),elapsed=time.monotonic()-self.started))
        self.scene_callback,self.scene_delegate = changed,self.scene_component.on_phase_changed
        self.scene_delegate.add_callable(changed)
        position = self.scene.get_actor_location()
        self.begin_route(list(points)+[(position.x+40.,position.y-70.,position.z)],'aim_scene')

    def finish_scene(self,state,events):
        if self.scene_component.get_phase()!=unreal.SovCinematicPhase.COMPLETED:
            return
        assert [e['beat'] for e in events]==FINAL[:FINAL.index(self.scene_beat)+1]
        assert entry.valid_guid(events[-1]['cinematic']) and not events[-1]['skipped']
        assert self.cues_seen==set(range(len(self.scene.dialogue_cues)))
        self.report['completed_scenes'][self.scene_beat] = dict(receipt=events[-1],cue_count=len(self.cues_seen),
            facts=self.facts(state,self.scene_beat),protected={k:dict(actor=_path(a),alive=rescue.alive(a),position=_xyz(a.get_actor_location())) for k,a in self.protected.items()})
        beat = self.scene_beat
        self.unbind_scene(); self.unbind_request(); self.scene = self.scene_component = None
        self.request_result = None
        if beat==QUARANTINE:
            self.stage('wait_quarantine_gates')
        else:
            assert state.is_mission_complete(unreal.Name(MISSION))
            self.report['completed_m12'] = events
            self.begin_route([(-150.,28900.,-1410.),(-130.,29500.,-1410.)],'aim_travel')

    def arm_travel(self,pc,pawn,state):
        assert self.travel_armed and not self.travel_departed
        assert self.saw_countdown and self.request_result and self.request_result['accepted']
        assert pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.TRAVELLING
        assert self.saves.is_mission_travel_pending()
        assert entry.journal(state)==self.report['completed_m12']
        writes = [s for s in self.report['saves'] if s['checkpoint'] and s['boundary']==DESTINATION]
        assert len(writes)==1 and writes[0]['succeeded']
        assert writes[0]['boundary_kind']==str(unreal.SovSaveBoundary.LONG_TRANSITION)
        assert writes[0]['journal']==self.report['completed_m12'] and writes[0]['mission']==MISSION
        self.report['travel_request'] = dict(result=self.request_result,checkpoint=writes[0],
            profiles=self.profiles(pc,pawn,self.companion),controller=_path(pc),world=_path(self.world),
            pending=True,transition=str(pc.get_campaign_transition_state()),native_hold_countdown=True)
        self.inject()
        self.travel_departed = True
        self.drop_world_references()  # only GameInstance/save-owner observers survive travel
        self.stage('wait_m13')

    def observe_destination(self,world):
        assert not self.report['loads'], 'A load/recovery callback replaced ordinary mission travel'
        assert not self.saves.is_awaiting_failure_decision()
        if not world:
            return
        assert unreal.GameplayStatics.get_game_instance(world)==self.game_instance
        if _path(world)==self.initial_world_path:
            return
        assert 'L_Aurelion_M13' in world.get_name() and '/Aurelion/Maps/UEDPIE_' in _path(world)
        pc = unreal.GameplayStatics.get_player_controller(world,0)
        pawn = unreal.GameplayStatics.get_player_pawn(world,0)
        if not pc or not pawn:
            return
        assert isinstance(pc,unreal.SovPlayerController) and isinstance(pawn,unreal.SovTarrikCharacter)
        assert _path(pc)!=self.initial_controller_path and _path(pawn)!=self.initial_pawn
        state = pc.get_campaign_state()
        if not state or not state.get_active_mission() or not pawn.is_character_ready() or not state.is_state_valid():
            return
        assert rescue.alive(pawn) and str(state.get_active_mission().mission_id)==DESTINATION
        assert pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.FAILED
        if pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.IDLE or self.saves.is_load_pending():
            return
        assert not self.saves.is_mission_travel_pending()
        assert not self.saves.has_travel_recovery()  # Native origin is retained until destination readiness.
        companion = self.companion_for(world,pc,pawn)
        if not companion:
            return
        events = entry.journal(state)
        assert events==self.report['completed_m12'], 'Destination lost or manufactured a journal receipt'
        assert state.is_mission_complete(unreal.Name(MISSION)) and not state.is_mission_complete(unreal.Name(DESTINATION))
        assert state.get_objective_state(unreal.Name(DESTINATION),unreal.Name('ContraryPosition')) in (unreal.SovObjectiveState.AVAILABLE,unreal.SovObjectiveState.ACTIVE)
        mode = unreal.GameplayStatics.get_game_mode(world)
        options = str(mode.options_string)
        token = unreal.GameplayStatics.parse_option(options,'SovMissionTravelRequest')
        assert entry.valid_guid(token) and unreal.GameplayStatics.parse_option(options,'SovCampaignTransition')=='1'
        restored = self.profiles(pc,pawn,companion)
        before = self.report['travel_request']['profiles']
        assert restored['player_state_guid']==before['player_state_guid']
        assert entry.valid_guid(restored['companion']['guid'])
        profiles = [p for p in state.get_active_mission().protagonist_companions
                    if p.protagonist.export_text()==companion.get_companion_identity().export_text()]
        assert len(profiles)==1 and str(profiles[0].companion_id)==str(companion.get_companion_component().companion_id)
        self.report['mission_companion_reconstruction'] = dict(origin_guid=before['companion']['guid'],
            destination_guid=restored['companion']['guid'],same_guid_required=False,
            profile=profiles[0].export_text(),owner=_path(companion.get_owner()),
            native_current_leader_command_admitted=True,private_curated_ledger_directly_inspected=False,
            contract='StageInitialCompanion creates a new mission-owned proxy from the previously played inactive protagonist ledger; same-map saved-proxy GUID restoration is a separate contract.')
        for role in ('player','companion'):
            assert restored[role]['cls']==before[role]['cls']
            assert restored[role]['items']==before[role]['items'], 'Travel changed actual inventory/ammunition'
            assert abs(restored[role]['resources']['echo']-before[role]['resources']['echo'])<.01
            for name,value in before[role]['resources'].items():
                if name.startswith('max_'):
                    assert abs(restored[role]['resources'][name]-value)<.01
        assert restored['companion']['identity']==before['companion']['identity']
        self.report['snapshot_comparisons'] = dict(before=before,after=restored,player_state_and_logical_companion_inventory_saved_ammo_echo_and_maxima_preserved=True,
            inventory_evidence_schema='Class/quantity plus public saved WeaponClipState and its native GUID-resolved inventory item; functional cached getters recorded separately',
            functional_weapon_use='A stored magazine comparison alone does not qualify wield/fire; ordinary weapon input must bind the runtime source',
            current_health_shield_stamina_poise='Observed without exact equality: normal passive regeneration remains enabled; no resource writes were issued')
        if self.destination_stable_at is None:
            self.destination_stable_at = time.monotonic()
        if time.monotonic()-self.destination_stable_at<2.:
            return
        self.report['native_travel'] = dict(world=_path(world),controller=_path(pc),pawn=_path(pawn),companion=_path(companion),
            mission=DESTINATION,journal=events,ready=True,load_pending=False,mission_travel_pending=False,
            request_token=token,same_game_instance=True,completed_m12=True,m13_uncompleted=True,first_objective='ContraryPosition')
        self.finish(True,'Full quarantine and recognition scenes earned native M12 completion; physical CP6 and durable travel checkpoint succeeded. The ordinary travel hold replaced the world with ready M13, retaining actual journal, player-state/logical Selene identities and saved inventory/magazines/resources. M13 gameplay is not yet qualified.')

    def tick(self,delta):
        if self.done:
            return
        try:
            now = time.monotonic()
            assert now-self.started<720., 'Continuation exceeded twelve minutes'
            assert now-self.phase_at<(240. if self.phase=='walk_route' else 150. if self.phase=='wait_m13' else 100.), 'Stage deadline: '+self.phase
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if self.phase=='wait_m13':
                self.observe_destination(world)
                if now-self.last_write>1.:
                    self.last_write=now;self.write()
                return
            assert world and '/Aurelion/Maps/UEDPIE_' in _path(world) and 'L_Aurelion_M12' in world.get_name()
            pc = unreal.GameplayStatics.get_player_controller(world,0); pawn = unreal.GameplayStatics.get_player_pawn(world,0)
            assert isinstance(pc,unreal.SovPlayerController) and pawn
            state = pc.get_campaign_state()
            assert state and state.get_active_mission() and str(state.get_active_mission().mission_id)==MISSION
            events = entry.journal(state)
            if self.phase=='initialize':
                self.initialize(world,pc,pawn,state,events)
            assert world==self.world and pc==self.initial_controller and _path(pawn)==self.initial_pawn
            assert rescue.alive(pawn) and rescue.alive(self.companion) and _path(self.companion)==self.initial_companion_path
            assert self.companion.get_owner()==pc
            assert events[:len(INITIAL)]==self.initial_events and [e['beat'] for e in events]==FINAL[:len(events)] and len(events)<=len(FINAL)
            assert all(e['mission']==MISSION for e in events)
            assert self.director.get_attempt_id().export_text()==self.attempt and self.director.has_confirmed_victory()
            assert all(rescue.alive(a) for a in self.protected.values())
            assert not self.saves.is_awaiting_failure_decision() and not self.report['loads']
            if self.scene_component:
                self.observe_scene()
            if self.request_result is not None:
                assert self.request_result['accepted'], 'Native request rejected: '+self.request_result['message']
            if now-self.last_sample>.5:
                self.last_sample=now
                self.report['samples'].append(dict(elapsed=now-self.started,phase=self.phase,position=_xyz(pawn.get_actor_location()),
                    health=pawn.get_health(),journal=events,transition=str(pc.get_campaign_transition_state())))
            if now-self.last_write>1.:
                self.last_write=now;self.write()
            if self.phase=='hold_scene':
                self.hold_use(pc);return
            if self.phase=='wait_scene':
                self.inject();self.finish_scene(state,events);return
            if self.phase=='hold_travel':
                remaining=float(pc.get_interaction_component().get_editor_property('remaining_interact_time'))
                if 0.<remaining<=.351:
                    self.saw_countdown=True
                if self.request_result is not None:
                    self.arm_travel(pc,pawn,state)
                else:
                    assert now-self.phase_at<8., 'Ordinary travel hold failed'
                    self.inject(interact=1.)
                return
            assert not self.saves.is_load_pending() and not self.saves.is_mission_travel_pending()
            if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world) or pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.IDLE:
                self.inject();return
            if self.phase=='walk_route':
                self.walk(pc,pawn)
            elif self.phase=='wait_evacuation_gate':
                self.inject()
                if not self.gates[('ThermalFracture',False)].is_blocking_route():
                    self.select_scene(QUARANTINE,[(0.,22800.,-1110.),(0.,23300.,-1110.)])
            elif self.phase=='aim_scene':
                self.aim_use(pc,pawn,self.requests[self.scene_beat],'scene')
            elif self.phase=='wait_quarantine_gates':
                self.inject()
                if self.gates[(QUARANTINE,True)].is_blocking_route() and not self.gates[(QUARANTINE,False)].is_blocking_route():
                    self.report['quarantine_physical_gates'] = dict(arena_sealed=True,recognition_access_open=True)
                    self.begin_route([(60.,23550.,-1197.3000058),(-210.,23550.,-1197.3000033),(-210.,24250.,-1372.2999885),(210.,24250.,-1372.2999924),(0.,25200.,-1410.),_xyz(self.checkpoint.get_actor_location())],'wait_cp6')
            elif self.phase=='wait_cp6':
                self.inject()
                relevant = [s for s in self.report['saves'] if s['checkpoint'] and s['boundary']=='Aurelion.CP6']
                assert all(s['succeeded'] for s in relevant)
                if relevant:
                    assert len(relevant)==1 and relevant[0]['journal']==events and [e['beat'] for e in events]==FINAL[:-1]
                    assert relevant[0]['boundary_kind']==str(unreal.SovSaveBoundary.EXPLICIT_CHECKPOINT)
                    assert relevant[0]['mission']==MISSION and relevant[0]['map']=='/Game/Aurelion/Maps/L_Aurelion_M12'
                    self.report['native_checkpoint']=relevant[0]
                    self.select_scene(RECOGNITION,[(0.,26600.,-1410.),(-300.,27900.,-1410.)])
            elif self.phase=='aim_travel':
                actor=self.travel_request; interaction=pc.get_interaction_component()
                look,error=self.look(world,pc,actor.get_actor_location())
                admission=actor.interactable.can_interact(pawn,interaction)
                self.inject(look=look)
                self.report['last_travel_admission']=dict(admitted=admission is not None,reason=str(admission),native_action_text=str(actor.interactable.get_interactable_action_text(pawn,interaction)),error=error,
                    focus=_path(interaction.get_editor_property('viewed_interactable')),actor=_path(actor),last_result=str(actor.last_result))
                if error<3. and admission is not None and interaction.get_editor_property('viewed_interactable')==actor.interactable:
                    self.unbind_request(); self.saw_countdown=False; self.request_result=None
                    def requested(accepted,message):
                        self.request_result=dict(accepted=accepted,message=str(message),elapsed=time.monotonic()-self.started)
                        self.report['request_results'].append(dict(kind='travel',**self.request_result))
                        if accepted:
                            # Retire strong world references within the publication
                            # frame, before ServerTravel can replace/collect it.
                            try:
                                current_pc=unreal.GameplayStatics.get_player_controller(self.world,0)
                                current_pawn=unreal.GameplayStatics.get_player_pawn(self.world,0)
                                self.arm_travel(current_pc,current_pawn,current_pc.get_campaign_state())
                            except Exception:
                                self.report['error']=traceback.format_exc()
                                self.finish(False,self.report['error'])
                    self.request_callback,self.request_delegate=requested,actor.on_request_result
                    self.request_delegate.add_callable(requested)
                    self.travel_armed=True; self.stage('hold_travel')
        except Exception:
            self.report['error']=traceback.format_exc()
            self.finish(False,self.report['error'])


def start(output_directory=None,restored_victory=None):
    global _RUN
    assert _RUN is None or _RUN.done
    for name,module in list(sys.modules.items()):
        if name.startswith('continue_aurelion_') and name!=__name__:
            run=getattr(module,'_RUN',None)
            assert run is None or not hasattr(run,'inject') or run.done, 'Stop the earlier input owner first: '+name
    output=Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (output/'m13-entry-input-continuation.json').exists(), 'Use a fresh evidence directory'
    _RUN=Run(output);_RUN.restored_victory=restored_victory;_RUN.write()
    _RUN.handle=unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False,'Stopped by operator; ordinary inputs released, no recovery/state repair issued')


if __name__=='__main__':
    start()
