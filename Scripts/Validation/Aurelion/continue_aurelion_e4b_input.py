"""Actual E4B: ordinary companion/frost/heat requests, native payoff, conventional victory.

Import is inert. Requires the earned nineteen-beat M12 prefix and exact retained
phase-B attempt. Never fabricates damage, resources, actor motion or proof.
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
import continue_aurelion_e4_entry_input as prior
import continue_aurelion_e4a_input as phase_a
import aurelion_wheel_input as wheel

_RUN=None
INITIAL=list(phase_a.FINAL)
BEAT='ThermalFracture'
FINAL=INITIAL+[BEAT]
MISSION=entry.MISSION
ENCOUNTER=phase_a.PHASE_B
CINDERLINE='/Game/Items/Weapons/WI_Cinderline.WI_Cinderline_C'
_path,_xyz,_optional=common._path,common._xyz,common._optional


FROST_RETRY_LIMIT = 3
AIM_REPOSITION_LIMIT = 4
AIM_FOCUS_STALL_SECONDS = 3.

class Run(phase_a.Run):
    def __init__(self,output_directory):
        self.last_frost_miss=-1000.
        super().__init__(output_directory)
        self.controls={}
        self.thermal=self.elite=self.core=self.poise=self.frost_anchor=None
        self.control_name=None
        self.walking_look=None
        self.thermal_started=None
        self.last_combat_progress=self.started
        self.last_combat_signature=None
        self.last_conventional_shot_at=-1000.
        self.report.update(scope='Retained actual E4B entry through ordinary Selene positioning, native frost/heat/Core payoff and conventional encounter victory only',
            pending=['SurvivorsClearAndQuarantine and all later scenes, checkpoints and travel',
                     'Physical keyboard use and rendered quality'],
            entry_requires=['earned nineteen-beat journal and real Tarrik/owned Selene',
                'same active E4B attempt with inherited living Elite and protected roster',
                'existing Cinderline/ammunition and three authored normal request controls'],
            thermal_receipts=[],thermal_damage=[],core_breaks=[],frost_windows=[],missed_frost_windows=[],
            contextual_requests=[],combat_targets=[],wheels=[],native_thermal=None,native_victory=None)

    def inject(self,move=(0.,0.),look=(0.,0.),attack=0.,aim=0.,reload=0.,interact=0.,pulse=0.,wheel_hold=0.):
        # The normal path follower still owns movement; turn toward the next
        # real control concurrently instead of wasting the short frost window.
        if self.walking_look is not None and not self.done:
            look=self.walking_look
        super().inject(move,look,attack,aim,reload,interact,pulse,wheel_hold)

    def walk(self,pc,pawn):
        if self.control_name=='HeatConfirm':
            self.walking_look,error=self.look(self.world,pc,self.controls['HeatConfirm'].get_actor_location())
            self.report['heat_approach_aim']=dict(error=error,
                remaining=float(self.thermal.get_fracture_window_remaining_seconds()))
        try:
            self.walk_heat_precisely(pc,pawn) if self.control_name=='HeatConfirm' else super().walk(pc,pawn)
        finally:
            self.walking_look=None

    def walk_heat_precisely(self,pc,pawn):
        # Same complete-path ordinary follower; only the Heat arrival tolerances
        # tighten because its authored range margin is smaller than the shared 40cm.
        if not self.waypoints:
            self.inject();self.stage(self.next_route_state);return
        target=self.waypoints[0];current=pawn.get_actor_location()
        if math.hypot(current.x-target[0],current.y-target[1])<=10. and abs(current.z-target[2])<140.:
            self.waypoints.pop(0);self.path_target=None
            self.last_motion_at=time.monotonic();self.inject();return
        now=time.monotonic()
        if self.path_target!=target or now-self.last_path>.8:
            self.path_target,self.last_path=target,now
            nav=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(
                self.world,current,unreal.Vector(*target),pawn,None)
            complete=nav is not None and nav.is_valid() and not nav.is_partial()
            self.path_points=list(nav.path_points)[1:] if complete else []
            row=dict(destination=target,complete=complete,
                points=[_xyz(p) for p in nav.path_points] if nav else [],elapsed=now-self.started,
                final_arrival_cm=10.,final_path_node_cm=8.)
            self.report['last_route_path']=row;self.report['route_paths'].append(row)
        movement=(0.,0.)
        while self.path_points:
            movement,reached=self.local_move(pc,pawn,_xyz(self.path_points[0]),stop=8. if len(self.path_points)==1 else 25.)
            if not reached:break
            self.path_points.pop(0)
        self.inject(move=movement)
        if self.last_position is None or math.hypot(current.x-self.last_position[0],current.y-self.last_position[1])>35.:
            self.last_position,self.last_motion_at=_xyz(current),now
        assert now-self.last_motion_at<15., 'Normal Heat traversal stalled: '+json.dumps(self.report.get('last_route_path'))

    def write(self):
        self.report['elapsed_seconds']=round(time.monotonic()-self.started,3)
        temp=self.out/'e4b-input-continuation.tmp'
        temp.write_text(json.dumps(self.report,indent=2,default=str),encoding='utf8')
        common.replace_report_with_retry(temp, self.out/'e4b-input-continuation.json')

    def finish(self,passed,reason):
        if self.done:
            return
        super().finish(passed,reason)
        self.controls.clear()
        self.thermal=self.elite=self.core=self.poise=self.frost_anchor=None
        self.write()

    def thermal_state(self):
        if not all(unreal.SystemLibrary.is_valid(v) for v in (self.elite,self.thermal,self.core,self.poise)):
            return dict(retired_after_observed_native_death=self.observed_defeats.get('E4.Elite')==self.e4_paths.get('E4.Elite'),
                completed_native_receipt=self.report['native_thermal'],
                remaining_component_queries='Not performed on retired objects')
        return dict(component=_path(self.thermal),elite=_path(self.elite),
            elite_health=self.elite.get_health() if unreal.SystemLibrary.is_valid(self.elite) else None,
            poise=self.poise.get_poise(),poise_broken=self.poise.is_poise_broken(),
            poise_recovering=self.poise.is_poise_recovering(),
            window=self.thermal.get_fracture_window_remaining_seconds(),
            completed=self.thermal.has_completed_fracture(self.e4b,self.e4b.get_attempt_id()),
            receipt=self.thermal.get_fracture_receipt().export_text(),last_error=str(self.thermal.last_error),
            core_broken=self.core.is_weak_point_broken(unreal.Name('Core')),
            core_revealed=self.core.is_weak_point_reveal_active(),
            companion=_path(self.companion),companion_location=_xyz(self.companion.get_actor_location()),
            anchor=_xyz(self.frost_anchor.get_actor_location()),
            anchor_distance=math.dist(_xyz(self.companion.get_actor_location()),_xyz(self.frost_anchor.get_actor_location())))

    def initialize(self,world,pc,pawn,state,events):
        assert [e['beat'] for e in events]==INITIAL, 'Start only after actual E4A link proof and Tarrik handoff'
        assert isinstance(pawn,unreal.SovTarrikCharacter) and pawn.is_character_ready() and rescue.alive(pawn)
        assert pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.IDLE
        self.world,self.initial_controller,self.initial_pawn=world,pc,_path(pawn)
        self.current_pawn_path=self.initial_pawn;self.initial_events=events
        self.owner=self.get_input_owner(world)
        # This class deliberately overrides phase-A initialization; configure
        # the inherited read-only precision-ray helper in this actual world too.
        channel=unreal.ArsenalStatics.get_narrative_pro_settings().weapon_trace_channel
        display=str(channel.get_display_name())
        trace_types=[]
        for index in range(32):
            try: trace=unreal.TraceTypeQuery.cast(index)
            except (TypeError,ValueError): continue
            if str(trace.get_display_name())==display: trace_types.append(trace)
        assert len(trace_types)==1, 'Native weapon collision channel lacks an exact trace mapping: '+display
        self.weapon_trace_type=trace_types[0]
        self.report['weapon_trace_mapping']=dict(native_channel=str(channel),display_name=display,
            trace_type=str(self.weapon_trace_type),scope='Read-only required Core follow-up; thermal proof and ordinary Cinderline damage owners unchanged')
        settings=unreal.GameUserSettings.get_game_user_settings().get_settings_snapshot()
        assert not settings.tap_interactions and abs(settings.interaction_hold_scale-1.)<.001
        self.companion=self.owned_companion(pc,pawn,'Selene')
        assert self.companion is not None
        self.current_companion_path=_path(self.companion)
        self.e4=self.unique(unreal.SovAurelionLinkPhaseDirector,'encounter_id',prior.E4_ID)
        self.e4b=self.unique(unreal.SovAurelionThermalPhaseDirector,'encounter_id',ENCOUNTER)
        self.phase_b_objective=self.unique(unreal.SovCampaignEncounterObjective,'completion_beat',BEAT)
        assert self.phase_b_objective.encounter_director==self.e4b and self.e4.phase_b_objective==self.phase_b_objective
        assert self.e4.get_encounter_state()==unreal.SovEncounterState.SUCCEEDED and not list(self.e4.participants)
        assert events[-2]['encounter']==prior.E4_ID and events[-2]['attempt']==self.e4.get_attempt_id().export_text()
        assert entry.valid_guid(events[-1]['handoff']) and events[-1]['anchor']=='M12_TarrikCrucible'
        assert self.e4b.get_encounter_state()==unreal.SovEncounterState.ACTIVE and self.e4b.has_encounter_player(pawn)
        self.attempt=self.e4b.get_attempt_id().export_text()
        assert entry.valid_guid(self.attempt) and not self.e4b.has_confirmed_victory()
        rows=self.roster_for(self.e4b)
        assert rows and all(r['alive'] for r in rows)
        assert {r['id'] for r in rows if not r['required']}==prior.PROTECTED
        assert {r['id'] for r in rows if r['required']}<=prior.HOSTILES
        assert {'E4.Elite','E4.Weaver'}<={r['id'] for r in rows}
        self.e4_paths={r['id']:r['actor'] for r in rows}
        self.observe_roster_deaths(self.e4b)
        self.elite=self.e4b.get_participant(unreal.Name('E4.Elite'))
        self.core=self.elite.get_core_weak_points()
        self.thermal=self.elite.get_thermal_fracture()
        poise=self.elite.get_components_by_class(unreal.SovPoiseComponent)
        assert len(poise)==1
        self.poise=poise[0]
        assert self.poise.is_initialized() and self.core.is_initialized()
        assert self.thermal.encounter_director==self.e4b
        assert not self.thermal.has_completed_fracture(self.e4b,self.e4b.get_attempt_id())
        assert not self.core.is_weak_point_broken(unreal.Name('Core'))
        self.frost_anchor=self.thermal.frost_anchor
        assert self.frost_anchor is not None and self.frost_anchor.actor_has_tag(self.thermal.frost_anchor_id)
        operations={'MovePartner':unreal.SovAurelionRequest.MOVE_FROST_PARTNER,
                    'FrostSetup':unreal.SovAurelionRequest.FROST_SETUP,
                    'HeatConfirm':unreal.SovAurelionRequest.HEAT_CONFIRM}
        for name,operation in operations.items():
            actor=self.unique(unreal.SovAurelionRequestActor,'request_id','Aurelion.E4.'+name)
            assert actor.operation==operation and str(actor.beat_id)==BEAT and str(actor.mission_id)==MISSION
            assert actor.thermal is None and actor.thermal_director==self.e4b and str(actor.thermal_participant_id)=='E4.Elite'
            assert actor.get_current_thermal_target()==self.thermal
            assert abs(float(actor.interactable.interaction_time)-.35)<.001
            self.controls[name]=actor
        def fractured(receipt):
            self.report['thermal_receipts'].append(dict(encounter=str(receipt.encounter_id),attempt=receipt.attempt_id.export_text(),
                frost=receipt.frost_application_id.export_text(),heat=receipt.heat_transaction_id.export_text(),
                payoff=receipt.payoff_transaction_id.export_text(),elapsed=time.monotonic()-self.started,
                actual_poise_broken=self.poise.is_poise_broken(),elite_alive=rescue.alive(self.elite),
                core_revealed=self.core.is_weak_point_reveal_active()))
        self.observe(self.thermal.on_thermal_fracture_completed,fractured)
        def damaged(result):
            if result.source_actor==pawn and result.target_actor==self.elite:
                self.report['thermal_damage'].append(dict(transaction=result.transaction_id.export_text(),
                    source=_path(result.source_actor),target=_path(result.target_actor),channels=result.damage_channels.export_text(),
                    health=result.applied_health_damage,shield=result.applied_shield_damage,poise=result.applied_poise_damage,
                    poise_broken=bool(result.poise_broken),fatal=bool(result.fatal),zone=str(result.hit_zone),
                    elapsed=time.monotonic()-self.started))
        self.observe(self.elite.get_narrative_ability_system_component().on_damage_resolved_as_target,damaged)
        def core_broken(zone,result):
            self.report['core_breaks'].append(dict(zone=str(zone),transaction=result.transaction_id.export_text(),
                source=_path(result.source_actor),target=_path(result.target_actor),
                health=result.applied_health_damage,shield=result.applied_shield_damage,
                elapsed=time.monotonic()-self.started,after_thermal=self.report['native_thermal'] is not None))
        self.observe(self.core.on_weak_point_broken,core_broken)
        instance=unreal.GameplayStatics.get_game_instance(world)
        saves=[s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance]
        assert len(saves)==1 and saves[0].is_platform_storage_owner_available()
        self.saves=saves[0]
        self.echo=pawn.get_component_by_class(unreal.SovEchoComponent)
        self.report['initial']=dict(attempt=self.attempt,journal=events,pawn=_path(pawn),companion=_path(self.companion),
            roster=rows,thermal=self.thermal_state(),settings=settings.export_text(),
            controls={k:dict(actor=_path(a),position=_xyz(a.get_actor_location()),range=float(a.interactable.interaction_distance),hold=float(a.interactable.interaction_time)) for k,a in self.controls.items()},
            native_ranges=dict(frost_anchor=float(self.thermal.frost_anchor_reach),frost_setup=float(self.thermal.frost_setup_range),
                heat_confirm=float(self.thermal.heat_confirm_range),frost_seconds=float(self.thermal.fracture_window_seconds)))
        self.selector=wheel.Selector(CINDERLINE)
        self.stage('select_cinderline')

    def retry_frost(self,pawn,reason):
        # Thermal Fracture is recoverable: a missed or rejected window returns to an ordinary fresh frost setup.
        self.inject();self.unbind_request();self.request_result=None
        self.report['missed_frost_windows'].append(dict(elapsed=time.monotonic()-self.started,phase=self.phase,
            control=self.control_name,reason=reason,state=self.thermal_state()))
        assert len(self.report['missed_frost_windows'])<=FROST_RETRY_LIMIT, 'Thermal Fracture window repeatedly missed: '+reason
        self.last_frost_miss=time.monotonic()
        if 'Selene must occupy' in reason:
            # The native frost rule needs Selene on the clean mark with sight of the elite; reposition her first.
            self.go_control('MovePartner')
            return
        p=self.controls['FrostSetup'].get_actor_location()
        self.go_control('FrostSetup',then='prepare_frost',point=(p.x-250.,p.y,pawn.get_actor_location().z))

    def reposition_for_focus(self,pawn,focus):
        # Another interactable (for example a hostile's) can hold focus in front of the control. Step to an
        # alternate ordinary standing point instead of aiming indefinitely under fire.
        rows=self.report.setdefault('aim_repositions',[])
        rows.append(dict(elapsed=time.monotonic()-self.started,control=self.control_name,focus=_path(focus),
            player=_xyz(pawn.get_actor_location())))
        assert len(rows)<=AIM_REPOSITION_LIMIT, 'Contextual control focus repeatedly obstructed: '+self.control_name
        p=self.controls[self.control_name].get_actor_location()
        side=150. if len(rows)%2 else -150.
        then='prepare_frost' if self.control_name=='FrostSetup' else 'aim_control'
        self.go_control(self.control_name,then=then,point=(p.x-200.,p.y+side,pawn.get_actor_location().z))

    def go_control(self,name,then='aim_control',point=None):
        self.control_name=name
        actor=self.controls[name];p=actor.get_actor_location()
        # Stand outside the real solid request body; entry.walk retains whole-path navigation/collision.
        destination=point or (p.x-200.,p.y-100.,p.z+20.)
        self.begin_route([destination],then)

    def aim_control(self,pc,pawn):
        actor=self.controls[self.control_name];component=actor.interactable;interaction=pc.get_interaction_component()
        look,error=self.look(self.world,pc,actor.get_actor_location())
        admission=component.can_interact(pawn,interaction)
        focus=interaction.get_editor_property('viewed_interactable')
        self.report['last_interaction']=dict(control=self.control_name,admitted=admission is not None,admission=str(admission),native_action_text=str(component.get_interactable_action_text(pawn,interaction)),
            focus=_path(focus),actor=_path(actor),error=error,player=_xyz(pawn.get_actor_location()),
            position=_xyz(actor.get_actor_location()),last_result=str(actor.last_result),thermal=self.thermal_state())
        self.inject(look=look)
        if (self.control_name=='HeatConfirm' or error<3.) and admission is not None and focus==component:
            self.unbind_request();self.hold_actor=actor;self.hold_started=unreal.GameplayStatics.get_time_seconds(self.world)
            self.saw_countdown=False;self.request_result=None
            name=self.control_name
            def result(accepted,message):
                self.request_result=dict(control=name,accepted=accepted,message=str(message),elapsed=time.monotonic()-self.started)
                self.report['contextual_requests'].append(self.request_result)
            self.request_delegate,self.request_callback=actor.on_request_result,result
            self.request_delegate.add_callable(self.request_callback)
            self.stage('hold_control')

    def hold_control(self,pc):
        interaction=pc.get_interaction_component()
        remaining=float(interaction.get_editor_property('remaining_interact_time'))
        focus=interaction.get_editor_property('viewed_interactable')
        admission=self.hold_actor.interactable.can_interact(pc.get_controlled_pawn(),interaction)
        sample=dict(control=self.control_name,focus=_path(focus),remaining=remaining,
            admitted=admission is not None,admission=str(admission))
        trace=self.report.setdefault('hold_trace',[])
        if not trace or any(trace[-1].get(k)!=v for k,v in sample.items()):
            sample['elapsed']=time.monotonic()-self.started
            trace.append(sample)
        if 0.<remaining<=.35:
            self.saw_countdown=True
        if self.hold_actor.is_request_pending() or self.request_result is not None:
            self.inject()
            assert self.saw_countdown, 'Authored ordinary hold countdown was not observed'
            self.report['holds'].append(dict(control=self.control_name,seconds=.35,native_countdown=True,
                input_game_seconds=unreal.GameplayStatics.get_time_seconds(self.world)-self.hold_started))
            self.stage('wait_control')
        else:
            if focus!=self.hold_actor.interactable or admission is None or (self.saw_countdown and remaining<=-998.):
                # Native focus/reach loss cancels a hold. A continuously held
                # input cannot generate another Started event: release and aim
                # again, with a bounded retry count and no admission override.
                self.inject()
                retries=self.report.setdefault('hold_retries',{})
                retries[self.control_name]=retries.get(self.control_name,0)+1
                assert retries[self.control_name]<=6, 'Contextual focus repeatedly lost during ordinary hold'
                self.unbind_request()
                self.stage('aim_control')
                return
            assert time.monotonic()-self.phase_at<8., 'Native contextual hold did not complete'
            look,error=self.look(self.world,pc,self.hold_actor.get_actor_location())
            self.inject(interact=1.,look=look)

    def wait_control(self,pc,pawn):
        self.inject()
        if self.request_result is None:
            assert time.monotonic()-self.phase_at<3., 'Deferred native request produced no current result'
            return
        if not self.request_result['accepted'] and self.control_name in ('FrostSetup','HeatConfirm'):
            self.retry_frost(pawn,'Native '+self.control_name+' rejected: '+self.request_result['message'])
            return
        assert self.request_result['accepted'], 'Native request rejected: '+self.request_result['message']
        self.unbind_request();self.request_result=None
        if self.control_name=='MovePartner':
            self.stage('wait_partner_mark')
        elif self.control_name=='FrostSetup':
            assert self.thermal.get_fracture_window_remaining_seconds()>0., 'Accepted frost did not leave a current native window'
            assert self.core.is_weak_point_reveal_active(), 'Native frost did not reveal the authored Core'
            self.report['frost_windows'].append(dict(elapsed=time.monotonic()-self.started,state=self.thermal_state()))
            self.thermal_started=unreal.GameplayStatics.get_time_seconds(self.world)
            destination=self.heat_destination(pawn)
            self.go_control('HeatConfirm',point=destination)
        else:
            self.stage('wait_thermal_payoff')

    def heat_destination(self,pawn,required=True):
        # Intersect the two actual horizontal reach intervals at player height.
        # Keep the original 25/20 cm native-range margins and all path/LOS gates.
        request=self.controls['HeatConfirm'].get_actor_location();elite=self.elite.get_actor_location()
        z=pawn.get_actor_location().z
        dx,dy=elite.x-request.x,elite.y-request.y;distance=math.hypot(dx,dy)
        elite_radius=float(self.thermal.heat_confirm_range)-25.
        request_radius=float(self.controls['HeatConfirm'].interactable.interaction_distance)-20.
        assert math.isfinite(elite_radius) and math.isfinite(request_radius) and elite_radius>0. and request_radius>0.
        if not (elite_radius>abs(z-elite.z) and request_radius>abs(z-request.z)):
            self.report['heat_endpoint']=dict(feasible=False,request=_xyz(request),elite=_xyz(elite),
                player_z=z,native_range_margins=[25.,20.],reason='No vertical reach overlap')
            assert not required, 'Current native heat/control ranges have no shared approach height'
            return None
        elite_reach=math.sqrt(elite_radius**2-(z-elite.z)**2)
        request_reach=math.sqrt(request_radius**2-(z-request.z)**2)
        low=max(100.,distance-elite_reach+1.)
        high=min(request_reach-1.,distance+elite_reach-1.)
        if low>high:
            self.report['heat_endpoint']=dict(feasible=False,request=_xyz(request),elite=_xyz(elite),
                player_z=z,feasible_interval=[low,high],native_range_margins=[25.,20.],
                reason='No horizontal reach overlap')
            assert not required, 'Current native heat/control ranges have no shared approach point'
            return None
        travel=(low+high)*.5  # Interior of both preserved native reach margins.
        x=request.x+(dx/distance*travel if distance>.01 else travel)
        y=request.y+(dy/distance*travel if distance>.01 else 0.)
        destination=(x,y,z)
        assert math.dist(destination,_xyz(elite))<elite_radius
        assert math.dist(destination,_xyz(request))<request_radius
        self.report['heat_endpoint']=dict(feasible=True,request=_xyz(request),elite=_xyz(elite),destination=destination,
            travel=travel,feasible_interval=[low,high],elite_distance=math.dist(destination,_xyz(elite)),
            control_distance=math.dist(destination,_xyz(request)),native_range_margins=[25.,20.])
        return destination

    def confirm_thermal(self):
        self.inject()
        if not self.report['thermal_receipts']:
            assert time.monotonic()-self.phase_at<3., 'Native heat supplied no current nonlethal Poise payoff: '+json.dumps(self.thermal_state())
            return
        assert len(self.report['thermal_receipts'])==1
        receipt=self.report['thermal_receipts'][0]
        assert receipt['encounter']==ENCOUNTER and receipt['attempt']==self.attempt
        assert all(entry.valid_guid(receipt[k]) for k in ('frost','heat','payoff')) and receipt['heat']!=receipt['payoff']
        assert receipt['elite_alive'] and receipt['actual_poise_broken']
        assert self.thermal.has_completed_fracture(self.e4b,self.e4b.get_attempt_id())
        heat=[d for d in self.report['thermal_damage'] if d['transaction']==receipt['heat']]
        payoff=[d for d in self.report['thermal_damage'] if d['transaction']==receipt['payoff']]
        assert len(heat)==len(payoff)==1 and heat[0]['poise']>0. and payoff[0]['poise']>0. and payoff[0]['poise_broken']
        assert all(d['health']==0. and d['shield']==0. and not d['fatal'] for d in heat+payoff)
        self.report['native_thermal']=dict(receipt=receipt,heat=heat[0],payoff=payoff[0],state=self.thermal_state(),
            owner='Actual native control/heat/Poise transaction owners; driver supplied ordinary holds only')
        self.stage('core_followup')

    def select_core_point(self,pawn):
        points=[p for p in self.shot_points(self.elite) if p[0]=='Core']
        assert points, 'Actual authored unbroken Core aim matcher unavailable'
        for point in points:
            exposed,unused=self.precision_ray(pawn,self.elite,point,True)
            if exposed: return point
        return points[0]

    def fire_at(self,pc,pawn,weapon,actor,point=None,precision=None):
        # The Elite needs a higher aim point at close range. Small enemies use
        # a visible authored bone or their origin; the same offset can put the
        # reticle above a wall-running mesh and waste an entire magazine.
        location=point or actor.get_actor_location()+unreal.Vector(0.,0.,75. if actor==self.elite else 0.)
        look,error=self.look(self.world,pc,location)
        desired_ray=actual_ray=None
        if precision is not None:
            assert actor==self.elite and precision[0]=='Core' and self.report['native_thermal'] is not None
            clear,desired_ray=self.precision_ray(pawn,actor,precision,True)
            actual_match,actual_ray=self.precision_ray(pawn,actor,precision)
        else:
            clear=self.clear_point(pawn,actor,location)
            actual_match=True
        distance=math.dist(_xyz(pawn.get_actor_location()),_xyz(location))
        in_range=distance<min(2400.,max(500.,weapon.get_attack_range()*.8))
        # Blocked sight has to be answered by moving, at any range: a pilot standing inside 450 cm with no
        # line to its target neither fires nor repositions, which stalled an E4B run against a live WallRunner.
        move=self.approach(self.world,pc,pawn,actor) if (not clear or not in_range) and distance>120. else (0.,0.)
        # A fixed-position aim pilot was adequate while authored melee attacks
        # faced away, but now walks into the Elite's striking radius. Preserve
        # ordinary movement and fire inputs while kiting the nearest live threat.
        if precision is None:
            nearby=[p.character for p in self.e4b.participants
                if p.required_for_victory and rescue.alive(p.character)
                and not p.character.get_editor_property('hidden')]
            if nearby:
                position=pawn.get_actor_location()
                threat=min(nearby,key=lambda enemy: math.dist(_xyz(position),_xyz(enemy.get_actor_location())))
                threat_position=threat.get_actor_location()
                dx,dy=position.x-threat_position.x,position.y-threat_position.y
                separation=math.hypot(dx,dy)
                if 1.<separation<550.:
                    goal=(position.x+dx/separation*350.,position.y+dy/separation*350.,position.z)
                    move,unused=self.local_move(pc,pawn,goal,stop=25.)
        clip,reserve=weapon.get_ammo_in_clip(),weapon.get_spare_ammo()
        assert clip>0 or reserve>0, 'Existing Cinderline ammunition exhausted; no refill was issued'
        game_time=unreal.GameplayStatics.get_time_seconds(self.world)
        input_gate=None
        if precision is not None:
            eligible=clip>0 and clear and actual_match and in_range
            fire,move,look,input_gate=self.settled_precision_input(pawn,weapon,actor,precision,move,look,error,eligible,actual_ray)
        else:
            # One ordinary trigger edge, then allow native weapon spread to
            # settle before the next shot. Long held bursts exhaust Cinderline
            # against small mobile targets without demonstrating accuracy.
            fire=clip>0 and clear and actual_match and in_range and error<1.5 and game_time-self.last_conventional_shot_at>=.8
            if fire:
                self.last_conventional_shot_at=game_time
                self.report.setdefault('conventional_shots',[]).append(dict(
                    elapsed=time.monotonic()-self.started,target=str(self.e4b.find_participant_id(actor)),
                    clip=clip,reserve=reserve,spread=weapon.get_weapon_spread(),point=_xyz(location)))
        reload_input=1. if clip<=0 and game_time%1.2<.15 else 0.
        self.report['last_combat']=dict(target=str(self.e4b.find_participant_id(actor)),actor=_path(actor),point=_xyz(location),
            clear=clear,error=error,distance=distance,clip=clip,reserve=reserve,attack=fire,health=actor.get_health(),
            core_revealed=self.core.is_weak_point_reveal_active() if unreal.SystemLibrary.is_valid(self.core) else False,
            core_precision_gate=precision is not None,actual_zone_hit=actual_match if precision is not None else None,
            desired_weapon_ray=desired_ray,actual_weapon_ray=actual_ray,input_gate=input_gate)
        self.inject(move=move,look=look,aim=0. if clip<=0 else 1.,attack=float(fire),reload=reload_input)

    def combat(self,pc,pawn,weapon,rows,events):
        if self.e4b.get_encounter_state()==unreal.SovEncounterState.SUCCEEDED:
            self.inject()
            if [e['beat'] for e in events]!=FINAL:
                return
            assert self.e4b.has_confirmed_victory() and not self.phase_b_objective.is_result_pending()
            assert events[-1]['encounter']==ENCOUNTER and events[-1]['attempt']==self.attempt
            assert all(not r['alive'] for r in rows if r['required']) and all(r['alive'] for r in rows if not r['required'])
            assert self.report['native_thermal'] is not None
            self.report['native_victory']=dict(attempt=self.attempt,receipt=events[-1],roster=rows,journal=events,
                thermal=self.report['native_thermal'],actual_core_followup=self.report['core_breaks'],
                player=_path(pawn),companion=_path(self.companion),health=pawn.get_health())
            self.finish(True,'Real companion movement and authored holds produced native frost, distinct heat/Poise payoff receipts and a living Elite. Ordinary Core follow-up and conventional Cinderline combat then earned actual ThermalFracture encounter victory with all protected people alive. Later scenes/travel remain unqualified.')
            return
        candidates=[p.character for p in self.e4b.participants if p.required_for_victory and rescue.alive(p.character)
            and not p.character.get_editor_property('hidden') and p.character.get_actor_enable_collision()]
        if not candidates:
            self.inject();return
        actor=min(candidates,key=lambda a:(str(self.e4b.find_participant_id(a))!='E4.Elite',
            math.dist(_xyz(pawn.get_actor_location()),_xyz(a.get_actor_location()))))
        identity=str(self.e4b.find_participant_id(actor))
        assert identity in prior.HOSTILES
        if actor!=self.target:
            self.target=actor
            self.report['combat_targets'].append(dict(id=identity,actor=_path(actor),elapsed=time.monotonic()-self.started))
        body_point=None
        if actor!=self.elite:
            for candidate in self.shot_points(actor):
                exposed,unused=self.precision_ray(pawn,actor,candidate,True)
                if exposed:
                    body_point=candidate[3]
                    break
        self.fire_at(pc,pawn,weapon,actor,point=body_point)
        signature=tuple((r['id'],r['alive'],r['health']) for r in rows if r['required'])
        if signature!=self.last_combat_signature:
            self.last_combat_signature=signature;self.last_combat_progress=time.monotonic()
        assert time.monotonic()-self.last_combat_progress<50., 'Ordinary conventional combat made no native health progress'

    def tick(self,delta):
        if self.done:
            return
        try:
            now=time.monotonic()
            assert now-self.started<720., 'E4B continuation exceeded twelve-minute bound'
            assert now-self.phase_at<(300. if self.phase=='combat' else 150. if self.phase=='walk_route' else 70.), 'Stage deadline: '+self.phase
            world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world and '/Aurelion/Maps/UEDPIE_' in _path(world) and 'L_Aurelion_M12' in world.get_name()
            pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0) if pc else None
            assert pc and pawn
            state=pc.get_campaign_state();assert state and state.get_active_mission() and str(state.get_active_mission().mission_id)==MISSION
            events=entry.journal(state)
            assert len(events)<=len(FINAL) and [e['beat'] for e in events]==FINAL[:len(events)]
            if self.phase=='initialize':
                self.initialize(world,pc,pawn,state,events)
            assert world==self.world and pc==self.initial_controller and _path(pawn)==self.initial_pawn
            assert events[:len(INITIAL)]==self.initial_events and all(e['mission']==MISSION for e in events)
            assert isinstance(pawn,unreal.SovTarrikCharacter) and rescue.alive(pawn), 'Player retired; no recovery issued'
            assert rescue.alive(self.companion) and self.companion.get_owner()==pc and not self.companion.get_companion_component().is_disabled()
            assert self.e4b.get_attempt_id().export_text()==self.attempt and self.e4b.get_encounter_state() in (unreal.SovEncounterState.ACTIVE,unreal.SovEncounterState.SUCCEEDED)
            assert self.e4b.has_encounter_player(pawn) and not list(self.e4.participants)
            if self.report['native_thermal'] is None:
                assert self.e4b.get_participant(unreal.Name('E4.Elite'))==self.elite
                assert all(a.get_current_thermal_target()==self.thermal for a in self.controls.values())
            assert not self.saves.is_awaiting_failure_decision() and not self.saves.is_load_pending() and not self.saves.is_mission_travel_pending()
            rows=self.roster_for(self.e4b)
            assert {r['id'] for r in rows}==set(self.e4_paths), 'Frozen phase-B participant IDs changed'
            self.check_roster_identities(rows)
            assert all(r['alive'] for r in rows if not r['required']) and {r['id'] for r in rows if not r['required']}==prior.PROTECTED
            if self.report['native_thermal'] is None:
                assert rescue.alive(self.elite), 'Elite died before its real thermal payoff'
            if self.request_result is not None and self.request_result['control'] not in ('FrostSetup','HeatConfirm'):
                assert self.request_result['accepted'], 'Native contextual request failed: '+self.request_result['message']
            if now-self.last_sample>.5:
                self.last_sample=now
                self.report['samples'].append(dict(elapsed=now-self.started,phase=self.phase,position=_xyz(pawn.get_actor_location()),
                    health=pawn.get_health(),echo=self.echo.get_echo() if self.echo else None,thermal=self.thermal_state(),roster=rows,journal=events))
            if now-self.last_write>1.:
                self.last_write=now;self.write()
            if self.phase=='select_cinderline':
                held,report=self.selector.step(world);self.inject(wheel_hold=float(held))
                if self.selector.done:
                    self.report['wheels'].append(report)
                    assert report['status']=='passed', 'Normal Cinderline wheel selection failed: '+report.get('reason','')
                    self.go_control('MovePartner')
                return
            if self.phase=='hold_control':
                self.hold_control(pc);return
            if self.phase=='wait_control':
                self.wait_control(pc,pawn);return
            if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world) or pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.IDLE:
                self.inject();return
            weapon=common.Run.weapon(self,pawn)
            if self.phase=='walk_route':
                if self.control_name=='HeatConfirm' and self.thermal.get_fracture_window_remaining_seconds()<=.35:
                    self.retry_frost(pawn,'Native frost window expired during physical approach; no extension was supplied')
                    return
                self.walk(pc,pawn)
            elif self.phase=='aim_control':
                focus=pc.get_interaction_component().get_editor_property('viewed_interactable')
                if (focus is not None and focus!=self.controls[self.control_name].interactable
                        and now-self.phase_at>AIM_FOCUS_STALL_SECONDS):
                    self.reposition_for_focus(pawn,focus)
                    return
                if self.control_name=='HeatConfirm' and self.thermal.get_fracture_window_remaining_seconds()<=.35:
                    self.retry_frost(pawn,'Native frost window expired before ordinary heat hold')
                    return
                self.aim_control(pc,pawn)
            elif self.phase=='wait_partner_mark':
                self.inject()
                distance=math.dist(_xyz(self.companion.get_actor_location()),_xyz(self.frost_anchor.get_actor_location()))
                if distance<float(self.thermal.frost_anchor_reach)-15.:
                    self.report['partner_positioning']=dict(distance=distance,companion=_path(self.companion),
                        position=_xyz(self.companion.get_actor_location()),anchor=_xyz(self.frost_anchor.get_actor_location()),
                        source='Ordinary held MoveFrostPartner request followed by actual companion movement')
                    p=self.controls['FrostSetup'].get_actor_location()
                    self.go_control('FrostSetup',then='prepare_frost',point=(p.x-250.,p.y,pawn.get_actor_location().z))
            elif self.phase=='prepare_frost':
                self.inject()
                # Do not open a three-second window unless the current physical heat endpoint is plausible.
                if self.heat_destination(pawn,required=False) is None:
                    self.report['waiting_for_elite']=dict(reason='Native reach intervals do not overlap',
                        endpoint=self.report['heat_endpoint'])
                    return
                # A retry waits for the closed window and the native one-second setup cooldown.
                if (self.poise.get_poise()>.1 and not self.poise.is_poise_recovering()
                        and self.thermal.get_fracture_window_remaining_seconds()<=0.
                        and time.monotonic()-self.last_frost_miss>=1.25):
                    self.stage('aim_control')
            elif self.phase=='wait_thermal_payoff':
                self.confirm_thermal()
            elif self.phase=='core_followup':
                actual=[r for r in self.report['core_breaks'] if r['zone']=='Core' and r['source']==self.initial_pawn and r['after_thermal']]
                if actual:
                    assert len(actual)==1 and entry.valid_guid(actual[0]['transaction'])
                    self.report['core_followup']=actual[0]
                    self.inject();self.stage('combat')
                else:
                    assert rescue.alive(self.elite), 'Elite died without the ordinary Core follow-up being observed'
                    point=self.select_core_point(pawn)
                    assert point is not None and point[0]=='Core', 'Actual authored Core aim matcher unavailable'
                    self.fire_at(pc,pawn,weapon,self.elite,point[3],precision=point)
            elif self.phase=='combat':
                self.combat(pc,pawn,weapon,rows,events)
        except Exception:
            self.report['error']=traceback.format_exc()
            self.finish(False,self.report['error'])


def start(output_directory=None):
    global _RUN
    assert _RUN is None or _RUN.done, 'E4B driver already runs'
    for name in ('continue_aurelion_e1_input','continue_aurelion_e2_input','continue_aurelion_e3_entry_input',
                 'continue_aurelion_e3_rescue_input','continue_aurelion_e4_entry_input','continue_aurelion_e4a_input'):
        module=sys.modules.get(name);run=getattr(module,'_RUN',None) if module else None
        assert run is None or run.done, 'Stop earlier input owner first: '+name
    output=Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (output/'e4b-input-continuation.json').exists(), 'Use a fresh evidence directory'
    _RUN=Run(output);_RUN.write()
    _RUN.handle=unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False,'Stopped by operator; ordinary inputs released, no thermal/save/progression repair issued')


if __name__=='__main__':
    start()
