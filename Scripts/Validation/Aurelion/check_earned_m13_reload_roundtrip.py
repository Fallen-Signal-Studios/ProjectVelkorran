"""Public CP9 loads against the latest earned unpaced route, with HUD readback.

No manufactured progress, character transforms, inventory edits or asset saves.
The first load compares against earned route evidence; the second additionally
checks new native world identities against the first loaded world.
"""
import hashlib,json,math,os,shutil,sys,time,traceback
from pathlib import Path
from types import SimpleNamespace
import unreal

root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
load_count=int(globals().get('M13_RELOAD_COUNT',2));assert load_count in (1,2)
sys.path.insert(0,str(root/'Scripts/Validation/Aurelion'))
import probe_m13_native_checkpoint_reload as check
source=root/'Saved/Validation/Aurelion/CinematicStartupUnpaced-20260920-213727-1184bfea'
source_file=source/'M13/m13-input-continuation.json'
raw=source_file.read_bytes();earned=json.loads(raw)
assert earned['status']=='passed' and earned['retained_gameplay_references_cleared']
expected=earned['native_departure'];assert len(expected['journal'])==35
assert set(earned['completed_scenes'])==set(check.completed_route.SCENES)
assert all(v['completed'] and not v['skipped'] for v in earned['completed_scenes'].values())
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.SovGameUserSettings.get_game_user_settings().complete_accessibility_setup()
maps=[root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in maps}
report=dict(status='running',scope=__doc__,requested_load_count=load_count,source=str(source_file),source_sha256=hashlib.sha256(raw).hexdigest(),banks=[],callbacks=[],loads=[],checkpoint_refreshes=[])
state=dict(phase='bootstrap',start=time.monotonic(),busy=False,index=0)

def write(): (out/'earned-m13-reload.json').write_text(json.dumps(report,indent=2))
def completed(result,header,message):
    report['callbacks'].append(dict(success=result==unreal.SovSaveResult.SUCCESS,header=check.header_data(header),message=str(message)));write()
def saved(result,slot,message):
    if str(slot.boundary_id)!='Aurelion.CP9' or slot.kind!=unreal.SovSaveSlotKind.CHECKPOINT or slot.slot_index!=0:return
    try:
        assert result==unreal.SovSaveResult.SUCCESS,str(message)
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        assert check.completed_route.journal(pc.get_campaign_state())==expected['journal']
        assert check.completed_route.evidence(pc.get_campaign_state())==expected['evidence']
        report['checkpoint_refreshes'].append(dict(generation=slot.generation,boundary=str(slot.boundary_id),
            source_journal_preserved=True,source_evidence_preserved=True))
    except Exception:report['save_observer_error']=traceback.format_exc()
    write()
def finish(error=None):
    report.update(status='failed' if error else 'passed_requires_visual_review',error=error,
        maps_unchanged=all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p)] for p in maps))
    if not report['maps_unchanged']:report['status']='failed'
    if state.get('delegate'):state['delegate'].remove_callable(completed);state['delegate']=None
    if state.get('save_delegate'):state['save_delegate'].remove_callable(saved);state['save_delegate']=None
    write();level.editor_request_end_play();state['phase']='stopping'
