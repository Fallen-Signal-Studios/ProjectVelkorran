"""Actual wheel input and rendered themes, then public restoration of earned Selene checkpoint 2.

No inventory grants, teleports or writes to widget selection state. Screenshots require visual review.
"""
import hashlib,json,os,shutil,sys,time,traceback
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Scripts/Validation/Aurelion'))
from aurelion_wheel_input import Selector,WHEEL_CLASS
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=Path(unreal.Paths.project_dir())/'Saved/Validation/Aurelion/SeleneShoulderHandoff-20260920-043213-8124ab93'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
settings=unreal.SovGameUserSettings.get_game_user_settings();original=settings.get_settings_snapshot()
settings.complete_accessibility_setup()
context=unreal.load_asset('/Game/Input/IMC_Combat')
rows=context.get_editor_property('default_key_mappings').get_editor_property('mappings')
wheel_actions={r.action for r in rows if r.action.get_name()=='IA_WeaponWheel'}
assert len(wheel_actions)==1
wheel_action=next(iter(wheel_actions))
wheel_class=unreal.load_class(None,WHEEL_CLASS)
report=dict(status='running',scope=__doc__,samples=[],selections=[],banks=[],callbacks=[])
state=dict(phase='ready',hero='Tarrik',case=0,at=time.monotonic(),start=time.monotonic(),busy=False)
cases=('waypoint','wheel','wheel_contrast','wheel_return','wheel_scale_150','wheel_scale_200')

def write(): (out/'weapon-wheel-review.json').write_text(json.dumps(report,indent=2))
def loaded(result,header,message):report['callbacks'].append(dict(result=str(result),message=str(message)))
def configure(world,pawn,case):
    snap=settings.get_settings_snapshot();snap.set_editor_property('high_contrast_hud',case=='wheel_contrast')
    snap.set_editor_property('navigation_contrast',False)
    snap.set_editor_property('ui_scale',2. if case=='wheel_scale_200' else 1.5 if case=='wheel_scale_150' else 1.)
    settings.apply_settings_snapshot(snap)
    if case=='waypoint':
        for p in unreal.ObjectIterator(unreal.SovAccessibilityPresentation):
            if p.get_world()==world:
                p.present_speech(unreal.Text('Lyessa'),unreal.Text('Hold the relay. Keep the east stair clear while the survivors cross the terrace.'),100.,pawn.get_actor_location(),True)

def bounds(widget):
    rect=unreal.SovWidgetTreeAuthoringLibrary.get_widget_paint_bounds(widget)
    return [rect.x,rect.y,rect.z,rect.w]

def clearance(world,wheel):
    presentation=next(p for p in unreal.ObjectIterator(unreal.SovAccessibilityPresentation)
        if p.get_world()==world and p.get_owning_player()==wheel.get_owning_player())
    rect=bounds(wheel.get_editor_property('WheelSafeFrame'))
    if rect[2]-rect[0]<1:
        widgets=[dict(name=item.get_name(),type=item.get_class().get_name(),bounds=bounds(item),visibility=str(item.get_visibility()),
            parent=item.get_parent().get_name() if item.get_parent() else None)
            for item in unreal.ObjectIterator(unreal.Widget) if item.get_path_name().startswith(wheel.get_path_name()+'.')]
        (out/'wheel-geometry-failure.json').write_text(json.dumps(widgets,indent=2))
    prefix=presentation.get_path_name()+'.'
    canvases=[widget for widget in unreal.ObjectIterator(unreal.CanvasPanel) if widget.get_path_name().startswith(prefix)]
    assert len(canvases)==1
    safe=bounds(canvases[0])
    assert rect[2]-rect[0]>100 and rect[3]-rect[1]>100,rect
    assert rect[0]>=safe[0] and rect[1]>=safe[1] and rect[2]<=safe[2] and rect[3]<=safe[3],(rect,safe)
    panels={}
    dialogue=False
    for widget in unreal.ObjectIterator(unreal.Border):
        if not widget.get_path_name().startswith(prefix):continue
        name=widget.get_name()
        if widget and widget.is_visible():
            panel=bounds(widget);panels[name]=panel
            assert not (rect[0]<panel[2] and rect[2]>panel[0] and rect[1]<panel[3] and rect[3]>panel[1]),(name,rect,panel)
            child=widget.get_content()
            if isinstance(child,unreal.TextBlock) and 'Lyessa' in str(child.get_text()):dialogue=True
    assert dialogue,'The clearance fixture must include live dialogue'
    return dict(wheel=rect,safe=safe,panels=panels)
