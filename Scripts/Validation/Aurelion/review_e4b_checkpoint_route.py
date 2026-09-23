"""Restore unchanged earned ArenaEntry banks, then observe ordinary E4B inputs.

No actor, health, ability, collision or campaign proof writes. This is checkpoint
replay coverage, not a new full mission run. Preserve terminal driver evidence.
"""
import hashlib,json,math,os,shutil,sys,time,traceback
from pathlib import Path
import unreal

project=Path(unreal.Paths.project_dir())
sys.path.insert(0,str(project/'Scripts/Validation/Aurelion'))
import continue_aurelion_e4b_input as pilot
import observe_companion_contribution as companion_audit
from aurelion_retry_input import RetryInput
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=Path(os.environ.get('SOV_AURELION_E4B_SOURCE',
    str(project/'Saved/Validation/Aurelion/PhaseHaloMissionRoute-20260920-144741-b8a1800f'))).resolve()
prior_path=source/'E1Continuation/route-follow-on/continue_aurelion_e4a_input/e4a-input-continuation.json'
if prior_path.exists():
    prior=json.loads(prior_path.read_text())
    assert prior['status']=='passed'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.SovGameUserSettings.get_game_user_settings().complete_accessibility_setup()
report=dict(status='running',scope=__doc__,source=str(source),banks=[],callbacks=[],obstructions=[])
state=dict(phase='bootstrap',start=time.monotonic(),at=0.,busy=False)

def write(): (out/'e4b-checkpoint-review.json').write_text(json.dumps(report,indent=2))
def loaded(result,header,message):
    report['callbacks'].append(dict(result=str(result),header=header.export_text(),message=str(message)));write()
def finish(error=None):
    report['status']='failed' if error else 'passed_requires_visual_review'
    if error:report['error']=error
    if state.get('delegate'):state['delegate'].remove_callable(loaded)
    if state.get('driver') and not state['driver'].done:pilot.stop()
    if state.get('companion_audit'):state['companion_audit'].stop('E4B checkpoint replay finished')
    if state.get('retry'):state['retry'].stop();state['retry']=None
    write();level.editor_request_end_play();state['phase']='stopping'

def observe_obstruction(world,pawn,driver,now):
    if driver.phase!='walk_route' or not driver.waypoints or now-state['at']<.5:return
    state['at']=now
    cap=pawn.get_editor_property('capsule_component');start=pawn.get_actor_location()
    target=unreal.Vector(*driver.waypoints[0]);delta=target-start;delta.z=0.
    length=delta.length()
    if length<1:return
    end=start+delta*(min(length,180.)/length)
    hit=unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,
        cap.get_scaled_capsule_radius()-2.,cap.get_scaled_capsule_half_height()-2.,
        'Pawn',False,[pawn],unreal.DrawDebugTrace.NONE,True)
    if not hit:return
    values=hit if isinstance(hit,tuple) else (hit,)
    hits=[v for v in values if isinstance(v,unreal.HitResult)]
    assert len(hits)==1
    parts=hits[0].to_tuple()
    report['obstructions'].append(dict(elapsed=now-state['start'],control=driver.control_name,
        player=start.export_text(),velocity=pawn.get_velocity().export_text(),destination=target.export_text(),
        actor=parts[9].get_path_name() if parts[9] else None,blocking=bool(parts[0]),
        initial_overlap=bool(parts[1]),hit=hits[0].export_text()))
    write()