def compare_earned(actual):
    assert actual['journal']==expected['journal'] and actual['evidence']==expected['evidence']
    assert actual['player_state']['stable_guid']==expected['profile']['player_state_guid']
    assert actual['companion']['stable_guid']==expected['profile']['companion']['guid']
    for who in ('player','companion'):
        assert actual[who]['items']==expected['profile'][who]['items'],who+' inventory/ammo differs'
        assert actual[who]['resources']==expected['profile'][who]['resources'],who+' resources differ'
        assert math.dist(actual[who]['transform']['location'],expected[who+'_exit'])<=check.LOCATION_TOLERANCE_CM
    assert not actual['paused']
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        if state['phase']=='stopping':
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        now=time.monotonic();assert now-state['start']<360,'Reload probe exceeded its deadline'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:return
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready() or pawn.is_character_pending_load():return
        if state['phase']=='bootstrap':
            instance=unreal.GameplayStatics.get_game_instance(world)
            saves=next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance)
            if saves.is_load_pending():return
            banks=sorted((source/'UserData/Saved/SaveGames').glob('*_2_0_*.sav'));assert len(banks)==2
            dest=(out/'UserData/Saved/SaveGames').resolve();assert dest.is_relative_to(out.resolve());dest.mkdir(parents=True,exist_ok=True)
            for bank in banks:
                target=dest/bank.name
                if target.exists():shutil.copy2(target,out/(bank.name+'.bootstrap'))
                digest=hashlib.sha256(bank.read_bytes()).hexdigest();shutil.copy2(bank,target)
                assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
                report['banks'].append(dict(name=bank.name,sha256=digest))
            state.update(saves=saves,delegate=saves.on_load_completed,save_delegate=saves.on_save_completed,phase='request')
            state['delegate'].add_callable(completed);state['save_delegate'].add_callable(saved);write();return
        if state['phase']=='request':
            assert not report.get('save_observer_error'),report.get('save_observer_error')
            if state['index']:
                # Restoration inside the physical CP9 volume intentionally re-arms
                # its one-write-per-visit capture. Admit only a witnessed native
                # refresh retaining the exact earned journal and evidence.
                headers=[h for h in state['saves'].list_slots() if h.kind==unreal.SovSaveSlotKind.CHECKPOINT and h.slot_index==0]
                assert len(headers)==1
                state['expected_header']=check.header_data(headers[0])
                assert any(r['generation']==headers[0].generation for r in report['checkpoint_refreshes'])
            state['old_world']=hash(world)
            result,message=state['saves'].load_slot(unreal.SovSaveSlotKind.CHECKPOINT,0)
            assert result==unreal.SovSaveResult.LOAD_STARTED,str(message)
            state.update(phase='load',at=now);return
        if state['phase']=='load':
            if hash(world)==state['old_world'] or state['saves'].is_load_pending() or len(report['callbacks'])<=state['index']:return
            assert len(report['callbacks'])==state['index']+1 and report['callbacks'][-1]['success']
            header=report['callbacks'][-1]['header']
            assert header['boundary']=='Aurelion.CP9'
            if not state['index']:assert header['generation']==max(v['generation'] for v in earned['checkpoints']['Aurelion.CP9'])
            else:assert header==state['expected_header']
            state.update(phase='observe',at=now,stable_at=None);return
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        actual=check.snapshot(world,pc,pawn);report['last_snapshot']=actual;compare_earned(actual)
        velocities={who:actual[who]['transform']['velocity'] for who in ('player','companion')}
        if any(math.dist(v,[0.,0.,0.])>=5 for v in velocities.values()):
            report.setdefault('settling_samples',[]).append(dict(load=state['index'],elapsed=now-state['at'],velocities=velocities,
                positions={who:actual[who]['transform']['location'] for who in velocities}))
            state['stable_at']=None
            assert now-state['at']<15,'Departure never settled after restoration'
            return
        if state['stable_at'] is None:state['stable_at']=now
        if state['index']:
            check.Run.verify(SimpleNamespace(report=dict(initial=report['loads'][0]['snapshot'])),actual)
        assert not pc.is_move_input_ignored() and not pc.is_look_input_ignored(),'Input remains captured after load'
        surfaces=[s for s in unreal.ObjectIterator(unreal.SovHolographicHUDSurface) if s.get_world()==world and s.is_in_viewport()]
        assert len(surfaces)==1
        view=surfaces[0].get_holographic_hud_view();assert view.valid
        assert check.tag(view.protagonist)=='Sov.Character.Player.Tarrik'
        assert abs(view.health.current-pawn.get_health())<.01
        assert abs(surfaces[0].get_editor_property('HealthBar').get_editor_property('percent')-view.health.fraction)<.001
        if now-state['stable_at']<4:return
        if not state.get('shot'):
            report['loads'].append(dict(snapshot=actual,stable_seconds=now-state['stable_at'],hud_protagonist=check.tag(view.protagonist),health=view.health.current,
                input_released=True,visible_hud_count=len(surfaces)))
            unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/('reload-'+str(state['index'])+'.png')))
            state['shot']=now;write();return
        if now-state['shot']<2:return
        assert (out/('reload-'+str(state['index'])+'.png')).exists()
        if state['index']+1==load_count:finish();return
        state.update(index=1,phase='request',shot=None)
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick);write();level.editor_request_begin_play()
