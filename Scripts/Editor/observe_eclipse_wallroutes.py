"""Observe native wall-traversal state without requesting traversal or moving actors."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'eclipse-wallroutes.json'
assert not out.exists()
report=dict(status='observing',gameplay_writes=False,actors={},errors=[])
state=dict(start=time.monotonic(),last=0.,handle=None,saw_world=False)


def tick(_delta):
    now=time.monotonic()
    if now-state['last']<.25: return
    state['last']=now
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    try:
        if world:
            state['saw_world']=True
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionWallRunner):
                component=actor.get_wall_traversal()
                route=component.get_editor_property('route')
                row=report['actors'].setdefault(actor.get_path_name(),dict(route=route.get_path_name() if route else None,
                    traversing_samples=0,ever_completed=False,results=[]))
                row['traversing_samples']+=int(component.is_traversing())
                row['ever_completed']|=component.has_completed_route()
                result=str(component.get_last_result())
                if not row['results'] or row['results'][-1]['result']!=result:
                    row['results'].append(dict(elapsed=now-state['start'],result=result,position=actor.get_actor_location().export_text()))
    except Exception:
        report['errors'].append(traceback.format_exc())
    if (not world and state['saw_world']) or report['errors'] or now-state['start']>1800.:
        report['status']='failed' if report['errors'] else 'observation_complete'
        unreal.unregister_slate_post_tick_callback(state['handle'])
    out.write_text(json.dumps(report,indent=2),encoding='utf8')


state['handle']=unreal.register_slate_post_tick_callback(tick)
