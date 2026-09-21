"""Earned E4B exit reload -> ordinary M12 aftermath, M13 travel and M13 play.

Only public save load and existing ordinary-input drivers advance gameplay.
Screenshots observe the rendered route; they require separate visual review.
No actor transforms, inventory, resources, abilities or campaign proof are patched.
"""
import hashlib,json,os,shutil,sys,time,traceback
from pathlib import Path
import unreal

project=Path(unreal.Paths.project_dir())
sys.path.insert(0,str(project/'Scripts/Validation/Aurelion'))
import continue_aurelion_m13_entry_input as aftermath
import continue_aurelion_m13_input as chamber

out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=project/'Saved/Validation/Aurelion/InteractionBracketsVisible-20260920-161801-848a4a9a'
source_path=source/'E4B/e4b-input-continuation.json'
source_bytes=source_path.read_bytes()
earned=json.loads(source_bytes)
assert earned['status']=='passed' and earned['assets_unchanged']
assert earned['native_victory']['receipt']['beat']=='ThermalFracture'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.SovGameUserSettings.get_game_user_settings().complete_accessibility_setup()
report=dict(status='running',scope=__doc__,source=str(source),source_report_sha256=hashlib.sha256(source_bytes).hexdigest(),banks=[],callbacks=[],captures=[],participant_observations=[])
state=dict(phase='bootstrap',start=time.monotonic(),busy=False,captured=set(),capture_key=None)

def write(): (out/'restored-m13-route.json').write_text(json.dumps(report,indent=2))

def inspect_participants(driver):
    if getattr(driver,'scene_beat',None)!='SeleneIndependentAssent' or not driver.scene:return
    now=time.monotonic()
    if now-state.get('last_participant_sample',0)<.1:return
    state['last_participant_sample']=now
    def read(obj,prop):
        try:
            value=obj.get_editor_property(prop)
            return value if isinstance(value,(bool,int,float,str)) else str(value)
        except Exception as error:return 'unexposed: '+str(error)
    rows=[]
    for obj in driver.scene.get_bound_objects():
        row=dict(path=obj.get_path_name(),class_name=obj.get_class().get_name())
        if isinstance(obj,unreal.NarrativeCharacter):
            asc=obj.get_component_by_class(unreal.NarrativeAbilitySystemComponent)
            visual=obj.get_character_visual()
            row.update(alive=obj.is_alive(),pending=obj.is_character_pending_load(),
                tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(obj).export_text(),
                startup_effects=read(asc,'startup_effects_applied') if asc else None,
                visual=visual.get_path_name() if visual else None,
                appearance_loaded=read(visual,'base_appearance_loaded') if visual else None)
        rows.append(row)
    report['participant_observations'].append(dict(elapsed=now-state['start'],phase=str(driver.scene_component.get_phase()),
        bound=rows,settings=driver.scene.narrative_sequence_params.export_text()))

def loaded(result,header,message):
    row=dict(load_result=str(result),header=header.export_text(),message=str(message),
        boundary=str(header.boundary_id),boundary_kind=str(header.boundary_kind),
        mission=str(header.mission_id),map=str(header.map_package),generation=header.generation)
    report['callbacks'].append(row);write()

def finish(error=None):
    report.update(status='failed' if error else 'passed_requires_visual_review',error=error)
    if state.get('delegate'):
        state['delegate'].remove_callable(loaded);state['delegate']=None
    driver=state.get('driver')
    if driver and not driver.done:
        driver.finish(False,'Review stopped; inputs released without state repair')
    write();level.editor_request_end_play();state['phase']='stopping'

def capture(world,driver):
    key=(state['phase'],driver.phase,str(getattr(driver,'scene_beat','')))
    if key!=state.get('capture_key'):
        state.update(capture_key=key,capture_at=time.monotonic())
    if driver.phase not in ('wait_scene','wait_cp6','wait_evacuation_gate','wait_departure_checkpoint'):
        return
    elapsed=time.monotonic()-state['capture_at']
    shot_key=key+('late' if elapsed>=9 else 'early',)
    if shot_key in state['captured'] or elapsed<2:return
    name='route-'+str(len(report['captures'])).zfill(2)+'.png'
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    pawn=unreal.GameplayStatics.get_player_pawn(world,0) if pc else None
    pixels=unreal.WidgetLayoutLibrary.get_viewport_size(world)
    row=dict(file=name,phase=key,world=world.get_path_name(),viewport=[pixels.x,pixels.y],
        pawn=pawn.get_path_name() if pawn else None,position=pawn.get_actor_location().export_text() if pawn else None)
    report['captures'].append(row);state['captured'].add(shot_key)
    unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/name))
    write()

def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='stopping':
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        if state['phase']=='terminal_capture':
            if now-state['at']>4:finish(state.get('error'))
            return
        assert now-state['start']<2600,'Restored route exceeded its bounded deadline'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if state['phase'] in ('aftermath','chamber'):
            driver=state['driver']
            inspect_participants(driver)
            if driver.done:
                report[state['phase']]=dict(status=driver.report['status'],reason=driver.report['reason'])
                if driver.report['status']=='passed' and state['phase']=='aftermath':
                    state.update(phase='chamber',driver=chamber.start(out/'M13'));write();return
                state['error']=driver.report['reason'] if driver.report['status']!='passed' else None
                if world:
                    unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/'terminal.png'))
                state.update(phase='terminal_capture',at=now);write();return
            if world:capture(world,driver)
            return
        pawn=unreal.GameplayStatics.get_player_pawn(world,0) if world else None
        if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready() or pawn.is_character_pending_load():return
        if state['phase']=='bootstrap':
            instance=unreal.GameplayStatics.get_game_instance(world)
            saves=next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance)
            if saves.is_load_pending():return
            banks=sorted((source/'UserData/Saved/SaveGames').glob('*_1_2_*.sav'));assert len(banks)==1
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
            result,message=saves.load_slot(unreal.SovSaveSlotKind.AUTO,2)
            assert result==unreal.SovSaveResult.LOAD_STARTED,str(message)
            write();return
        if state['phase']=='load':
            if hash(world)==state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:return
            admission=dict(report['callbacks'][-1])
            assert admission['load_result']==str(unreal.SovSaveResult.SUCCESS)
            assert admission['generation']==3 and admission['boundary_kind']==str(unreal.SovSaveBoundary.ARENA_EXIT)
            admission.update(source_report=earned,source_report_sha256=report['source_report_sha256'],banks=report['banks'])
            state['delegate'].remove_callable(loaded);state['delegate']=None
            state.update(phase='aftermath',driver=aftermath.start(out/'Aftermath',restored_victory=admission))
            write()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
write();level.editor_request_begin_play()
