"""Controlled CP9 portrait/VT-cache comparison, not campaign gameplay qualification.

Restores earned checkpoint banks through public save load. Only a PIE camera is
repositioned. No character, material, lighting, resource or campaign proof edits.
"""
import hashlib, json, os, shutil, time, traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=root/'Saved/Validation/Aurelion/FirearmCompanionRoute-20260919-223152-de264556'
assert json.loads((source/'CP9Reload/checkpoint-reload.json').read_text())['status']=='passed'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.SovGameUserSettings.get_game_user_settings().complete_accessibility_setup()
maps=[root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in maps}
report=dict(status='running',scope=globals().get('FACE_REVIEW_SCOPE',__doc__),banks=[],callbacks=[],frames=[])
state=dict(phase='bootstrap',started=time.monotonic(),busy=False)

def write(): (out/'face-streaming.json').write_text(json.dumps(report,indent=2))
def completed(result,header,message):
    report['callbacks'].append(dict(result=str(result),header=header.export_text(),message=str(message)));write()
def end(error=None):
    report.update(status='failed' if error else 'captured_requires_visual_review',error=error)
    report['maps_unchanged']=all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p)] for p in maps)
    if state.get('delegate'):state['delegate'].remove_callable(completed);state['delegate']=None
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world and state.get('buffer_view'):
        command(world,'ShowFlag.VisualizeBuffer '+str(state['original_buffer_flag']))
        command(world,'r.BufferVisualizationTarget '+state['original_buffer_target'])
    if world and 'original_sss' in state:command(world,'r.SSS.Scale '+str(state['original_sss']))
    if state.get('fuzz') and unreal.SystemLibrary.is_valid(state['fuzz']):
        state['fuzz'].set_visibility(state['fuzz_visible'])
    write();level.editor_request_end_play();state['phase']='stopping'
