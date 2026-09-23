"""Passively sample the real E4B WallRunner's wall pose during checkpoint play."""
import json
import os
import time
import traceback
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
state = dict(start=time.monotonic(), next_sample=0., saw_pie=False, last_capture=0., captures=0)
report = dict(status='waiting_for_pie', samples=[], captures=[], errors=[], read_only=True)


def write():
    (out / 'wall-runner-live.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


def path(value):
    return value.get_path_name() if value else None


def tick(_delta):
    try:
        now = time.monotonic()
        if now < state['next_sample']:
            return
        state['next_sample'] = now + .2
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            if state['saw_pie']:
                report['status'] = 'completed'
                unreal.unregister_slate_post_tick_callback(state['handle'])
                write()
            return
        state['saw_pie'] = True
        rows = []
        for runner in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionWallRunner):
            if runner.get_editor_property('hidden') or not runner.is_alive():
                continue
            traversal = runner.get_wall_traversal()
            route = traversal.get_editor_property('route')
            points = route.get_editor_property('local_points') if route else []
            entry = route.get_actor_transform().transform_location(points[0]) if points else None
            mesh = runner.get_editor_property('mesh')
            anim = mesh.get_anim_instance() if mesh else None
            montage = anim.get_current_active_montage() if anim else None
            row = dict(elapsed=round(now-state['start'], 3), runner=path(runner),
                alive=runner.is_alive(), actor_location=runner.get_actor_location().export_text(),
                actor_rotation=runner.get_actor_rotation().export_text(),
                velocity=runner.get_velocity().export_text(),
                traversing=traversal.is_traversing(),
                can_begin=traversal.can_begin_traversal(),
                completed=traversal.has_completed_route(),
                last_result=str(traversal.get_last_result()),
                mesh_location=mesh.get_world_location().export_text() if mesh else None,
                mesh_relative=mesh.get_relative_transform().export_text() if mesh else None,
                montage=path(montage),
                route=path(route),
                entry_distance=(runner.get_actor_location()-entry).length() if entry else None)
            rows.append(row)
            if row['traversing'] and state['captures'] < 4 and now-state['last_capture'] > .3:
                name = f'wall-live-{state["captures"]}.png'
                unreal.SystemLibrary.execute_console_command(world,
                    'Shot showui -nosuffix filename='+str(out / name))
                report['captures'].append(dict(file=name, sample=row))
                state['captures'] += 1
                state['last_capture'] = now
        if rows:
            report['status'] = 'observing'
            report['samples'].extend(rows)
            write()
    except Exception:
        report['errors'].append(traceback.format_exc())
        report['status'] = 'observer_error'
        unreal.unregister_slate_post_tick_callback(state['handle'])
        write()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
write()
