"""Hold an earned CP9 portrait for operator-selected same-process color isolation.

Commands in this run's color-command.json: reload, isolate, ui, scene and stop.
The opt-in material probe also supports flush, rebind, dynamic, simple, restore, close and wide.
No gameplay-state or saved-asset edits. Transient material assignments and render
overrides are restored before cleanup or travel.
"""
import json,runpy,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_departure_keys.py'))
scope=runpy.run_path(str(Path(__file__).with_name('check_earned_m13_reload_roundtrip.py')),init_globals={'M13_RELOAD_COUNT':1})
scope=scope['tick'].__globals__;original_finish=scope['finish']
out=scope['out'];report=dict(status='loading',scope=__doc__,portraits=[],isolations=[])
state=dict(phase='initial',index=0,start=time.monotonic())
fixed_camera=globals().get('M13_COLOR_FIXED_CAMERA')
material_probe=bool(globals().get('M13_COLOR_MATERIAL_PROBE',False))
steps=(('BaseColor',{'ShowFlag.VisualizeBuffer':1,'r.BufferVisualizationTarget':'BaseColor'}),
    ('lit-restored',{}),('sss-off',{'r.SubsurfaceScattering':0}),('sss-restored',{}),
    ('megalights-off',{'r.MegaLights.Allowed':0}),('megalights-restored',{}),
    ('profile-cache-off',{'r.SSS.Burley.EnableProfileIdCache':0}),('profile-cache-restored',{}),
    ('checkerboard-off',{'r.SSS.Checkerboard':0}),('final-restored',{}))
variables=sorted({k for _,v in steps for k in v})
def write(): (out/'selene-live-color.json').write_text(json.dumps(report,indent=2))
def set_values(values):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:return
    for key,value in values.items():
        argument='""' if value=='' else str(value)
        unreal.SystemLibrary.execute_console_command(world,key+' '+argument)
        actual=(unreal.SystemLibrary.get_console_variable_string_value(key) if isinstance(value,str)
            else unreal.SystemLibrary.get_console_variable_int_value(key))
        assert actual==value,(key,actual,value)
def release_camera():
    restore_materials()
    if state.get('pc') and unreal.SystemLibrary.is_valid(state['pc']):state['pc'].set_view_target_with_blend(state['view'],0)
    if state.get('camera') and unreal.SystemLibrary.is_valid(state['camera']):state['camera'].destroy_actor()
    for key in ('pc','view','camera','selene','face'):state.pop(key,None)
def frame_face():
    if fixed_camera and not state.get('close'):
        state['camera'].set_actor_location(unreal.Vector(*fixed_camera['position']),False,False)
        state['camera'].set_actor_rotation(unreal.Rotator(yaw=fixed_camera['yaw']),False)
        state['camera'].get_component_by_class(unreal.CameraComponent).set_field_of_view(fixed_camera['fov'])
        return
    target=state['face'].get_socket_location('head')
    camera=state['camera']
    camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(30.)
    camera.set_actor_location(target+state['selene'].get_actor_forward_vector()*140+unreal.Vector(0,0,10),False,False)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),target),False)

def restore_materials():
    originals=state.pop('material_originals',None)
    if originals and state.get('face') and unreal.SystemLibrary.is_valid(state['face']):
        for index,material in originals.items():state['face'].set_material(index,material)
        assert all(state['face'].get_material(i)==m for i,m in originals.items())
        report['material_assignments_restored']=True

def material_action(action):
    assert material_probe
    face=state['face']
    if action=='flush':
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        unreal.SystemLibrary.execute_console_command(world,'r.VT.ListPhysicalPools')
        unreal.SystemLibrary.execute_console_command(world,'r.VT.Flush')
    elif action in ('close','wide'):
        assert action=='close' or fixed_camera
        state['close']=action=='close'
    elif action=='restore':restore_materials()
    else:
        if 'material_originals' not in state:
            state['material_originals']={i:m for i,m in enumerate(face.get_materials()) if m and 'Face_Skin' in m.get_name()}
        originals=state['material_originals'];assert originals and 0 in originals
        report['material_assignments_restored']=False
        for index,material in originals.items():
            if action=='rebind':
                face.set_material(index,None);face.set_material(index,material)
            elif action=='simple':
                simple=unreal.load_asset('/Game/MetaHumans/MHC_Selene/Face/Materials/M_SeleneFace')
                assert isinstance(simple,unreal.Material), 'Simple face sampler material missing'
                face.set_material(index,simple)
            else:
                assert action=='dynamic'
                assert face.create_dynamic_material_instance(index,source_material=material)
    report.setdefault('material_actions',[]).append(dict(action=action,elapsed=time.monotonic()-state['start']))
def end(error=None):
    try:
        if state.get('originals'):set_values(state['originals']);report['settings_restored']=True
        release_camera()
    except Exception:error=(error or '')+'\n'+traceback.format_exc()
    report.update(status='failed' if error else 'captured_requires_visual_review',error=error);write()
    state['phase']='done';original_finish(error);unreal.unregister_slate_post_tick_callback(handle)
def arm():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready() or pawn.is_character_pending_load():return False
    actual=scope['check'].snapshot(world,pc,pawn);scope['compare_earned'](actual)
    selene=scope['check'].live_companion(world,pc,pawn)
    if selene.is_character_pending_load():return False
    visual=selene.get_character_visual()
    face=next(c for c in visual.get_components_by_class(unreal.SkeletalMeshComponent)
        if c.get_editor_property('skeletal_mesh_asset') and 'FaceMesh' in c.get_editor_property('skeletal_mesh_asset').get_name())
    target=face.get_socket_location('head')
    camera=helpers['summon'](world,'CameraActor')
    camera.set_actor_location(target+selene.get_actor_forward_vector()*140+unreal.Vector(0,0,10),False,False)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),target),False)
    camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(30.)
    state.update(pc=pc,view=pc.get_view_target(),selene=selene,face=face,camera=camera,phase='settle',at=time.monotonic())
    pc.set_view_target_with_blend(camera,0)
    return True