def command(world,text): unreal.SystemLibrary.execute_console_command(world,text)
def capture(world,name):
    face=state['face']
    report['frames'].append(dict(file=name+'.png',elapsed=time.monotonic()-state['started'],
        character=state['selene'].get_path_name(),head=face.get_socket_location('head').export_text(),
        camera=state['camera'].get_actor_transform().export_text(),
        materials=[face.get_material(i).get_path_name() if face.get_material(i) else None for i in range(face.get_num_materials())]))
    command(world,'Shot -nosuffix filename='+str(out/(name+'.png')));write()
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
        assert now-state['started']<300,'Portrait review exceeded its bounded deadline'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:return
        if state['phase']=='external_review':
            globals()['FACE_REVIEW_HOOK'](world,state,report,capture,end);return
        if state['phase']=='bootstrap':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():return
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
                report['banks'].append(dict(source=str(bank),sha256=digest))
            state.update(saves=saves,delegate=saves.on_load_completed,old_world=hash(world))
            state['delegate'].add_callable(completed)
            result,message=saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT,0)
            assert result==unreal.SovSaveResult.LOAD_STARTED,str(message)
            state['phase']='ready';write();return
        if state['phase']=='ready':
            if hash(world)==state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:return
            assert len(report['callbacks'])==1 and 'SUCCESS' in report['callbacks'][0]['result']
            assert 'Aurelion.CP9' in report['callbacks'][0]['header'] and 'L_Aurelion_M13' in world.get_name()
            characters=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.NarrativeCharacter)
                        if isinstance(a,(unreal.SovPlayerCharacterBase,unreal.SovProtagonistCompanionCharacter)) and 'Selene' in a.get_class().get_name()]
            if len(characters)!=1 or characters[0].is_character_pending_load():return
            selene=characters[0];visual=selene.get_character_visual()
            faces=[c for c in visual.get_components_by_class(unreal.SkeletalMeshComponent)
                   if c.get_editor_property('skeletal_mesh_asset') and 'FaceMesh' in c.get_editor_property('skeletal_mesh_asset').get_name()]
            assert len(faces)==1 and faces[0].does_socket_exist('head')
            cameras=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CineCameraActor)
                     if 'LS_GrammarPropagation_CameraClose' in a.get_actor_label()]
            assert len(cameras)==1
            camera=cameras[0];face=faces[0];target=face.get_socket_location('head')
            position=target+selene.get_actor_forward_vector()*140+unreal.Vector(0,0,10)
            report['original_camera']=camera.get_actor_transform().export_text()
            camera.set_actor_location(position,False,False)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position,target),False)
            camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(30)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            state['original_view_target']=pc.get_view_target()
            pc.set_view_target_with_blend(camera,0)
            state.update(phase='before',at=now,selene=selene,face=face,camera=camera)
            if globals().get('FACE_GROOM_ISOLATION',False):
                report['grooms']=[]
                fuzz=[]
                for groom in visual.get_components_by_class(unreal.GroomComponent):
                    asset=groom.get_editor_property('groom_asset')
                    report['grooms'].append(dict(component=groom.get_path_name(),asset=asset.get_path_name() if asset else None,
                        visible=groom.is_visible(),materials=[m.get_path_name() if m else None for m in groom.get_materials()]))
                    if asset and 'Peachfuzz' in asset.get_name():fuzz.append(groom)
                assert len(fuzz)==1,'Expected one actual peach-fuzz groom'
                state.update(phase='groom_before',fuzz=fuzz[0],fuzz_visible=fuzz[0].is_visible())
            if globals().get('FACE_REVIEW_HOOK'):state['phase']='external_review'
            else:command(world,'r.VT.ListPhysicalPools')
            write();return
        if state['phase']=='groom_before' and now-state['at']>15:
            capture(world,'groom-original');state.update(phase='groom_hide',at=now);return
        if state['phase']=='groom_hide' and now-state['at']>3:
            state['fuzz'].set_visibility(False);state.update(phase='groom_hidden',at=now);return
        if state['phase']=='groom_hidden' and now-state['at']>10:
            capture(world,'groom-fuzz-hidden');state.update(phase='groom_restore',at=now);return
        if state['phase']=='groom_restore' and now-state['at']>3:
            state['fuzz'].set_visibility(state['fuzz_visible']);state.update(phase='groom_restored',at=now);return
        if state['phase']=='groom_restored' and now-state['at']>10:
            capture(world,'groom-fuzz-restored');state.update(phase='groom_finish',at=now);return
        if state['phase']=='groom_finish' and now-state['at']>3:end();return
        if state['phase']=='before' and now-state['at']>15:
            capture(world,'before-flush');state.update(phase='flush',at=now);return
        if state['phase']=='flush' and now-state['at']>3 and (out/'before-flush.png').exists():
            command(world,'r.VT.Flush');report['flush_elapsed']=now-state['started']
            state.update(phase='after',at=now);write();return
        if state['phase']=='after' and now-state['at']>15:
            command(world,'r.VT.ListPhysicalPools');capture(world,'after-flush')
            state.update(phase='finish',at=now);return
        if state['phase']=='finish' and now-state['at']>3 and (out/'after-flush.png').exists():
            state.update(phase='buffer',buffer_index=0,at=now,buffer_view=True)
            state['original_buffer_flag']=unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.VisualizeBuffer')
            state['original_buffer_target']=unreal.SystemLibrary.get_console_variable_string_value('r.BufferVisualizationTarget')
            command(world,'ShowFlag.VisualizeBuffer 1')
            assert unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.VisualizeBuffer')==1
            command(world,'r.BufferVisualizationTarget BaseColor');return
        buffers=('BaseColor','Roughness','Specular','WorldNormal','Metallic','SubsurfaceColor','ShadingModel')
        if state['phase']=='buffer' and now-state['at']>5:
            capture(world,'buffer-'+buffers[state['buffer_index']])
            state.update(phase='next_buffer',at=now);return
        if state['phase']=='next_buffer' and now-state['at']>2:
            state['buffer_index']+=1
            if state['buffer_index']==len(buffers):
                command(world,'ShowFlag.VisualizeBuffer '+str(state['original_buffer_flag']))
                command(world,'r.BufferVisualizationTarget '+state['original_buffer_target'])
                state['original_sss']=unreal.SystemLibrary.get_console_variable_float_value('r.SSS.Scale')
                report['original_sss']=state['original_sss']
                command(world,'r.SSS.Scale 0');state.update(phase='sss_off',at=now);return
            command(world,'r.BufferVisualizationTarget '+buffers[state['buffer_index']])
            state.update(phase='buffer',at=now)
        if state['phase']=='sss_off' and now-state['at']>10:
            capture(world,'lit-sss-disabled');state.update(phase='sss_capture',at=now);return
        if state['phase']=='sss_capture' and now-state['at']>3:
            command(world,'r.SSS.Scale '+str(state['original_sss']))
            state.update(phase='sss_restored',at=now);return
        if state['phase']=='sss_restored' and now-state['at']>10:
            capture(world,'lit-sss-restored');state.update(phase='sss_final',at=now);return
        if state['phase']=='sss_final' and now-state['at']>3:end()
    except Exception:end(traceback.format_exc())
    finally:state['busy']=False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick);write();level.editor_request_begin_play()
