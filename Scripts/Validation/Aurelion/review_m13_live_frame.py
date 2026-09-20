"""Controlled M13 PIE camera review using ordinary viewport frames, not high-res tiles.

A public load restores unmodified, previously earned CP9 banks in an isolated
profile. No progress is fabricated and the review does not teleport the pawn.
This is rendering evidence, not a new campaign-route or physical-input test.
"""
from pathlib import Path
import hashlib,json,os,shutil,struct,time,traceback,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M12'
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
settings=unreal.SovGameUserSettings.get_game_user_settings();assert settings.complete_accessibility_setup()
views=globals().get('M13_REVIEW_VIEWS',[('default',(0,33600,-1570),0)])
assert views and len(views)<=4
state=dict(phase='bootstrap',started=time.monotonic(),at=time.monotonic(),busy=False,view_index=0)
report=dict(status='running',scope=__doc__,callbacks=[],banks=[])
report['views']=[]
def frame_path(name):
    return out/((views[state['view_index']][0]+'-' if len(views)>1 else '')+name)
def review_pose():
    _,position,pitch=views[state['view_index']]
    return unreal.Transform(location=unreal.Vector(*position),rotation=unreal.Rotator(pitch=pitch,yaw=90))
def write(): (out/'live-frame.json').write_text(json.dumps(report,indent=2))
def completed(result,header,message):
    report['callbacks'].append(dict(result=str(result),header=header.export_text(),message=str(message)));write()