def finish(error=None):
    if error:end(error);return
    scope['state']['phase']='stopping';state['phase']='arm'
scope['finish']=finish
def materials():
    rows=[]
    for material in state['face'].get_materials():
        row=dict(path=material.get_path_name() if material else None,layers=[]);current=material
        for _ in range(8):
            if not isinstance(current,unreal.MaterialInstance):break
            layer=dict(path=current.get_path_name())
            for key in ('scalar_parameter_values','vector_parameter_values','texture_parameter_values','subsurface_profile','override_subsurface_profile'):
                try:
                    value=current.get_editor_property(key)
                    layer[key]=[v.export_text() for v in value] if key.endswith('_values') else str(value)
                except Exception as error:layer[key]='unexposed: '+str(error)
            row['layers'].append(layer);current=current.get_editor_property('parent')
        rows.append(row)
    return rows
def shot(name):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    unreal.SystemLibrary.execute_console_command(world,'Shot '+('showui ' if state.get('show_ui') else '')+'-nosuffix filename='+str(out/(name+'.png')))
    lod={}
    for key in ('forced_lod_model','predicted_lod_level','min_lod_model'):
        try:lod[key]=str(state['face'].get_editor_property(key))
        except Exception:lod[key]='unexposed'
    lod['sync']=[c.get_lod_sync_debug_text() for c in state['selene'].get_character_visual().get_components_by_class(unreal.LODSyncComponent)]
    return dict(file=name+'.png',elapsed=time.monotonic()-state['start'],materials=materials(),lod=lod,show_ui=bool(state.get('show_ui')),
        fov=state['camera'].get_component_by_class(unreal.CameraComponent).field_of_view,
        character=state['selene'].get_path_name(),camera=state['camera'].get_actor_transform().export_text())
def tick(dt):
    if state['phase']=='initial':return
    try:
        now=time.monotonic();assert now-state['start']<1200,'Bounded portrait probe expired'
        if state['phase']=='arm':arm();return
        if state['phase']=='settle' and now-state['at']>12:
            # The appearance-ready flag can precede the restored animation pose.
            # Frame the actual settled head, then allow the camera manager to update.
            frame_face();state.update(phase='framed',at=now);return
        if state['phase']=='framed' and now-state['at']>2:
            report['portraits'].append(shot('portrait-'+str(state['index'])))
            report.update(status='awaiting_command',command_file=str(out/'color-command.json'))
            state.update(phase='wait',at=now);write();return
        if state['phase']=='wait':
            assert now-state['at']<180,'No diagnostic command within three minutes'
            command=out/'color-command.json'
            if now-state['at']<3 or not command.exists():return
            action=json.loads(command.read_text())['action'];command.unlink()
            assert action in ('reload','isolate','ui','scene','stop') or (material_probe and action in ('flush','rebind','dynamic','simple','restore','close','wide'))
            if action=='stop':end();return
            if action in ('flush','rebind','dynamic','simple','restore','close','wide'):
                assert state['index']<15,'Maximum sixteen material-probe portraits reached'
                material_action(action);state['index']+=1
                state.update(phase='settle',at=now);report['status']='material_probe';write();return
            if action in ('ui','scene'):
                assert state['index']<5
                state['show_ui']=action=='ui';state['index']+=1
                state.update(phase='settle',at=now);report['status']='capture_mode_change';write();return
            if action=='reload':
                assert state['index']<5,'Maximum six portrait attempts reached'
                release_camera();state['index']+=1
                state['callback_count']=len(scope['report']['callbacks'])
                result,message=scope['state']['saves'].load_slot(unreal.SovSaveSlotKind.CHECKPOINT,0)
                assert result==unreal.SovSaveResult.LOAD_STARTED,str(message)
                state['phase']='reload';report['status']='reloading';write();return
            state['originals']={key:(unreal.SystemLibrary.get_console_variable_string_value(key) if key=='r.BufferVisualizationTarget'
                else unreal.SystemLibrary.get_console_variable_int_value(key)) for key in variables}
            report['render_originals']=dict(state['originals']);state.update(phase='isolate',step=0,shot_at=None,at=now)
            set_values(steps[0][1]);report['status']='isolating';write();return
        if state['phase']=='reload':
            if scope['state']['saves'].is_load_pending() or len(scope['report']['callbacks'])<=state['callback_count']:return
            assert scope['report']['callbacks'][-1]['success']
            state['phase']='arm';return
        if state['phase']=='isolate':
            if now-state['at']<8:return
            if state['shot_at'] is None:
                name,values=steps[state['step']]
                row=shot('isolate-'+name);row['overrides']=values;report['isolations'].append(row)
                state['shot_at']=now;write();return
            if now-state['shot_at']<2:return
            assert (out/report['isolations'][-1]['file']).exists()
            set_values(state['originals']);state['step']+=1
            if state['step']==len(steps):
                if material_probe:
                    report.update(status='awaiting_command',settings_restored=True)
                    state.update(phase='wait',at=now);write()
                else:end()
                return
            set_values(steps[state['step']][1]);state.update(at=now,shot_at=None)
    except Exception:end(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick);write()
