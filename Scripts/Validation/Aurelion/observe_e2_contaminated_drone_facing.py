"""Observe E2 contaminated-drone body, mesh, target and actual fire in live PIE."""
import json
import math
import os
import sys
import time
import traceback
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
state = dict(start=time.monotonic(), next_sample=0., next_write=0., handle=None,
             saw_pie=False, stopped=False, seen_shots=set(), seen_rockets=set(),
             captures=0)
report = dict(status='waiting_for_pie', gameplay_read_only=True, samples=[],
              gunshots=[], rockets=[], captures=[], errors=[])


def path(value):
    return value.get_path_name() if value else None


def optional(call):
    try:
        return call()
    except Exception as error:
        return {'unavailable': str(error)}


def bearing_error(actor, target):
    if not target:
        return None
    delta = target.get_actor_location() - actor.get_actor_location()
    if delta.length() < 1.:
        return None
    angle = math.degrees(math.atan2(delta.y, delta.x))
    return abs((angle-actor.get_actor_rotation().yaw+180.) % 360. - 180.)


def write():
    (out / 'e2-contaminated-drone-facing.json').write_text(
        json.dumps(report, indent=2), encoding='utf-8')


def stop():
    if state['stopped']:
        return
    state['stopped'] = True
    if state['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        state['handle'] = None
    if report['status'] != 'observer_error':
        report['status'] = 'completed'
    write()


def tick(_delta):
    try:
        now = time.monotonic()
        if now < state['next_sample']:
            return
        state['next_sample'] = now + .1
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            if state['saw_pie']:
                stop()
            return
        state['saw_pie'] = True
        chain = sys.modules.get('continue_aurelion_route_input')
        if chain and chain._RUN and chain._RUN.done:
            report['driver_status'] = chain._RUN.report.get('status')
            stop()
            return
        player = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not player:
            return
        drones = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(
            world, unreal.SovDroneNPCBase)
            if 'BP_AurelionContaminatedDrone_C' in actor.get_class().get_name()
            and actor.is_alive() and not actor.get_editor_property('hidden')]
        rows = []
        for drone in drones:
            mesh = drone.get_editor_property('mesh')
            anim = mesh.get_anim_instance() if mesh else None
            montage = anim.get_current_active_montage() if anim else None
            ai = drone.get_controller()
            focus = ai.get_focus_actor() if ai else None
            target = drone.get_combat_facing_target()
            activity = drone.get_activity_component()
            perception = ai.get_components_by_class(unreal.AIPerceptionComponent) if ai else []
            rows.append(dict(elapsed=round(now-state['start'], 3), actor=path(drone),
                actor_location=drone.get_actor_location().export_text(),
                actor_rotation=drone.get_actor_rotation().export_text(),
                hidden=drone.get_editor_property('hidden'),
                collision=drone.get_actor_enable_collision(),
                tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(drone).export_text(),
                mesh_relative_transform=mesh.get_relative_transform().export_text() if mesh else None,
                skeletal_mesh=path(mesh.get_skeletal_mesh_asset()) if mesh else None,
                montage=path(montage), controller=path(ai),
                activity_active=activity.is_active() if activity else None,
                activity=path(activity.get_current_activity()) if activity else None,
                goal=path(activity.get_current_activity_goal()) if activity else None,
                perception=[dict(path=path(component),
                    active=optional(lambda: component.is_active()),
                    tick_enabled=optional(lambda: component.is_component_tick_enabled()),
                    sight_enabled=optional(lambda: component.is_sense_enabled(unreal.AISense_Sight)))
                    for component in perception],
                tree=optional(lambda: path(ai.get_current_tree())) if ai else None,
                direct_target=optional(lambda: ai.can_directly_target_threat(player)) if ai else None,
                line_of_sight=optional(lambda: ai.line_of_sight_to(player)) if ai else None,
                threat_memories=optional(lambda: [memory.export_text() for memory in
                    ai.get_threat_debug_snapshot()[:4]]) if ai else None,
                focus=path(focus), facing_target=path(target),
                player_bearing_error=bearing_error(drone, player),
                focus_bearing_error=bearing_error(drone, focus),
                native_target_bearing_error=bearing_error(drone, target)))
        if rows:
            report['status'] = 'observing'
            report['samples'].extend(rows)
        for shot in unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.SovReformationDroneGunshotPresentation):
            ident = path(shot)
            if ident in state['seen_shots']:
                continue
            state['seen_shots'].add(ident)
            start = shot.get_trace_start()
            nearest = min(drones, key=lambda drone: (drone.get_actor_location()-start).length()) if drones else None
            distance = (nearest.get_actor_location()-start).length() if nearest else None
            if nearest is None or distance > 300.:
                continue
            row = dict(elapsed=round(now-state['start'], 3), shot=ident,
                nearest_drone=path(nearest), start=start.export_text(),
                end=shot.get_trace_end().export_text(),
                hit=path(shot.get_hit_actor()), damaged=bool(shot.damaged_target()),
                drone_to_start_cm=distance,
                drone_bearing_error=bearing_error(nearest, player))
            report['gunshots'].append(row)
            if state['captures'] < 1:
                filename = f'e2-contaminated-fire-{state["captures"]}.png'
                unreal.SystemLibrary.execute_console_command(world,
                    'Shot showui -nosuffix filename='+str(out / filename))
                report['captures'].append(dict(file=filename, shot=row))
                state['captures'] += 1
        for rocket in unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.SovReformationDroneRocketProjectile):
            ident = path(rocket)
            if ident in state['seen_rockets']:
                continue
            state['seen_rockets'].add(ident)
            nearest = min(drones, key=lambda drone: (
                drone.get_actor_location()-rocket.get_actor_location()).length()) if drones else None
            distance = (nearest.get_actor_location()-rocket.get_actor_location()).length() if nearest else None
            if nearest is None or distance > 300.:
                continue
            report['rockets'].append(dict(elapsed=round(now-state['start'], 3),
                rocket=ident, nearest_drone=path(nearest),
                location=rocket.get_actor_location().export_text(),
                velocity=rocket.get_velocity().export_text(),
                drone_to_rocket_cm=distance,
                drone_bearing_error=bearing_error(nearest, player)))
        if now >= state['next_write']:
            state['next_write'] = now + 1.
            write()
    except Exception:
        report['errors'].append(traceback.format_exc())
        report['status'] = 'observer_error'
        stop()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
write()
