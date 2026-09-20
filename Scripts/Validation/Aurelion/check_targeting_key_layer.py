"""Exercise saved gamepad mappings using UE's simulated key path, not action injection.

Bypasses hardware and Slate/CommonUI preprocessing. Saves no content.
"""
from pathlib import Path
import json,os,time,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
settings=unreal.GameUserSettings.get_game_user_settings();original=settings.get_settings_snapshot();settings.complete_accessibility_setup()
report=dict(status='running',qualification=__doc__,samples=[])
rows=list(unreal.load_asset('/Game/Input/IMC_Combat').get_editor_property('default_key_mappings').get_editor_property('mappings'))
wheel=next(str(r.key.get_editor_property('key_name')) for r in rows if r.action.get_name()=='IA_WeaponWheel' and str(r.key.get_editor_property('key_name')).startswith('Gamepad'))
pairs=[('Gamepad_FaceButton_Top','ThreatFocus'),('Gamepad_FaceButton_Left','Designate'),('Gamepad_DPad_Left','CycleTargetLeft'),('Gamepad_DPad_Right','CycleTargetRight')]
steps=[('wheel', [wheel],None)]
for key,name in pairs:steps.extend([(name,[wheel,key],name),('release_'+name,[wheel],None)])
steps.extend([('wheel_released',[],None),('base_Y',['Gamepad_FaceButton_Top'],'Ability3'),('base_released',[],None)])
phase='start';index=0;at=time.monotonic();started=at;busy=False;delegate=None;events=[];forced=set();seen=set();world=None;ended=None
def event(tag,pressed):events.append(dict(tag=str(unreal.GameplayTagLibrary.get_tag_name(tag)),pressed=bool(pressed),step=index))
def keys(desired):
    global forced
    # Neutralize before removing forcing: immediate removal can release ahead
    # of a pressed event already queued by the preceding forced-input tick.
    for key in forced-set(desired):unreal.SystemLibrary.execute_console_command(world,'Input.+key '+key+' 0')
    for key in set(desired)-forced:unreal.SystemLibrary.execute_console_command(world,'Input.+key '+key+' 1')
    forced=set(desired)
    seen.update(desired)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(dt):
    global phase,index,at,busy,delegate,world,ended
    if busy:return
    busy=True
    try:
        editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase=='end':
            if ended is None:
                keys([])
                for key in seen:unreal.SystemLibrary.execute_console_command(world,'Input.-key '+key)
                if delegate is not None:delegate.remove_callable(event)
                settings.apply_settings_snapshot(original)
                (out/'targeting-key-layer.json').write_text(json.dumps(report,indent=2))
                editor.editor_request_end_play();ended=time.monotonic()
            elif time.monotonic()-ended>2:
                unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert time.monotonic()-started<150,'Timed out in '+phase
        if phase=='start':editor.editor_request_begin_play();phase='ready';return
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn=unreal.GameplayStatics.get_player_pawn(world,0) if world else None
        if not pawn or not pawn.is_character_ready() or pawn.is_character_pending_load():return
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        if delegate is None:delegate=pc.on_semantic_input_changed;delegate.add_callable(event)
        name,desired,expected=steps[index]
        if phase=='ready':keys(desired);phase='settle';at=time.monotonic();return
        if phase=='settle' and time.monotonic()-at>1.5:
            observed=[e for e in events if e['step']==index]
            held=bool(pc.get_editor_property('WeaponWheelHeld'))
            sample=dict(step=name,wheel_held=held,events=observed,wielded=[w.get_class().get_path_name() for w in pawn.get_wielded_weapons()])
            report['samples'].append(sample)
            assert held==(wheel in desired),sample
            presses=[e['tag'] for e in observed if e['pressed']]
            if expected:assert 'Narrative.Input.'+expected in presses,sample
            if wheel in desired:
                forbidden=['Ability3','Interact','Reload','OpenInventory','QuickUseItems']
                assert not any('Narrative.Input.'+n in presses for n in forbidden),sample
            if index==0:report['initial_wielded']=sample['wielded']
            if name=='wheel_released':assert sample['wielded']==report['initial_wielded'],'Neutral stick chord selected a different weapon'
            index+=1
            if index==len(steps):
                report['routing_and_base_y_restore_passed']=True
                report['neutral_wheel_qualification']='No weapon was wielded; this does not qualify weapon preservation.' if not report['initial_wielded'] else 'Wielded classes unchanged with neutral stick.'
                sequences={n:[e['pressed'] for e in events if e['tag']=='Narrative.Input.'+n] for _,n in pairs}
                report['targeting_event_sequences']=sequences
                assert all(v==[True,False] for v in sequences.values()),'Targeting press/release was not exactly once: '+str(sequences)
                report['status']='passed_simulated_keys';phase='end'
            else:phase='ready'
    except Exception as exc:
        report.update(status='failed',error=str(exc),failed_step=index);phase='end'
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(tick)