def owner(world,pc):
    engine=unreal.GameplayStatics.get_game_instance(world).get_outer()
    candidates=[s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)
        if isinstance(s.get_outer(),unreal.LocalPlayer) and s.get_outer().get_outer()==engine]
    assert len(candidates)==1
    return candidates[0]
def stop(error=None):
    report['status']='failed' if error else 'passed_requires_visual_review'
    if error:report['error']=error
    settings.apply_settings_snapshot(original)
    if state.get('delegate'):state['delegate'].remove_callable(loaded)
    if state.get('input'):state['input'].inject_input_vector_for_action(wheel_action,unreal.Vector(),[],[])
    write();level.editor_request_end_play();state['phase']='stopping'
def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='stopping':
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        if state['phase']=='failure_capture':
            if now-state['at']>3:stop(state['error'])
            return
        assert now-state['start']<260,'Wheel review timed out'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn=unreal.GameplayStatics.get_player_pawn(world,0) if world else None
        if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready() or pawn.is_character_pending_load():return
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        state['input']=owner(world,pc)
        if state['phase']=='ready':
            configure(world,pawn,cases[0]);state.update(phase='settle',at=now)
        if state['phase']=='load':
            if hash(world)==state['old_world'] or state['saves'].is_load_pending() or not report['callbacks']:return
            assert 'SUCCESS' in report['callbacks'][-1]['result']
            assert pawn.get_class().get_name()=='BP_SovSelene_C'
            state.update(hero='Selene',case=0,phase='ready');return
        if state['phase']=='select':
            held,result=state['selector'].step(world)
            state['input'].inject_input_vector_for_action(wheel_action,unreal.Vector(float(held),0,0),[],[])
            if state['selector'].done:
                report['selections'].append(result);write()
                assert result['status']=='passed',result.get('reason','Selection failed')
                if state['hero']=='Selene':stop();return
                banks=sorted((source/'UserData/Saved/SaveGames').glob('*_2_0_*.sav'));assert len(banks)==2
                destination=out/'UserData/Saved/SaveGames';destination.mkdir(parents=True,exist_ok=True)
                for bank in banks:
                    target=destination/bank.name
                    if target.exists():shutil.copy2(target,out/(bank.name+'.before'))
                    shutil.copy2(bank,target);digest=hashlib.sha256(bank.read_bytes()).hexdigest()
                    assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
                    report['banks'].append(dict(name=bank.name,sha256=digest))
                instance=unreal.GameplayStatics.get_game_instance(world)
                saves=next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance)
                state.update(saves=saves,delegate=saves.on_load_completed,old_world=hash(world),phase='load')
                state['delegate'].add_callable(loaded)
                result,message=saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT,0)
                assert result==unreal.SovSaveResult.LOAD_STARTED,str(message)
            return
        if state['phase']=='release':
            state['input'].inject_input_vector_for_action(wheel_action,unreal.Vector(),[],[])
            if now-state['at']>2:
                assert not bool(pc.get_editor_property('WeaponWheelHeld'))
                assert not any(w.is_activated() for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,wheel_class,False))
                wanted='Velkorran' if state['hero']=='Tarrik' else 'Staccato'
                wielded=[w.get_class().get_name() for w in pawn.get_wielded_weapons()]
                if 'WI_'+wanted+'_C' in wielded:wanted='Cinderline' if state['hero']=='Tarrik' else 'Verity'
                weapon_class='/Game/Items/Weapons/WI_'+wanted+'.WI_'+wanted+'_C'
                hand=unreal.get_default_object(unreal.load_class(None,weapon_class)).get_editor_property('weapon_hand')
                # Two-handed weapons use the wheel's ordinary release-to-confirm path.
                # The main-hand slot is only populated by the one-handed choice path.
                manual=hand!=unreal.WeaponHandRule.WHR_BOTH
                state['selector']=Selector(weapon_class,manual_mainhand=manual)
                report.setdefault('hand_rules',[]).append(dict(weapon=weapon_class,rule=str(hand),manual_mainhand=manual))
                state['phase']='select'
            return
        case=cases[state['case']]
        held=case.startswith('wheel')
        state['input'].inject_input_vector_for_action(wheel_action,unreal.Vector(float(held),0,0),[],[])
        if held:
            open_wheels=[w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,wheel_class,False) if w.is_activated() and w.get_owning_player()==pc]
            target_angle=240. if state['hero']=='Tarrik' else 90.
            if open_wheels and abs(float(open_wheels[0].get_editor_property('CurrentAngle'))-target_angle)>2.:
                dx,dy=(-60.,-103.923) if state['hero']=='Tarrik' else (0.,120.)
                unreal.SovAurelionPIEInputLibrary.inject_aurelion_pie_mouse_delta(world,dx,dy)
        if state['phase']=='settle' and now-state['at']>5:
            row=dict(hero=state['hero'],case=case,pawn=pawn.get_class().get_name(),wheel_held=bool(pc.get_editor_property('WeaponWheelHeld')))
            assert row['wheel_held']==held
            if held:
                wheels=[w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,wheel_class,False) if w.is_activated() and w.get_owning_player()==pc]
                assert len(wheels)==1
                w=wheels[0];image=w.get_editor_property('Image_WeaponWheel')
                row['clearance']=clearance(world,w)
                row['backgrounds']=[dict(name=b.get_name(),visibility=str(b.get_visibility()),opacity=b.get_render_opacity(),color=b.get_editor_property('brush_color').export_text())
                    for b in unreal.ObjectIterator(unreal.CommonBorder) if b.get_path_name().startswith(w.get_path_name()+'.')]
                row['camera_rotation']=pc.player_camera_manager.get_camera_rotation().export_text()
                mat=image.get_editor_property('brush').get_editor_property('resource_object')
                assert isinstance(mat,unreal.MaterialInstanceDynamic)
                assert mat.get_editor_property('parent').get_path_name()=='/Game/Aurelion/UI/HUD/M_SovWeaponWheelGlass.M_SovWeaponWheelGlass',mat.get_path_name()
                accent=mat.get_vector_parameter_value('ProtagonistAccent')
                row.update(material=mat.get_path_name(),accent=[accent.r,accent.g,accent.b],contrast=mat.get_scalar_parameter_value('HighContrast'),dominion=mat.get_scalar_parameter_value('DominionFrame'),
                    count=int(w.get_editor_property('SectorCount')),selected=int(w.get_editor_property('SelectedSector')),
                    angle=float(w.get_editor_property('CurrentAngle')),active_angle=mat.get_scalar_parameter_value('ActiveAngle'),
                    bounds=list(w.get_editor_property('SectorListMinBounds')))
                assert row['contrast']==float(case=='wheel_contrast'),row
                assert row['dominion']==float(state['hero']=='Tarrik'),row
                if case!='wheel_contrast':assert (accent.r>accent.b)==(state['hero']=='Tarrik'),row
                else:assert min(row['accent'])>.99,row
                glyphs=[]
                for item in w.get_editor_property('Overlay_RadialItems').get_all_children():
                    icon=item.get_editor_property('CommonLazyImage_RadialIcon')
                    glyph=icon.get_editor_property('brush').get_editor_property('resource_object')
                    assert isinstance(glyph,unreal.MaterialInstanceDynamic),str(glyph)
                    assert glyph.get_editor_property('parent').get_path_name()=='/Game/Aurelion/UI/HUD/M_SovWeaponWheelGlyph.M_SovWeaponWheelGlyph'
                    texture=glyph.get_texture_parameter_value('WeaponTexture');assert texture
                    actual_item=item.call_method('GetItem')
                    if actual_item:
                        expected=actual_item.get_editor_property('thumbnail')
                        expected_path=expected.get_path_name() if isinstance(expected,unreal.Object) else expected.to_string()
                        assert texture.get_path_name()==expected_path,(texture.get_path_name(),expected_path)
                    glyphs.append(texture.get_path_name())
                row['glyph_textures']=glyphs
            report['samples'].append(row);write()
            unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/(state['hero']+'-'+case+'.png')))
            state.update(phase='capture',at=now)
        elif state['phase']=='capture' and now-state['at']>3:
            state['case']+=1
            if state['case']==len(cases):
                # Close before opening a fresh wheel through the ordinary selector.
                state['input'].inject_input_vector_for_action(wheel_action,unreal.Vector(),[],[])
                state.update(phase='release',at=now)
            else:
                configure(world,pawn,cases[state['case']]);state.update(phase='settle',at=now)
    except Exception:
        error=traceback.format_exc()
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if world:
            unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/'failure.png'))
            state.update(phase='failure_capture',at=time.monotonic(),error=error)
        else:stop(error)
    finally:state['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
write();level.editor_request_begin_play()
