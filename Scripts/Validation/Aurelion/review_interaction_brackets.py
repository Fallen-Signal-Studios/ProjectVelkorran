"""Capture the shared interaction HUD on the authored E4B MovePartner console.

Public load and ordinary inputs reach the authored console. The review camera frames
the normally acquired target, then gameplay pauses for accessibility screenshots.
This is presentation coverage, not an encounter-victory or physical-input claim.
No actor transforms, health, target selection, mission proof or content assets are written.
"""
import json, os, sys, time, traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Scripts/Validation/Aurelion'))
import review_e4b_checkpoint_route as route
settings = unreal.SovGameUserSettings.get_game_user_settings()
original = settings.get_settings_snapshot()
settings.complete_accessibility_setup()
report = dict(status='running', scope=__doc__, samples=[],banks=[],callbacks=[])
state = dict(phase='observe', at=time.monotonic(), start=time.monotonic(), case=0,owned=False)
cases = ('normal', 'scale_200', 'contrast', 'custom_color', 'disabled')

def projected_bounds(world, pc, target, viewed):
    # Match the native bounds source for ordinary interactables; do not accept
    # a focus pointer alone as proof that the painted outline can be on screen.
    slots=list(viewed.get_editor_property('interaction_slots'))
    assert not slots, 'This interaction target fixture expects component bounds, not authored interaction slots'
    center, extent=target.get_actor_bounds(False)
    pixels=unreal.WidgetLayoutLibrary.get_viewport_size(world)
    dpi=unreal.WidgetLayoutLibrary.get_viewport_scale(world)
    points=[]
    for x in (-1,1):
        for y in (-1,1):
            for z in (-1,1):
                point=unreal.GameplayStatics.project_world_to_screen(pc,
                    center+unreal.Vector(x*extent.x,y*extent.y,z*extent.z),True)
                if point is None: return None
                if not (16*dpi<point.x<pixels.x-16*dpi and 16*dpi<point.y<pixels.y-16*dpi): return None
                points.append([point.x,point.y])
    return points

def write():
    (out/'interaction-brackets-review.json').write_text(json.dumps(report, indent=2))

def finish(error=None):
    report.update(status='failed' if error else 'passed_requires_visual_review', error=error)
    if not state['owned']:
        unreal.unregister_slate_post_tick_callback(route.handle)
        if route.state.get('driver') and not route.state['driver'].done: route.pilot.stop()
        if route.state.get('retry'): route.state['retry'].stop()
    if route.state.get('delegate'): route.state['delegate'].remove_callable(route.loaded)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world: unreal.GameplayStatics.set_game_paused(world, False)
    settings.apply_settings_snapshot(original)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    state.update(phase='end', at=time.monotonic())
    write()

def tick(dt):
    try:
        now = time.monotonic()
        if state['phase']=='end':
            if now-state['at']>3:
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert now-state['start']<240, 'No naturally focused interaction target captured within the bounded review'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn = unreal.GameplayStatics.get_player_pawn(world,0) if world else None
        pc = unreal.GameplayStatics.get_player_controller(world,0) if world else None
        if not isinstance(pawn,unreal.SovPlayerCharacterBase) or not pawn.is_character_ready(): return
        interaction = pc.get_component_by_class(unreal.PlayerInteractionComponent)
        viewed = interaction.get_editor_property('viewed_interactable') if interaction else None
        if state['phase']=='observe':
            driver=route.state.get('driver')
            if not driver:return
            assert not driver.done,'Gameplay ended before interaction target focus: '+str(driver.report.get('reason'))
            target=viewed.get_owner() if viewed else None
            if not isinstance(target,unreal.SovAurelionRequestActor) or str(target.request_id)!='Aurelion.E4.MovePartner':return
            if pawn.get_health()<=0:return
            target_interaction=viewed
            unreal.unregister_slate_post_tick_callback(route.handle)
            state['owned']=True
            report['captured']=dict(target=target.get_path_name(),interactable=target_interaction.get_path_name(),
                player_health=pawn.get_health(),player_position=pawn.get_actor_location().export_text())
            route.pilot.stop()
            route.report.update(status='stopped_for_presentation_review',qualification=__doc__);route.write()
            state.update(phase='frame',target=target_interaction,at=now)
        if state['phase']=='frame':
            assert now-state['at']<20,'Review camera could not frame the existing interaction target bounds'
            assert pawn.get_health()>0,'Player died before camera framing completed'
            target=state['target'].get_owner()
            center,_=target.get_actor_bounds(False)
            camera=pc.player_camera_manager.get_camera_location()
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera,center))
            if viewed!=state['target']:return
            corners=projected_bounds(world,pc,target,viewed)
            if now-state['at']<2 or not corners:return
            assert unreal.GameplayStatics.set_game_paused(world,True)
            report['captured']['projected_corners']=corners
            state['phase']='apply'
        if state['phase']=='apply':
            if state['case']==len(cases): finish(); return
            case = cases[state['case']]
            snap = settings.get_settings_snapshot()
            snap.set_editor_property('interactable_outlines',case!='disabled')
            snap.set_editor_property('high_contrast_hud',case=='contrast')
            snap.set_editor_property('ui_scale',2. if case=='scale_200' else 1.)
            snap.set_editor_property('override_team_color',case=='custom_color')
            snap.set_editor_property('team_color',unreal.LinearColor(1.,0.,1.,1.))
            settings.apply_settings_snapshot(snap)
            for p in unreal.ObjectIterator(unreal.SovAccessibilityPresentation):
                if p.get_world()==world:
                    p.present_speech(unreal.Text('Selene'),unreal.Text('Keep moving. I will cover the survivors.'),100.,pawn.get_actor_location(),False)
                    p.present_caption(unreal.Text('Incoming fire'),100.,pawn.get_actor_location())
            state.update(phase='settle',at=now)
            return
        if state['phase']=='settle' and now-state['at']>2:
            assert viewed==state['target'], 'Focused target changed during paused presentation review'
            assert pawn.get_health()>0, 'Presentation capture requires a living player'
            case=cases[state['case']]
            pixels=unreal.WidgetLayoutLibrary.get_viewport_size(world)
            report['samples'].append(dict(case=case,viewport=[pixels.x,pixels.y],
                dpi=unreal.WidgetLayoutLibrary.get_viewport_scale(world),
                target=viewed.get_path_name(),player_health=pawn.get_health()))
            unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/(case+'.png')))
            state.update(phase='capture',at=now);write();return
        if state['phase']=='capture' and now-state['at']>2:
            state['case']+=1;state['phase']='apply'
    except Exception:
        finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
write()