def end(error=None):
    report['status']='failed' if error else 'captured'
    if error:report['error']=error
    if state.get('delegate'):state['delegate'].remove_callable(completed);state['delegate']=None
    if 'original_delay' in state:
        current=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if current:unreal.SystemLibrary.execute_console_command(current,'r.HighResScreenshotDelay '+str(state['original_delay']))
    write();level.editor_request_end_play();state.update(phase='stopping',at=time.monotonic())
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='stopping':
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert now-state['started']<240+60*(len(views)-1),'Live review timed out'
        assert level.is_in_play_in_editor() or now-state['started']<10,'PIE stopped before a playable M13 session was established; inspect the login failure in Editor.log'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:return
        if state['phase']=='bootstrap':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():return
            instance=unreal.GameplayStatics.get_game_instance(world)
            saves=next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance)
            if saves.is_load_pending():return
            source=Path(unreal.Paths.project_dir())/'Saved/Validation/Aurelion/FirearmCompanionRoute-20260919-223152-de264556'
            evidence=json.loads((source/'CP9Reload/checkpoint-reload.json').read_text())
            assert evidence['status']=='passed',evidence['status']
            banks=sorted((source/'UserData/Saved/SaveGames').glob('*_2_0_*.sav'));assert len(banks)==2
            dest=(out/'UserData/Saved/SaveGames').resolve();assert dest.is_relative_to(out.resolve());dest.mkdir(parents=True,exist_ok=True)
            for bank in banks:
                target=dest/bank.name
                if target.exists():shutil.copy2(target,out/(bank.name+'.bootstrap'))
                digest=hashlib.sha256(bank.read_bytes()).hexdigest();shutil.copy2(bank,target)
                assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
                report['banks'].append(dict(source=str(bank),sha256=digest))
            state['delegate']=saves.on_load_completed;state['delegate'].add_callable(completed);state['saves']=saves
            state['old_world']=hash(world)
            result,message=saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT,0)
            assert result==unreal.SovSaveResult.LOAD_STARTED,str(message)
            state['phase']='ready';write();return
        if state['phase']=='ready':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if hash(world)==state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:return
            assert len(report['callbacks'])==1 and 'SUCCESS' in report['callbacks'][0]['result'],report['callbacks']
            assert 'Aurelion.CP9' in report['callbacks'][0]['header']
            if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready() or pawn.is_character_pending_load():return
            assert 'L_Aurelion_M13' in world.get_name(),world.get_name()
            if globals().get('M13_EXPECT_WAYFINDING',False):
                housings=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor)
                          if a.get_actor_label().startswith('Aurelion_WayfindingPortal_')]
                assert len(housings)==3,'Saved sign housings missing from the restored game world'
                report['wayfinding']=[dict(actor=a.get_actor_label(),transform=a.get_actor_transform().export_text(),
                    mesh=a.static_mesh_component.static_mesh.get_path_name(),
                    collision=str(a.static_mesh_component.get_collision_enabled())) for a in housings]
            if globals().get('M13_EXPECT_WAYFINDING_LIGHTING',False):
                washes=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RectLight)
                        if a.get_actor_label().startswith('ENVL_M13_WayfindingWash_')]
                assert {a.get_actor_label() for a in washes}=={'ENVL_M13_WayfindingWash_Z11','ENVL_M13_WayfindingWash_Z12'}
                report['wayfinding_lighting']=[]
                for a in washes:
                    c=a.get_component_by_class(unreal.RectLightComponent)
                    assert abs(c.intensity-12)<.01 and abs(c.attenuation_radius-480)<.01
                    assert abs(c.get_editor_property('specular_scale')-.15)<.001
                    assert c.get_editor_property('cast_shadows')
                    report['wayfinding_lighting'].append(dict(actor=a.get_actor_label(),lumens=c.intensity,
                        radius_cm=c.attenuation_radius,specular_scale=c.get_editor_property('specular_scale')))
            if globals().get('M13_EXPECT_DEPARTURE_DIRECTIONS',False):
                targets={a.get_actor_label():a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
                         if a.get_actor_label() in ('Aurelion_Art_Sign_Z12_f79ad4','Z12_Dominion_Dock','Z12_Reformation_Dock')}
                assert len(targets)==3
                sign=targets['Aurelion_Art_Sign_Z12_f79ad4']
                label=sign.get_component_by_class(unreal.TextRenderComponent).text
                assert str(label)=='DEPARTURE CONCOURSE\n< REFORMATION   DOMINION >'
                assert unreal.TextLibrary.text_is_from_string_table(label)
                right=unreal.MathLibrary.get_right_vector(unreal.Rotator(yaw=90))
                offsets={}
                for faction,side in [('Reformation',-1),('Dominion',1)]:
                    delta=targets['Z12_'+faction+'_Dock'].get_actor_location()-sign.get_actor_location()
                    projection=delta.x*right.x+delta.y*right.y+delta.z*right.z
                    assert projection*side>1000
                    offsets[faction]=projection
                report['departure_sign']=dict(text=str(label),table_key=str(unreal.TextLibrary.string_table_id_and_key_from_text(label)),
                    dock_view_right_cm=offsets)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pose=review_pose()
            # Reposition only the PIE copy of a finished scene's existing camera.
            cameras=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CineCameraActor)
                     if 'LS_GrammarPropagation_CameraClose' in a.get_actor_label()]
            assert len(cameras)==1,'Require the known GrammarPropagation camera in the restored world'
            camera=cameras[0]
            report['review_camera_original']=camera.get_actor_transform().export_text()
            camera.set_actor_transform(pose,False,False)
            camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(80)
            pc.set_view_target_with_blend(camera,0)
            state.update(phase='settle',at=now,camera=camera)
            report.update(world=world.get_name(),pawn=pawn.get_class().get_name(),pawn_position=pawn.get_actor_location().export_text())
            write();return
        if state['phase']=='settle' and now-state['at']>12:
            manager=unreal.GameplayStatics.get_player_camera_manager(world,0)
            actual=manager.get_camera_location()
            assert (actual-unreal.Vector(*views[state['view_index']][1])).length()<1,actual
            report['camera']=actual.export_text()
            report['fov']=manager.get_fov_angle()
            report['cvars']={n:unreal.SystemLibrary.get_console_variable_float_value(n) for n in ('r.AntiAliasingMethod','r.ScreenPercentage','r.ShadowQuality','r.Nanite','r.HighResScreenshotDelay')}
            unreal.SystemLibrary.execute_console_command(world,'Shot -nosuffix filename='+str(frame_path('ordinary-game-frame.png')))
            state.update(phase='capture',at=now);return
        if state['phase']=='capture' and frame_path('ordinary-game-frame.png').exists():
            width,height=struct.unpack('>II',frame_path('ordinary-game-frame.png').read_bytes()[16:24])
            report['frame_dimensions']=[width,height]
            state['original_delay']=unreal.SystemLibrary.get_console_variable_int_value('r.HighResScreenshotDelay')
            unreal.SystemLibrary.execute_console_command(world,'r.HighResScreenshotDelay 64')
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(width,height,str(frame_path('same-pie-highres.png')))
            state.update(phase='highres',at=now);return
        if state['phase']=='highres' and state['task'].is_task_done():
            assert frame_path('same-pie-highres.png').exists()
            unreal.SystemLibrary.execute_console_command(world,'r.HighResScreenshotDelay '+str(state['original_delay']))
            state.update(phase='resettle',at=now);return
        if state['phase']=='resettle' and now-state['at']>12:
            unreal.SystemLibrary.execute_console_command(world,'Shot -nosuffix filename='+str(frame_path('ordinary-after-highres.png')))
            state.update(phase='recapture',at=now);return
        if state['phase']=='recapture' and frame_path('ordinary-after-highres.png').exists():
            report['views'].append(dict(name=views[state['view_index']][0],camera=report['camera'],
                dimensions=report['frame_dimensions'],frame=str(frame_path('ordinary-game-frame.png'))))
            if state['view_index']+1<len(views):
                state['view_index']+=1
                state['camera'].set_actor_transform(review_pose(),False,False)
                state.update(phase='settle',at=now);write();return
            report['status']='ready_for_window_review';write();state.update(phase='review',at=now)
        if state['phase']=='review' and ((out/'release-review.txt').exists() or now-state['at']>30):end()
    except Exception:end(traceback.format_exc())
    finally:state['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick);write();level.editor_request_begin_play()
