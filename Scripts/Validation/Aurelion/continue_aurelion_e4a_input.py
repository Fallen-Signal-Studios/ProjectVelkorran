"""Ordinary-input E4A precision/severs -> native Tarrik handoff -> real E4B entry.

Import is inert. The normal wheel selects existing Axiom; no weapon/resource,
damage, transform, link, encounter, mission, checkpoint or proof mutation occurs.
"""
import json
import math
import os
import re
from pathlib import Path
import sys
import time
import traceback
import unreal
import continue_aurelion_e1_input as common
import continue_aurelion_e3_entry_input as entry
import continue_aurelion_e3_rescue_input as rescue
import continue_aurelion_e4_entry_input as prior
import aurelion_wheel_input as wheel

_RUN = None
MISSION = entry.MISSION
INITIAL = list(prior.FINAL)
SEVER = 'SeverCrucibleLinks'
HANDOFF = 'HandoffToTarrikCrucible'
FINAL = INITIAL + [SEVER, HANDOFF]
PHASE_B = 'M12_E4_QuarantineCrucibleB'
AXIOM = '/Game/Items/Weapons/WI_Axiom.WI_Axiom_C'
EARN_TARGETS = frozenset(('E4.Linkbound1', 'E4.Linkbound2', 'E4.WallRunner'))
LINK_IDS = frozenset(('Aurelion.Weaver.AnchorA', 'Aurelion.Weaver.AnchorB'))
_path, _xyz, _optional = common._path, common._xyz, common._optional


