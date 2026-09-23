"""Load an earned E4A checkpoint and test ordinary repeat fire plus companion response.

The source bank is copied into the runner's isolated profile. The player uses
the normal wheel, aim and attack actions; the companion uses FocusTarget. No
damage, inventory, actor transforms, mission receipts or abilities are supplied.
"""
import hashlib,json,os,runpy,time,traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
os.environ['SOV_CONTACT_RETRY_ENTRY']='1'  # Ordinary checkpoint retry input activates the saved arena.
maps=[root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
map_hashes={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in maps}
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report=dict(status='loading',scope=__doc__,source=os.environ.get('SOV_EARNED_COMPANION_SAVE_DIRECTORY'),
    maps_before=map_hashes)
state=dict(start=time.monotonic(),phase='probe',target_set=False)

def write(): (out/'earned-e4a-repeat-contact.json').write_text(json.dumps(report,indent=2))
def blood_receipt(result):
    if result.applied_health_damage<=0 or state.get('capture_requested'):return
    import observe_companion_after_player_shot as contact
    probe=contact._RUN
    if not probe or not probe.world:return
    capture=out/'native-eclipse-hit-ui.png'
    state.update(capture_requested=True,capture_at=time.monotonic())
    report['blood_capture']=dict(target=result.target_actor.get_path_name() if result.target_actor else None,
        receipt=result.export_text(),image=str(capture))
    write()
def sample_blood_components():
    world=state.get('world')
    if not world:return
    entries=[]
    for component in unreal.ObjectIterator(unreal.NiagaraComponent):
        if component.get_world()!=world:continue
        try:system=component.get_editor_property('asset')
        except Exception:system=None
        if not system or '/Blood/' not in system.get_path_name():continue
        entries.append(dict(component=component.get_path_name(),system=system.get_path_name(),
            active=component.is_active(),location=component.get_world_location().export_text()))
    report.setdefault('blood_component_samples',[]).append(dict(elapsed=time.monotonic()-state['capture_at'],entries=entries))
def finish(error=None):
    if state['phase']=='stopping':return
    import observe_companion_after_player_shot as contact
    probe=contact._RUN
    if probe:
        report['contact']=probe.report
        if probe.handle is not None:probe.stop('Earned contact wrapper ended')
    for delegate in state.get('blood_delegates',[]):delegate.remove_callable(blood_receipt)
    state['blood_delegates']=[]
    report.update(status='failed' if error else 'observed_requires_review',error=error,
        maps_unchanged=all(hashlib.sha256(path.read_bytes()).hexdigest()==map_hashes[str(path)] for path in maps))
    write();level.editor_request_end_play();state.update(phase='stopping',at=time.monotonic())

def tick(dt):
    if state['phase']=='stopping':
        if not level.is_in_play_in_editor() and time.monotonic()-state['at']>2:
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        return
    try:
        assert time.monotonic()-state['start']<360,'Earned E4A contact deadline exceeded'
        import observe_companion_after_player_shot as contact
        probe=contact._RUN
        if not probe:return
        if (state.get('capture_requested') and time.monotonic()-state['capture_at']<2
                and time.monotonic()-state.get('last_blood_sample',0)>.04):
            state['last_blood_sample']=time.monotonic();sample_blood_components()
            if not state.get('shot_requested'):
                state['shot_requested']=True
                unreal.SystemLibrary.execute_console_command(state['world'],
                    'Shot showui -nosuffix filename='+str(out/'native-eclipse-hit-ui.png'))
        if (os.environ.get('SOV_BLOOD_GATE_ONLY')=='1' and state.get('capture_requested')
                and time.monotonic()-state['capture_at']>1.0):
            capture=out/'native-eclipse-hit-ui.png'
            if capture.exists() and capture.stat().st_size>0:
                report.update(blood_gate_only=True,blood_capture_file_exists=True,
                    player_damage=probe.report.get('player_damage_details',[]),
                    companion_damage=probe.report.get('companion_damage_details',[]))
                finish();return
        if not state['target_set']:
            assert probe.handle is not None and not probe.report.get('trigger_states'), 'Player fired before target selection'
            elites=[actor for actor in unreal.GameplayStatics.get_all_actors_of_class(probe.world,unreal.NarrativeNPCCharacter)
                if 'AurelionElite' in actor.get_class().get_name() and actor.is_alive()
                and actor.get_health()>150 and probe.pc.line_of_sight_to(actor)
                and unreal.ArsenalStatics.get_attitude(probe.pawn,actor)==unreal.TeamAttitude.HOSTILE
                and probe.pawn.get_distance_to(actor)<5000]
            assert len(elites)==1,'Expected one living visible Elite for nonfatal player shots'
            capture_actors=(probe.target,) if os.environ.get('SOV_BLOOD_CAPTURE_COMPANION')=='1' else (probe.target,elites[0])
            state['blood_delegates']=[actor.get_narrative_ability_system_component().on_damage_resolved_as_target
                for actor in capture_actors]
            for delegate in state['blood_delegates']:delegate.add_callable(blood_receipt)
            state['world']=probe.world
            probe.player_target=elites[0];state['target_set']=True
            report.update(player_target=elites[0].get_path_name(),companion_target=probe.target.get_path_name())
            blood=probe.target.get_component_by_class(unreal.SovBloodFeedbackComponent)
            report['target_blood_component']=dict(path=blood.get_path_name() if blood else None)
            if blood:
                for prop in ('enabled','black_blood','systems'):
                    try:report['target_blood_component'][prop]=str(blood.get_editor_property(prop))
                    except Exception as error:report['target_blood_component'][prop+'_error']=str(error)
            write();return
        if probe.handle is not None:return
        hits=[r for r in probe.report.get('player_damage_details',[])
            if r['target']==report['player_target'] and r['health']+r['shield']>0 and r['target_alive']]
        follow=[r for r in probe.report.get('companion_damage_details',[])
            if hits and r['target']==report['companion_target'] and r['health']>0 and r['elapsed']>hits[0]['elapsed']]
        capture=out/'native-eclipse-hit-ui.png'
        if follow and state.get('capture_requested'):
            if not capture.exists() and time.monotonic()-state['capture_at']<15:return
        report.update(player_hits=hits,companion_hits_after_player=follow,
            trigger_states=probe.report.get('trigger_states',[]),
            blood_capture_file_exists=capture.exists() and capture.stat().st_size>0,
            player_ammo_samples=[dict(elapsed=s['elapsed'],ammo=s['player_weapon_ammo']) for s in probe.report['samples']
                if s.get('player_weapon_ammo') is not None])
        finish(None if hits and follow and report['blood_capture_file_exists'] and not probe.report['errors'] else
            'No verified player hit and later companion health damage; inspect ordinary shots and attack path')
    except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick);write()
runpy.run_path(str(Path(__file__).with_name('start_earned_companion_contact_probe.py')))
