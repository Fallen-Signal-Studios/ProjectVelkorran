"""Observe native CP9 restoration movement without weakening the acceptance check.

The original check runs unchanged. On its terminal result, retain PIE for fifteen
seconds of read-only observation, then preserve any original failure and recheck
earned state. The underlying checker restores its isolated bank from earned
save bytes. This observer changes no characters, AI, input or materials.
"""
from pathlib import Path
import json,runpy,time,traceback
import unreal
scope=runpy.run_path(str(Path(__file__).with_name('check_earned_m13_reload_roundtrip.py')),init_globals={'M13_RELOAD_COUNT':1})
scope=scope['tick'].__globals__;original_finish=scope['finish'];out=scope['out']
report=dict(status='observing',scope=__doc__,samples=[],errors=[])
state=dict(start=time.monotonic(),last=0.,last_write=0.)

def ref(value):return value.get_path_name() if value else None
def vector(value):return [value.x,value.y,value.z]
def write():(out/'departure-settle.json').write_text(json.dumps(report,indent=2))
def goal_target(goal):
    if not isinstance(goal,unreal.SovCompanionCommandGoal):return None
    try:return ref(goal.get_editor_property('target'))
    except Exception as error:
        report.setdefault('unexposed_fields',{})['command_goal_target']=str(error)
        return 'unexposed'

def finish(error=None):
    if state.get('terminal_at') is not None:return
    report['original_error']=error;report['original_result']='failed' if error else 'passed_requires_visual_review'
    state['terminal_at']=time.monotonic();scope['state']['phase']='stopping';write()
scope['finish']=finish

def end(observer_error=None):
    error=report.get('original_error')
    if observer_error:report['errors'].append(observer_error);error=error or observer_error
    try:
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        actual=scope['check'].snapshot(world,pc,pawn);scope['compare_earned'](actual)
        report['final_earned_state_matches']=True
    except Exception:
        report['final_earned_state_matches']=False;report['final_error']=traceback.format_exc();error=error or report['final_error']
    report['status']='observed_requires_review';write()
    original_finish(error);unreal.unregister_slate_post_tick_callback(handle)

def tick(dt):
    now=time.monotonic()
    if now-state['last']<.05:return
    state['last']=now
    try:
        if state.get('terminal_at') is not None and now-state['terminal_at']>=15:end();return
        assert now-state['start']<420,'Bounded restore observer expired'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world or 'M13' not in world.get_name():return
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.NarrativeNPCCharacter):
            companion=actor.get_component_by_class(unreal.SovCompanionComponent)
            if not companion or str(companion.get_editor_property('companion_id'))!='Selene':continue
            controller=actor.get_controller();activities=actor.get_activity_component();visual=actor.get_character_visual()
            goal=activities.get_current_activity_goal() if activities else None
            movement=actor.get_component_by_class(unreal.CharacterMovementComponent)
            meshes=list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
            if visual:meshes.extend(visual.get_components_by_class(unreal.SkeletalMeshComponent))
            animations=[]
            for mesh in meshes:
                anim=mesh.get_anim_instance()
                if anim:animations.append(dict(mesh=mesh.get_name(),anim=ref(anim),montage=ref(anim.get_current_active_montage()),root_motion_mode=str(anim.get_editor_property('root_motion_mode'))))
            report['samples'].append(dict(elapsed=now-state['start'],game_seconds=unreal.GameplayStatics.get_time_seconds(world),
                checker_phase=scope['state']['phase'],actor=ref(actor),position=vector(actor.get_actor_location()),velocity=vector(actor.get_velocity()),
                pending=actor.is_character_pending_load(),hidden=actor.get_editor_property('hidden'),alive=actor.is_alive(),
                movement_mode=str(movement.get_editor_property('movement_mode')) if movement else None,
                move_status=str(controller.get_move_status()) if isinstance(controller,unreal.AIController) else None,
                move_destination=vector(controller.get_immediate_move_destination()) if isinstance(controller,unreal.AIController) else None,
                activity=ref(activities.get_current_activity()) if activities else None,goal=ref(goal),
                goal_target=goal_target(goal),
                command_state=str(companion.get_command_state()),tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(actor).export_text(),animations=animations))
        if now-state['last_write']>1:write();state['last_write']=now
    except Exception:end(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick);write()