class Run(prior.Run):
    def __init__(self, output_directory):
        super().__init__(output_directory)
        self.links = {}
        self.link_instances = {}
        self.observers = []
        self.selector = None
        self.weaver = self.echo = self.generator = self.phase_b_objective = None
        self.shot_count = 0
        self.pulse_started = None
        self.pulse_before = None
        self.last_earn_at = None
        self.observed_defeats = {}
        self.report.update(scope='Actual active E4A through normal precision fire and two native Axiom severs, WallRunner release, native Tarrik handoff and E4B entry only',
            pending=['Thermal Fracture, conventional E4B victory and every later beat',
                     'Physical keyboard operation and rendered battle/cinematic quality'],
            entry_requires=['unchanged seventeen-beat journal through real WestStretchers/LocalPriorityCommitted',
                'same ready living Selene and active E4A with four shown / one reserved, seven protected alive',
                'native Axiom independent-link selector fix compiled; actual wheel input adapter available',
                'existing Axiom, ammunition and native Echo generation; no resources are supplied'],
            wheels=[], link_receipts=[], link_samples=[], echo_awards=[], echo_spends=[], waves=[],
            pulses=[], missed_pulses=[], precision_shots=[], saves=[], route_paths=[], request_results=[],
            native_phase_a=None, native_phase_b=None)
        self.report['native_deaths'] = []
        self.weapon_trace_type = None
        for name in ('IA_Ability2', 'IA_WeaponWheel'):
            self.actions[name] = unreal.load_asset(common.ACTION_ROOT+name)
            assert self.actions[name] is not None, 'Missing existing action '+name

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 3)
        temp = self.out/'e4a-input-continuation.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        common.replace_report_with_retry(temp, self.out/'e4a-input-continuation.json')

    def inject(self, move=(0.,0.), look=(0.,0.), attack=0., aim=0., reload=0., interact=0., pulse=0., wheel_hold=0.):
        common.Run.inject(self, move, look, attack, aim, reload, interact)
        if self.owner:
            for name, value in (('IA_Ability2', pulse), ('IA_WeaponWheel', wheel_hold)):
                self.owner.inject_input_vector_for_action(self.actions[name], unreal.Vector(value,0.,0.), [], [])
                if value:
                    self.report['input_frames'][name] = self.report['input_frames'].get(name,0)+1

    def observe(self, delegate, callback):
        # The Python delegate proxy must itself stay strongly referenced.
        self.observers.append((delegate,callback))
        delegate.add_callable(callback)

    def finish(self, passed, reason):
        if self.done:
            return
        if self.selector is not None:
            self.selector.stop()
        for delegate, callback in self.observers:
            _optional(lambda d=delegate,c=callback: d.remove_callable(c))
        self.observers.clear()
        super().finish(passed, reason)
        self.links.clear()
        self.selector = self.weaver = self.echo = self.generator = self.phase_b_objective = None
        self.actions.clear()
        self.report['retained_gameplay_references_cleared'] = True
        self.write()

    def link_rows(self):
        rows=[]
        for identity, link in self.links.items():
            assert unreal.SystemLibrary.is_valid(link) and link.get_owner()==self.weaver
            snapshot=link.capture_command_link_state()
            assert snapshot.link_instance_id.export_text()==self.link_instances[identity], 'Required link instance changed'
            rows.append(dict(id=identity,component=_path(link),state=str(snapshot.state),
                active=link.is_command_link_active(),instance=snapshot.link_instance_id.export_text(),
                transaction=snapshot.last_sever_transaction_id.export_text()))
        return rows

    def observe_roster_deaths(self,director):
        def make_death_observer(key,path,asc_path):
            # Capture identity outside the three-argument native delegate.
            def died(changed,changed_asc,is_dead):
                row=dict(id=key,actor=_path(changed),asc=_path(changed_asc),dead=bool(is_dead),
                    elapsed=time.monotonic()-self.started)
                self.report['native_deaths'].append(row)
                if is_dead and row['actor']==path and row['asc']==asc_path:
                    self.observed_defeats[key]=path
            return died
        for participant in director.participants:
            identity=str(participant.participant_id)
            if not participant.required_for_victory:
                continue
            actor=participant.character
            expected=_path(actor)
            asc=actor.get_narrative_ability_system_component()
            died=make_death_observer(identity,expected,_path(asc))
            self.observe(asc.on_death_state_changed,died)

    def check_roster_identities(self,rows):
        for row in rows:
            expected=self.e4_paths[row['id']]
            if row['actor'] is None:
                assert row['required'] and not row['alive'] and self.observed_defeats.get(row['id'])==expected, 'Actor retired without its exact observed native death'
            else:
                assert row['actor']==expected, 'Encounter participant identity changed'

    def initialize(self, world, pc, pawn, state, events):
        assert [e['beat'] for e in events]==INITIAL, 'Start only after the actual complete E4 entry driver'
        assert isinstance(pawn,unreal.SovSeleneCharacter) and pawn.is_character_ready() and rescue.alive(pawn)
        assert pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.IDLE
        self.world,self.initial_controller,self.initial_pawn=world,pc,_path(pawn)
        self.current_pawn_path=self.initial_pawn
        self.initial_events=events
        self.owner=self.get_input_owner(world)
        self.companion=self.owned_companion(pc,pawn,'Tarrik')
        assert self.companion is not None
        self.current_companion_path=_path(self.companion)
        settings=unreal.GameUserSettings.get_game_user_settings().get_settings_snapshot()
        assert not settings.tap_interactions and abs(settings.interaction_hold_scale-1.)<.001, 'Standard authored holds required'
        self.e4=self.unique(unreal.SovAurelionLinkPhaseDirector,'encounter_id',prior.E4_ID)
        self.e4b=self.unique(unreal.SovAurelionThermalPhaseDirector,'encounter_id',PHASE_B)
        self.e4_entry=self.unique(unreal.SovCampaignEncounterObjective,'completion_beat',SEVER)
        self.phase_b_objective=self.unique(unreal.SovCampaignEncounterObjective,'completion_beat','ThermalFracture')
        assert self.e4.phase_b_objective==self.phase_b_objective and self.phase_b_objective.encounter_director==self.e4b
        assert self.e4.get_encounter_state()==unreal.SovEncounterState.ACTIVE and self.e4.has_encounter_player(pawn)
        assert self.e4b.get_encounter_state()==unreal.SovEncounterState.INACTIVE and not list(self.e4b.participants)
        assert not self.e4.has_confirmed_victory() and not self.e4.auto_request_handoff and self.e4.auto_start_phase_b
        self.attempt=self.e4.get_attempt_id().export_text()
        assert entry.valid_guid(self.attempt)
        rows=self.roster_for(self.e4)
        assert {r['id'] for r in rows if r['required']}==prior.HOSTILES
        assert {r['id'] for r in rows if not r['required']}==prior.PROTECTED and all(r['alive'] for r in rows)
        assert {r['id'] for r in rows if r['required'] and r['hidden']}=={'E4.WallRunner'}
        self.e4_paths={r['id']:r['actor'] for r in rows}
        self.observe_roster_deaths(self.e4)
        self.coordination=self.e4.get_coordination_component()
        assert self.coordination.get_current_wave()==0
        self.weaver=self.e4.get_participant(unreal.Name('E4.Weaver'))
        assert self.weaver is not None
        def make_sever_observer(expected):
            # Default capture parameters also count toward Unreal's arity check.
            def severed(result):
                self.report['link_receipts'].append(dict(expected=expected,id=str(result.link_id),
                    transaction=result.transaction_id.export_text(),instance=result.link_instance_id.export_text(),
                    owner=_path(result.link_owner),source=_path(result.command_source),severer=_path(result.severed_by),
                    reward_eligible=bool(result.eligible_for_echo_reward),
                    affected=[_path(a) for a in result.affected_actors],elapsed=time.monotonic()-self.started))
            return severed
        for binding in self.e4.required_links:
            assert str(binding.participant_id)=='E4.Weaver'
            matches=[c for c in self.weaver.get_components_by_class(unreal.SovCommandLinkComponent)
                if c.get_name()==str(binding.component_name) and str(c.get_link_id())==str(binding.link_id)]
            assert len(matches)==1 and matches[0].is_command_link_active()
            link=matches[0];identity=str(binding.link_id)
            self.links[identity]=link
            self.link_instances[identity]=link.get_link_instance_id().export_text()
            assert entry.valid_guid(self.link_instances[identity]) and link.get_command_source()==self.weaver
            severed=make_sever_observer(identity)
            self.observe(link.on_command_link_severed,severed)
        assert set(self.links)==LINK_IDS and len(set(self.link_instances.values()))==2
        self.observe(self.coordination.on_wave_changed,lambda wave:self.report['waves'].append(
            dict(wave=wave,elapsed=time.monotonic()-self.started,roster=self.roster_for(self.e4))))
        self.echo=pawn.get_component_by_class(unreal.SovEchoComponent)
        self.generator=pawn.get_component_by_class(unreal.SovSeleneEchoGenerationComponent)
        assert self.echo and self.generator and self.generator.is_initialized()
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
            trace_type=str(self.weapon_trace_type),method='Conservative complex zero-radius CameraTowardsFocus ray; no native damage/proof function called')
        self.observe(self.generator.on_selene_echo_awarded,lambda amount,new,kind,zone,other:
            self.report['echo_awards'].append(dict(amount=amount,new=new,kind=str(kind),zone=str(zone),
                other=_path(other),elapsed=time.monotonic()-self.started)))
        self.observe(self.echo.on_echo_spent,lambda tag,amount,new:self.report['echo_spends'].append(
            dict(tag=tag.export_text(),amount=amount,new=new,elapsed=time.monotonic()-self.started)))
        instance=unreal.GameplayStatics.get_game_instance(world)
        owners=[s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance]
        assert len(owners)==1 and owners[0].is_platform_storage_owner_available()
        self.saves=owners[0]
        def saved(result,slot,message):
            self.report['saves'].append(dict(result=str(result),success=result==unreal.SovSaveResult.SUCCESS,
                kind=str(slot.kind),checkpoint=slot.kind==unreal.SovSaveSlotKind.CHECKPOINT,
                boundary=str(slot.boundary_id),arena_entry=slot.boundary_kind==unreal.SovSaveBoundary.ARENA_ENTRY,
                generation=slot.generation,slot_index=slot.slot_index,mission=str(slot.mission_id),map=str(slot.map_package),
                protagonist=slot.active_protagonist.export_text(),message=str(message),
                journal=[str(e.beat_id) for e in pc.get_campaign_state().get_journal()],elapsed=time.monotonic()-self.started))
        self.observe(self.saves.on_save_completed,saved)
        self.handoff_request=self.unique(unreal.SovAurelionRequestActor,'beat_id',HANDOFF)
        assert self.handoff_request.operation==unreal.SovAurelionRequest.HANDOFF
        assert self.handoff_request.handoff_anchor==self.e4.handoff_anchor
        assert str(self.handoff_request.handoff_anchor.anchor_id)=='M12_TarrikCrucible'
        assert abs(float(self.handoff_request.interactable.interaction_time)-.35)<.001
        self.report['initial']=dict(pawn=_path(pawn),companion=_path(self.companion),attempt=self.attempt,
            journal=events,roster=rows,links=self.link_rows(),echo=self.echo.get_echo(),settings=settings.export_text(),
            actions={name:[k.export_text() for k in self.owner.query_keys_mapped_to_action(action)] for name,action in self.actions.items()},
            pulse_contract=dict(cost=30.,native_refund=12.,full_charge=.65,maximum_range=4000.,half_angle=22.5,
                expected_current_mapping='IA_Ability2: X / Gamepad_DPad_Up; actual mapping recorded above'))
        self.selector=wheel.Selector(AXIOM,manual_mainhand=True)
        self.stage('select_axiom')

    def actual_axiom(self,pawn):
        wielded=[w for w in pawn.get_wielded_weapons() if w is not None]
        assert wielded and all(w.get_class().get_path_name()==AXIOM for w in wielded), 'Actual Axiom wield ownership changed'
        assert len(wielded)<=2, 'Unexpected Axiom wield multiplicity'
        return max(wielded,key=lambda w:w.get_ammo_in_clip())

    def shot_points(self,actor):
        points=[]
        bodies=[actor]
        visual=actor.get_character_visual()
        if visual:
            bodies.append(visual)
        meshes=[m for body in bodies for m in body.get_components_by_class(unreal.SkeletalMeshComponent)]
        for component in actor.get_components_by_class(unreal.SovWeakPointComponent):
            if not component.is_initialized():
                continue
            for zone in component.get_editor_property('weak_point_zones'):
                if component.is_weak_point_broken(zone.zone_id):
                    continue
                for bone in zone.hit_bones:
                    for mesh in meshes:
                        if mesh.does_socket_exist(bone):
                            location=mesh.get_socket_location(bone)
                            points.append((str(zone.zone_id),str(bone),mesh,location,component,zone))
        # Prefer a real authored head matcher; every candidate comes from the native zone.
        return sorted(points,key=lambda v:('head' not in v[1].lower(),v[0],v[1],_path(v[2])))

    def shot_point(self,actor):
        # Preserve the existing first-four-fields contract for E4B's Core follow-up.
        points=self.shot_points(actor)
        return points[0] if points else None

    def precision_ray(self,pawn,target,point,desired=False):
        camera=unreal.GameplayStatics.get_player_camera_manager(self.world,0)
        eye=camera.get_camera_location()
        if desired:
            delta=tuple(a-b for a,b in zip(_xyz(point[3]),_xyz(eye)))
            length=math.sqrt(sum(v*v for v in delta))
            assert length>1.
            direction=tuple(v/length for v in delta)
        else:
            rotation=camera.get_camera_rotation()
            pitch,yaw=math.radians(rotation.pitch),math.radians(rotation.yaw)
            direction=(math.cos(pitch)*math.cos(yaw),math.cos(pitch)*math.sin(yaw),math.sin(pitch))
        location=pawn.get_actor_location()
        projection=sum(v*d for v,d in zip((location.x-eye.x,location.y-eye.y,location.z-eye.z),direction))
        origin=unreal.Vector(*(v+projection*d for v,d in zip(_xyz(eye),direction)))
        end=unreal.Vector(*(v+15000.*d for v,d in zip(_xyz(origin),direction)))
        ignored=[pawn]
        for actor in list(pawn.get_all_child_actors())+list(pawn.get_attached_actors(reset_array=True,recursively_include_attached_actors=True)):
            if actor not in ignored: ignored.append(actor)
        raw=unreal.SystemLibrary.line_trace_single(self.world,origin,end,self.weapon_trace_type,
            True,ignored,unreal.DrawDebugTrace.NONE,True)
        hit=raw if isinstance(raw,unreal.HitResult) else next((v for v in (raw or ()) if isinstance(v,unreal.HitResult)),None)
        values=hit.to_tuple() if hit else None
        actor=values[9] if values else None
        mesh=values[10] if values else None
        bone=values[11] if values else unreal.Name('None')
        target_hit=bool(values and values[0] and (actor==target or (actor and actor.get_owner()==target)))
        component,zone=point[4],point[5]
        exact=target_hit and str(bone)!='None' and str(bone) in {str(b) for b in zone.hit_bones}
        descendants=False
        ancestry=[]
        if target_hit and not exact and zone.match_descendant_bones and isinstance(mesh,unreal.SkinnedMeshComponent):
            current=bone
            # Read the actual hit mesh hierarchy, matching native MatchesBone's
            # semantic test. Never submit a synthetic FSovDamageResult to it.
            for unused in range(128):
                if str(current)=='None' or str(current) in ancestry: break
                ancestry.append(str(current))
                current=mesh.get_parent_bone(current)
                if str(current) in {str(b) for b in zone.hit_bones}:
                    descendants=True; break
        matched=bool(target_hit and (exact or descendants) and component.is_initialized()
            and not component.is_weak_point_broken(zone.zone_id))
        row=dict(target=_path(target),hit=_path(actor),hit_component=_path(mesh),hit_bone=str(bone),
            blocking=bool(values and values[0]),target_hit=target_hit,zone=point[0],zone_component=_path(component),
            exact_bone=bool(exact),descendant_bone=descendants,parent_chain=ancestry,matched_unbroken_zone=matched,
            origin=_xyz(origin),direction=direction,impact=_xyz(values[5]) if values else None,
            trace_channel=str(self.weapon_trace_type),trace_complex=True,sweep_radius=0.,desired_ray=desired)
        return matched,row

    def clear_point(self,pawn,target,point,origin=None):
        if origin is None:
            origin=unreal.GameplayStatics.get_player_camera_manager(self.world,0).get_camera_location()
        ignored=[pawn]+list(pawn.get_attached_actors())
        visual=pawn.get_character_visual()
        if visual and visual not in ignored:
            ignored.append(visual)
        raw=unreal.SystemLibrary.line_trace_single(self.world,origin,point,
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
        if raw is None:
            # Python packs native bool=false/out-hit as None: this precise
            # visibility segment had no blocker. It is not a damage receipt.
            self.report['last_sight']=dict(clear=True,origin=_xyz(origin),target=_path(target),
                point=_xyz(point),hit=None,impact=None,native_trace_hit=False)
            return True
        hits=[v for v in raw if isinstance(v,unreal.HitResult)] if isinstance(raw,tuple) else [raw]
        assert len(hits)==1 and isinstance(hits[0],unreal.HitResult)
        hit=hits[0].to_tuple();actor=hit[9]
        clear=not hit[0] or actor==target or (actor is not None and actor.get_owner()==target)
        self.report['last_sight']=dict(clear=clear,origin=_xyz(origin),target=_path(target),point=_xyz(point),
            hit=_path(actor),impact=_xyz(hit[5]))
        return clear

    def stage(self,name,detail=None):
        # A phase change retires only this QA trigger intent; native proof owners
        # and the inherited ordinary-input phase machine remain unchanged.
        self.precision_pending=None
        self.precision_ready_since=None
        self.precision_previous=None
        self.precision_release_until=0.
        super().stage(name,detail)

    def settled_precision_input(self,pawn,weapon,target,point,move,look,error,eligible,actual_ray):
        """Track actual zone hits while stationary; freeze look only for the trigger."""
        now=float(unreal.GameplayStatics.get_time_seconds(self.world))
        wielded=[item for item in pawn.get_wielded_weapons() if item is not None]
        assert weapon in wielded and wielded, 'Precision weapon ownership changed'
        ammo={_path(item):int(item.get_ammo_in_clip()) for item in wielded}
        spreads={_path(item):float(item.get_weapon_spread()) for item in wielded}
        assert all(math.isfinite(value) and value>=0. for value in spreads.values()), 'Invalid native weapon spread'
        spread=max(spreads.values())
        # Magazine refill can precede the native reload/equip action ending.
        # Observe its public tags before starting a new ordinary trigger edge.
        asc=pawn.get_narrative_ability_system_component()
        assert asc is not None and asc.get_avatar_owner()==pawn
        owned_tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(asc).export_text()
        tag_names=re.findall(r'TagName="([^"]+)"',owned_tags)
        blocking_roots=('Narrative.State.Weapon.BlockFiring',
            'Narrative.State.Weapon.Equipping','Narrative.State.Weapon.Reloading',
            'Narrative.State.Busy','Sov.State.EchoAbility.Active')
        blockers=sorted(name for name in tag_names
            if any(name==root or name.startswith(root+'.') for root in blocking_roots))
        input_ready=not blockers
        velocity=_xyz(pawn.get_velocity())
        assert all(math.isfinite(value) for value in velocity)
        speed=math.sqrt(sum(value*value for value in velocity))
        key=(self.phase,_path(target),point[0],point[1],_path(point[2]),tuple(sorted(ammo)))
        row=dict(target=_path(target),zone=point[0],bone=point[1],speed_cm_s=speed,
            spread_degrees=spread,weapon_spreads=spreads,clips=ammo,error_degrees=error,
            actual_zone_hit=bool(actual_ray['matched_unbroken_zone']),game_seconds=now,
            native_input_ready=input_ready,native_input_blockers=blockers,owned_tags=owned_tags,
            policy=dict(maximum_speed_cm_s=5.,maximum_spread_degrees=.05,
                consecutive_hit_seconds=.05,minimum_hit_samples=3,maximum_sample_gap_seconds=.1,
                maximum_pulse_seconds=.05,minimum_release_seconds=.2,
                angle_error='diagnostic; actual exact/descendant native zone ray owns admission'))
        pending=getattr(self,'precision_pending',None)
        if pending is not None:
            assert set(ammo)==set(pending['clips']), 'Wielded set changed during the owned precision pulse'
            consumed=any(ammo[name]<value for name,value in pending['clips'].items())
            row.update(status='awaiting_actual_ammunition',pulse_target=pending['target'],
                pulse_seconds=now-pending['started'],ammunition_consumed=consumed)
            if consumed:
                self.report.setdefault('precision_input_results',[]).append(dict(
                    target=pending['target'],zone=pending['zone'],before=pending['clips'],after=ammo,
                    requested_at=pending['started'],observed_at=now,
                    note='Actual ammo debit only; native Echo/Core/damage receipts still own success'))
                self.precision_pending=None
                self.precision_release_until=now+.2
                row['status']='observed_ammunition_release'
                self.report['precision_input_gate']=row
                return False,(0.,0.),look,row
            assert now-pending['started']<2., 'Ordinary precision trigger consumed no ammunition within2seconds'
            # Recheck eligibility each frame. Never steer/move on a frame which
            # may still deliver the earlier ordinary trigger to native input.
            fire=bool(now<pending['until'] and key==pending['key'] and eligible and input_ready
                and actual_ray['matched_unbroken_zone'] and speed<5. and spread<=.05)
            self.report['precision_input_gate']=row
            return fire,(0.,0.),(0.,0.) if now<pending['until'] else look,row
        if now<getattr(self,'precision_release_until',0.):
            row['status']='release_interval'
            self.report['precision_input_gate']=row
            return False,(0.,0.),look,row
        locked=bool(eligible and input_ready and actual_ray['matched_unbroken_zone']
            and not any(abs(value)>.001 for value in move))
        quiet=bool(locked and speed<5. and spread<=.05)
        previous=getattr(self,'precision_previous',None)
        consecutive=bool(quiet and previous is not None and previous['quiet']
            and previous['key']==key and 0.<now-previous['at']<=.1)
        samples=previous['samples']+1 if consecutive else 1 if quiet else 0
        if not consecutive:
            self.precision_ready_since=now if quiet else None
        self.precision_previous=dict(key=key,at=now,quiet=quiet,samples=samples)
        settled=0. if self.precision_ready_since is None else now-self.precision_ready_since
        fire=quiet and samples>=3 and settled>=.05
        row.update(status='trigger' if fire else 'tracking_hits' if locked else 'acquiring',
            consecutive_hit_seconds=settled,consecutive_hit_samples=samples)
        if fire:
            self.precision_pending=dict(key=key,target=_path(target),zone=point[0],clips=ammo,started=now,until=now+.05)
            self.precision_ready_since=None
            self.precision_previous=None
        self.report['precision_input_gate']=row
        # Ordinary tracking remains enabled while observing moving zones. Only
        # the frame that requests a shot and its short pulse freeze the camera.
        return bool(fire),(0.,0.) if locked else move,(0.,0.) if fire else look,row

    def earn_echo(self,pc,pawn,weapon):
        candidates=[]
        for row in self.e4.participants:
            actor=row.character
            if str(row.participant_id) in EARN_TARGETS and rescue.alive(actor) and not actor.get_editor_property('hidden') and actor.get_actor_enable_collision():
                for point in self.shot_points(actor):
                    candidates.append((actor,point))
        assert candidates, 'No released eligible conventional target has a usable unbroken native weak point; no Echo was supplied'
        candidates.sort(key=lambda p:(p[0]!=self.target,
            math.dist(_xyz(p[0].get_actor_location()),_xyz(pawn.get_actor_location())),
            'head' not in p[1][1].lower(),p[1][0],p[1][1],_path(p[1][2])))
        selected=None
        for actor,point in candidates:
            exposed,desired_ray=self.precision_ray(pawn,actor,point,True)
            if selected is None: selected=(actor,point,exposed,desired_ray)
            if exposed:
                selected=(actor,point,exposed,desired_ray); break
        actor,point,clear,desired_ray=selected
        self.target=actor
        identity=str(self.e4.find_participant_id(actor))
        assert identity in EARN_TARGETS, 'Never shoot Weaver, Elite or protected people to manufacture a phase result'
        look,error=self.look(self.world,pc,point[3])
        actual_match,actual_ray=self.precision_ray(pawn,actor,point)
        distance=math.dist(_xyz(pawn.get_actor_location()),_xyz(point[3]))
        in_range=distance<min(2200.,max(500.,weapon.get_attack_range()*.75))
        move=self.approach(self.world,pc,pawn,actor) if (not clear or not in_range) and distance>450. else (0.,0.)
        clip,reserve=weapon.get_ammo_in_clip(),weapon.get_spare_ammo()
        assert clip>0 or reserve>0, 'Existing Axiom ammunition exhausted; no refill was issued'
        game_time=unreal.GameplayStatics.get_time_seconds(self.world)
        reload_input=1. if clip<=0 and game_time%1.2<.15 else 0.
        eligible=clip>0 and clear and actual_match and in_range
        fire,move,look,input_gate=self.settled_precision_input(pawn,weapon,actor,point,move,look,error,eligible,actual_ray)
        if fire and not getattr(self,'was_firing',False):
            self.shot_count+=1
            self.report['precision_shots'].append(dict(target=identity,zone=point[0],bone=point[1],mesh=_path(point[2]),
                point=_xyz(point[3]),echo_before=self.echo.get_echo(),clip=clip,elapsed=time.monotonic()-self.started,
                actual_weapon_ray=actual_ray,input_gate=input_gate))
        self.was_firing=fire
        assert self.shot_count<=40, 'Bounded ordinary precision attempts exhausted; native Echo generation must be inspected'
        self.report['last_precision']=dict(target=identity,zone=point[0],bone=point[1],clear=clear,error=error,
            distance=distance,in_range=in_range,health=actor.get_health(),echo=self.echo.get_echo(),clip=clip,reserve=reserve,
            actual_zone_hit=actual_match,desired_weapon_ray=desired_ray,actual_weapon_ray=actual_ray,input_gate=input_gate)
        self.inject(move=move,look=look,attack=float(fire),aim=0. if clip<=0 else 1.,reload=reload_input)

    def prepare_pulse(self,pc,pawn):
        assert rescue.alive(self.weaver), 'Actual Weaver died before both required severs'
        target=self.weaver.get_actor_location()
        look,error=self.look(self.world,pc,target)
        eyes,unused=pawn.get_actor_eyes_view_point()
        distance=math.dist(_xyz(eyes),_xyz(target))
        clear=self.clear_point(pawn,self.weaver,target,eyes)
        self.report['last_pulse_aim']=dict(origin=_xyz(eyes),target=_xyz(target),distance=distance,
            camera_angle_error=error,visibility_clear=clear,echo=self.echo.get_echo())
        if not clear or distance>3400.:
            move=self.approach(self.world,pc,pawn,self.weaver) if distance>450. else (0.,0.)
            self.inject(move=move,look=look)
            return
        self.inject(look=look)
        if error<2.:
            if self.echo.get_echo()<30.:
                self.stage('earn_echo')
                return
            self.pulse_before=dict(echo=self.echo.get_echo(),receipts=len(self.report['link_receipts']),
                spends=len(self.report['echo_spends']),links=self.link_rows(),position=_xyz(pawn.get_actor_location()))
            self.pulse_started=unreal.GameplayStatics.get_time_seconds(self.world)
            self.stage('hold_pulse')

    def hold_pulse(self,pc,pawn):
        elapsed=unreal.GameplayStatics.get_time_seconds(self.world)-self.pulse_started
        look,error=self.look(self.world,pc,self.weaver.get_actor_location())
        receipts=self.report['link_receipts'][self.pulse_before['receipts']:]
        if receipts or elapsed>=.85:
            self.inject()
            self.stage('wait_pulse_result')
        else:
            self.inject(look=look,pulse=1.)

    def confirm_pulse(self):
        self.inject()
        receipts=self.report['link_receipts'][self.pulse_before['receipts']:]
        spends=self.report['echo_spends'][self.pulse_before['spends']:]
        if not receipts:
            if time.monotonic()-self.phase_at<3.:
                return
            # A moving enemy can obstruct a charged pulse after the initial aim
            # check. Preserve the real miss/cost, release input and earn any
            # needed Echo normally before another bounded attempt.
            links=self.link_rows()
            assert links==self.pulse_before['links'], 'Unobserved link mutation cannot count as a pulse retry'
            assert len(spends)<=1 and all(abs(s['amount']-30.)<.001 for s in spends), 'Unexpected pulse spending'
            miss=dict(aim=self.report.get('last_pulse_aim'),before=self.pulse_before,
                spends=spends,links=links,elapsed=time.monotonic()-self.started)
            self.report['missed_pulses'].append(miss)
            assert len(self.report['missed_pulses'])<=2, 'Bounded ordinary pulse retries exhausted: '+json.dumps(miss)
            self.stage('aim_pulse')
            return
        assert len(receipts)==1, 'One Weaver pulse must commit only one of its independent links'
        receipt=receipts[0]
        assert receipt['id']==receipt['expected'] and receipt['id'] in LINK_IDS
        assert receipt['instance']==self.link_instances[receipt['id']] and entry.valid_guid(receipt['transaction'])
        assert receipt['owner']==receipt['source']==_path(self.weaver) and receipt['severer']==self.initial_pawn and receipt['reward_eligible']
        assert len(spends)==1 and abs(spends[0]['amount']-30.)<.001, 'Pulse must pay one real native 30 Echo cost'
        assert len({r['transaction'] for r in self.report['link_receipts']})==len(self.report['link_receipts'])
        assert len({r['id'] for r in self.report['link_receipts']})==len(self.report['link_receipts']), 'A retired link was replayed'
        self.report['pulses'].append(dict(before=self.pulse_before,receipt=receipt,spend=spends[0],
            input_game_seconds=unreal.GameplayStatics.get_time_seconds(self.world)-self.pulse_started,
            echo_after=self.echo.get_echo(),links_after=self.link_rows()))
        self.stage('wait_phase_a' if len(self.report['link_receipts'])==2 else 'wait_wallrunner')

    def aim_handoff(self,pc,pawn):
        actor=self.handoff_request;component=actor.interactable;interaction=pc.get_interaction_component()
        look,error=self.look(self.world,pc,actor.get_actor_location())
        admission=component.can_interact(pawn,interaction)
        focus=interaction.get_editor_property('viewed_interactable')
        self.report['last_interaction']=dict(actor=_path(actor),focus=_path(focus),admitted=admission is not None,
            admission=str(admission),native_action_text=str(component.get_interactable_action_text(pawn,interaction)),angle_error=error,request=_xyz(actor.get_actor_location()),
            anchor=_xyz(actor.handoff_anchor.get_actor_location()),player=_xyz(pawn.get_actor_location()),
            native_hold=float(component.interaction_time),range=float(component.interaction_distance),
            anchor_range=float(actor.handoff_anchor.request_range),last_result=str(actor.last_result))
        self.inject(look=look)
        if error<3. and admission is not None and focus==component:
            self.unbind_request();self.hold_actor=actor;self.hold_started=unreal.GameplayStatics.get_time_seconds(self.world)
            self.saw_countdown=False;self.request_result=None
            def result(accepted,message):
                self.request_result=dict(beat=HANDOFF,accepted=accepted,message=str(message),elapsed=time.monotonic()-self.started)
                self.report['request_results'].append(self.request_result)
            self.request_delegate,self.request_callback=actor.on_request_result,result
            self.request_delegate.add_callable(self.request_callback)
            self.stage('hold_handoff')

    def hold_handoff(self,pc,state):
        remaining=float(pc.get_interaction_component().get_editor_property('remaining_interact_time'))
        if 0.<remaining<=.35:
            self.saw_countdown=True
        accepted=self.handoff_request.is_request_pending() or self.request_result is not None or pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.SWITCHING or state.is_beat_complete(unreal.Name(MISSION),unreal.Name(HANDOFF))
        if accepted:
            self.inject()
            assert self.saw_countdown, 'Native authored hold countdown was not observed'
            self.report['holds'].append(dict(beat=HANDOFF,seconds=.35,native_countdown=True,
                input_game_seconds=unreal.GameplayStatics.get_time_seconds(self.world)-self.hold_started))
            self.stage('wait_phase_b')
        else:
            assert time.monotonic()-self.phase_at<8., 'Actual handoff hold failed: '+json.dumps(self.report['last_interaction'])
            self.inject(interact=1.)

    def confirm_phase_b(self,pc,pawn,events):
        self.inject()
        if [e['beat'] for e in events]!=FINAL or not isinstance(pawn,unreal.SovTarrikCharacter) or not pawn.is_character_ready():
            return
        assert _path(pawn)!=self.initial_pawn and pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.IDLE
        assert entry.valid_guid(events[-1]['handoff']) and events[-1]['anchor']=='M12_TarrikCrucible'
        if self.e4b.get_encounter_state()!=unreal.SovEncounterState.ACTIVE:
            return
        companion=self.owned_companion(pc,pawn,'Selene')
        if companion is None:
            return
        assert _path(companion)!=self.current_companion_path
        rows=self.roster_for(self.e4b)
        expected={r['id']:r['actor'] for r in self.report['native_phase_a']['roster'] if r['alive']}
        assert {r['id']:r['actor'] for r in rows}==expected, 'Phase B must transfer the exact surviving actors'
        assert all(r['alive'] for r in rows) and not list(self.e4.participants)
        assert self.e4b.has_encounter_player(pawn) and entry.valid_guid(self.e4b.get_attempt_id().export_text())
        assert self.e4b.get_attempt_id().export_text()!=self.attempt and not self.e4b.has_confirmed_victory()
        assert not self.phase_b_objective.is_result_pending()
        records=[s for s in self.report['saves'] if s['checkpoint'] and s['arena_entry'] and s['boundary']==PHASE_B]
        if not records:
            return
        assert len(records)==1 and records[0]['success'], 'Native phase-B checkpoint failed or was replayed'
        saved=records[0]
        assert saved['slot_index']==0 and saved['mission']==MISSION and saved['map']=='/Game/Aurelion/Maps/L_Aurelion_M12'
        assert saved['journal']==FINAL and 'Tarrik' in saved['protagonist']
        self.report['native_phase_b']=dict(attempt=self.e4b.get_attempt_id().export_text(),pawn=_path(pawn),
            companion=_path(companion),roster=rows,journal=events,entry_checkpoint=saved,
            checkpoint_identity='ArenaEntry / '+PHASE_B+' (the route design calls this CP5b)',
            ready=True,health=pawn.get_health(),no_thermal_receipt=True,readback_claim='Native success callback only; no LoadSlot request')
        self.finish(True,'Normal precision fire earned any needed Echo; two ordinary Axiom inputs produced the exact two native Weaver receipts. Native WallRunner release and phase freeze preceded the real held Tarrik handoff, verified ArenaEntry checkpoint and transfer of the same survivors into active E4B. Thermal Fracture and later route remain unqualified.')

    def tick(self,delta):
        if self.done:
            return
        try:
            now=time.monotonic()
            assert now-self.started<600., 'E4A continuation exceeded ten-minute bound'
            assert now-self.phase_at<(150. if self.phase in ('earn_echo','aim_pulse','walk_route') else 65.), 'Stage deadline: '+self.phase
            world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            assert world and '/Aurelion/Maps/UEDPIE_' in _path(world) and 'L_Aurelion_M12' in world.get_name()
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            assert pc is not None
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);state=pc.get_campaign_state()
            assert state and state.get_active_mission() and str(state.get_active_mission().mission_id)==MISSION
            events=entry.journal(state)
            assert len(events)<=len(FINAL) and [e['beat'] for e in events]==FINAL[:len(events)]
            assert all(e['mission']==MISSION for e in events)
            if self.phase=='initialize':
                self.initialize(world,pc,pawn,state,events)
            assert world==self.world and pc==self.initial_controller and events[:len(INITIAL)]==self.initial_events
            assert self.e4.get_attempt_id().export_text()==self.attempt, 'Phase A was retried or replaced'
            assert self.e4.get_encounter_state() in (unreal.SovEncounterState.ACTIVE,unreal.SovEncounterState.SUCCEEDED)
            assert not str(self.e4.last_phase_error), 'Native phase-A error: '+str(self.e4.last_phase_error)
            assert self.e4b.get_encounter_state() in (unreal.SovEncounterState.INACTIVE,unreal.SovEncounterState.ACTIVE)
            assert not self.saves.is_awaiting_failure_decision() and not self.saves.is_load_pending() and not self.saves.is_mission_travel_pending()
            assert pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.FAILED
            if self.request_result is not None:
                assert self.request_result['accepted'], 'Native handoff rejected: '+self.request_result['message']
            rows=self.roster_for(self.e4) if list(self.e4.participants) else self.roster_for(self.e4b)
            self.check_roster_identities(rows)
            assert all(r['alive'] for r in rows if not r['required'] or r['id'] in ('E4.Elite','E4.Weaver'))
            assert {r['id'] for r in rows if not r['required']}==prior.PROTECTED
            if not pawn:
                assert self.phase in ('hold_handoff','wait_phase_b')
                self.inject();return
            assert rescue.alive(pawn), 'Controlled player died; no retry/recovery was issued'
            if self.phase not in ('hold_handoff','wait_phase_b'):
                assert _path(pawn)==self.current_pawn_path and isinstance(pawn,unreal.SovSeleneCharacter)
                assert rescue.alive(self.companion) and self.companion.get_owner()==pc and not self.companion.get_companion_component().is_disabled()
            if now-self.last_sample>.5:
                self.last_sample=now
                self.report['samples'].append(dict(elapsed=now-self.started,phase=self.phase,pawn=_path(pawn),
                    location=_xyz(pawn.get_actor_location()),health=pawn.get_health(),ready=pawn.is_character_ready(),
                    echo=self.echo.get_echo() if self.phase not in ('hold_handoff','wait_phase_b') else None,
                    links=self.link_rows(),roster=rows,journal=events,wave=self.coordination.get_current_wave()))
            if now-self.last_write>1.:
                self.last_write=now;self.write()
            if self.phase=='select_axiom':
                held,report=self.selector.step(world)
                self.inject(wheel_hold=float(held))
                if self.selector.done:
                    self.report['wheels'].append(report)
                    assert report['status']=='passed', 'Normal weapon wheel failed: '+report.get('reason','')
                    self.stage('aim_pulse')
                return
            if self.phase=='hold_handoff':
                self.hold_handoff(pc,state);return
            if self.phase=='wait_phase_b':
                self.confirm_phase_b(pc,pawn,events);return
            if not pawn.is_character_ready() or not state.is_state_valid() or unreal.GameplayStatics.is_game_paused(world) or pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.IDLE:
                self.inject();return
            if self.phase in ('aim_pulse','earn_echo','hold_pulse'):
                weapon=self.actual_axiom(pawn)
            if self.phase=='aim_pulse':
                self.prepare_pulse(pc,pawn)
            elif self.phase=='earn_echo':
                if self.echo.get_echo()>=30.:
                    self.inject();self.stage('aim_pulse')
                else:
                    self.earn_echo(pc,pawn,weapon)
            elif self.phase=='hold_pulse':
                self.hold_pulse(pc,pawn)
            elif self.phase=='wait_pulse_result':
                self.confirm_pulse()
            elif self.phase=='wait_wallrunner':
                self.inject()
                if self.coordination.get_current_wave()==1:
                    runner=self.e4.get_participant(unreal.Name('E4.WallRunner'))
                    assert rescue.alive(runner) and not runner.get_editor_property('hidden') and runner.get_actor_enable_collision()
                    self.report['wallrunner_release']=dict(native_wave=1,actor=_path(runner),position=_xyz(runner.get_actor_location()),receipts=list(self.report['link_receipts']))
                    self.stage('aim_pulse')
            elif self.phase=='wait_phase_a':
                self.inject()
                if self.e4.get_encounter_state()==unreal.SovEncounterState.SUCCEEDED and len(events)==len(INITIAL)+1:
                    assert self.e4.has_confirmed_victory() and not self.e4_entry.is_result_pending()
                    assert events[-1]['encounter']==prior.E4_ID and events[-1]['attempt']==self.attempt
                    assert self.coordination.get_current_wave()==1 and len(self.report['link_receipts'])==2
                    assert self.report.get('wallrunner_release') is not None
                    self.report['native_phase_a']=dict(attempt=self.attempt,receipt=events[-1],links=self.link_rows(),roster=rows,
                        confirmed_before_transfer=True,wave=1,protected_alive=True,elite_alive=True)
                    p=self.handoff_request.get_actor_location()
                    self.begin_route([(p.x+20.,p.y-190.,p.z+20.)],'aim_handoff')
            elif self.phase=='walk_route':
                self.walk(pc,pawn)
            elif self.phase=='aim_handoff':
                self.aim_handoff(pc,pawn)
        except Exception:
            self.report['error']=traceback.format_exc()
            self.finish(False,self.report['error'])


def start(output_directory=None):
    global _RUN
    assert _RUN is None or _RUN.done, 'E4A driver already runs'
    for name in ('continue_aurelion_e1_input','continue_aurelion_e2_input','continue_aurelion_e3_entry_input',
                 'continue_aurelion_e3_rescue_input','continue_aurelion_e4_entry_input'):
        module=sys.modules.get(name);run=getattr(module,'_RUN',None) if module else None
        assert run is None or run.done, 'Stop earlier input owner first: '+name
    output=Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (output/'e4a-input-continuation.json').exists(), 'Use a fresh evidence directory'
    _RUN=Run(output);_RUN.write()
    _RUN.handle=unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN


def stop():
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False,'Stopped by operator; all ordinary inputs released, no recovery or proof repair issued')


if __name__=='__main__':
    start()