def capture_camera_context(world,pawn):
    manager=unreal.GameplayStatics.get_player_camera_manager(world,0)
    if not manager:return {'error':'No player camera manager at terminal capture'}
    eye=manager.get_camera_location()
    nearby=[]
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.NarrativeCharacter):
        distance=(actor.get_actor_location()-eye).length()
        origin,extent=actor.get_actor_bounds(False)
        gaps=[max(0.,abs(getattr(eye,axis)-getattr(origin,axis))-getattr(extent,axis)) for axis in ('x','y','z')]
        bounds_distance=math.sqrt(sum(gap*gap for gap in gaps))
        if bounds_distance>500 or actor.get_editor_property('hidden'):continue
        capsule=actor.get_component_by_class(unreal.CapsuleComponent)
        meshes=[]
        for mesh in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            center,half,_=unreal.SystemLibrary.get_component_bounds(mesh)
            visual_gaps=[max(0.,abs(getattr(eye,axis)-getattr(center,axis))-getattr(half,axis)) for axis in ('x','y','z')]
            asset=mesh.get_skeletal_mesh_asset()
            meshes.append(dict(component=mesh.get_name(),asset=asset.get_path_name() if asset else None,
                bounds_distance=round(math.sqrt(sum(gap*gap for gap in visual_gaps)),1),
                origin=center.export_text(),extent=half.export_text(),visible=mesh.is_visible()))
        nearby.append(dict(actor=actor.get_path_name(),class_name=actor.get_class().get_name(),
            eye_distance=round(distance,1),player_distance=round(actor.get_distance_to(pawn),1),
            position=actor.get_actor_location().export_text(),alive=actor.is_alive(),
            bounds_distance=round(bounds_distance,1),bounds_origin=origin.export_text(),bounds_extent=extent.export_text(),meshes=meshes,
            capsule_profile=str(capsule.get_collision_profile_name()) if capsule else None,
            capsule_camera_response=str(capsule.get_collision_response_to_channel(unreal.CollisionChannel.cast(4))) if capsule else None))
    nearby.sort(key=lambda row:row['bounds_distance'])
    forward=unreal.MathLibrary.get_forward_vector(manager.get_camera_rotation())
    hit=unreal.SystemLibrary.line_trace_single(world,eye,eye+forward*500.,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
    hits=[value for value in (hit if isinstance(hit,tuple) else (hit,)) if isinstance(value,unreal.HitResult)]
    center_hit=hits[0].to_tuple()[9].get_path_name() if hits and hits[0].to_tuple()[9] else None
    return dict(eye=eye.export_text(),rotation=manager.get_camera_rotation().export_text(),
        player=pawn.get_actor_location().export_text(),player_eye_distance=round((pawn.get_actor_location()-eye).length(),1),
        center_hit=center_hit,nearby=nearby)

def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='stopping':
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        if state['phase']=='capture':
            if now-state['at']>4:finish(state.get('error'))
            return
        assert now-state['start']<900,'Checkpoint replay timeout'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn=unreal.GameplayStatics.get_player_pawn(world,0) if world else None
        if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready() or pawn.is_character_pending_load():return
        if state['phase']=='bootstrap':
            instance=unreal.GameplayStatics.get_game_instance(world)
            saves=next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance)
            if saves.is_load_pending():return
            banks=sorted((source/'UserData/Saved/SaveGames').glob('*_2_0_*.sav'));assert len(banks)==2
            destination=(out/'UserData/Saved/SaveGames').resolve();assert destination.is_relative_to(out.resolve())
            destination.mkdir(parents=True,exist_ok=True)
            for bank in banks:
                target=destination/bank.name
                if target.exists():shutil.copy2(target,out/(bank.name+'.bootstrap'))
                digest=hashlib.sha256(bank.read_bytes()).hexdigest();shutil.copy2(bank,target)
                assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
                report['banks'].append(dict(name=bank.name,sha256=digest))
            state.update(saves=saves,delegate=saves.on_load_completed,old_world=hash(world),phase='load')
            state['delegate'].add_callable(loaded)
            result,message=saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT,0)
            assert result==unreal.SovSaveResult.LOAD_STARTED,str(message)
            write();return
        if state['phase']=='load':
            if hash(world)==state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:return
            assert 'SUCCESS' in report['callbacks'][-1]['result'],report['callbacks']
            assert 'BoundaryId="M12_E4_QuarantineCrucibleB"' in report['callbacks'][-1]['header'], 'Source is not an earned E4B checkpoint'
            assert isinstance(pawn,unreal.SovTarrikCharacter)
            directors=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionThermalPhaseDirector)
                if str(a.encounter_id)==pilot.ENCOUNTER]
            assert len(directors)==1
            report['restored_encounter_state']=str(directors[0].get_encounter_state())
            if directors[0].get_encounter_state()==unreal.SovEncounterState.FAILED:
                state.update(retry=RetryInput(world,directors[0],out/'RetryInput'),phase='retry');write();return
            state.update(driver=pilot.start(out/'E4B'),phase='drive');return
        if state['phase']=='retry':
            retry=state['retry'];pc=unreal.GameplayStatics.get_player_controller(world,0)
            done=retry.step(world,pc,pawn)
            report['retry_samples']=retry.samples;write()
            if done:
                report['retry_input_frames']=retry.driver.report['input_frames'].copy()
                retry.stop();state['retry']=None
                state.update(phase='binding_readiness',ready_at=now)
            return
        if state['phase']=='binding_readiness':
            director=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionThermalPhaseDirector)
                if str(a.encounter_id)==pilot.ENCOUNTER)
            elite=director.get_participant(unreal.Name('E4.Elite'))
            thermal=elite.get_thermal_fracture() if elite else None
            controls=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionRequestActor)
                if str(a.request_id) in ('Aurelion.E4.MovePartner','Aurelion.E4.FrostSetup','Aurelion.E4.HeatConfirm')]
            def path(obj):return obj.get_path_name() if obj else None
            row=dict(elapsed=now-state['ready_at'],encounter_state=str(director.get_encounter_state()),
                director=path(director),elite=path(elite),elite_alive=elite.is_alive() if elite else False,
                thermal=path(thermal),configured_director=path(thermal.encounter_director) if thermal else None,
                frost_anchor=path(thermal.frost_anchor) if thermal else None,
                frost_anchor_id=str(thermal.frost_anchor_id) if thermal else None,
                last_error=str(thermal.last_error) if thermal else None,
                controls=[dict(id=str(a.request_id),resolved=path(a.get_current_thermal_target())) for a in controls])
            if now-state['at']>.5:
                state['at']=now;report.setdefault('binding_readiness',[]).append(row);write()
            if thermal and thermal.encounter_director==director and thermal.frost_anchor and len(controls)==3 and all(a.get_current_thermal_target()==thermal for a in controls):
                if os.environ.get('SOV_E4B_COMPANION_AUDIT') == '1':
                    state['companion_audit']=companion_audit.start(out/'companion-contribution.json')
                state.update(driver=pilot.start(out/'E4B'),phase='drive');return
            if now-state['ready_at']>8:
                state['error']='Native retry did not restore the Elite thermal director/anchor bindings within eight seconds: '+json.dumps(row)
                unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/'missing-thermal-bindings.png'))
                state.update(phase='capture',at=now)
            return
        if state['phase']=='drive':
            driver=state['driver']
            if driver.done:
                report['driver_status']=driver.report['status'];report['driver_reason']=driver.report['reason']
                state['error']=driver.report['reason'] if driver.report['status']!='passed' else None
                report['camera_context']=capture_camera_context(world,pawn)
                unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/'terminal.png'))
                state.update(phase='capture',at=now);write();return
            observe_obstruction(world,pawn,driver,now)
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
write();level.editor_request_begin_play()
