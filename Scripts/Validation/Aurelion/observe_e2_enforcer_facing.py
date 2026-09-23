"""Passively sample E2 Enforcer attack montages and their visible facing in PIE."""
import json
import math
import os
import sys
import time
import traceback
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'e2-enforcer-facing.json'
state = dict(start=time.monotonic(), next_sample=0., next_write=0., handle=None,
             saw_pie=False, stopped=False)
report = dict(status='waiting_for_pie', read_only=True, samples=[], errors=[])


def path(value):
    return value.get_path_name() if value else None


def write():
    out.write_text(json.dumps(report, indent=2), encoding='utf-8')


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


def bearing_error(actor, target):
    if not target:
        return None
    delta = target.get_actor_location() - actor.get_actor_location()
    if delta.length() < 1.:
        return None
    angle = math.degrees(math.atan2(delta.y, delta.x))
    yaw = actor.get_actor_rotation().yaw
    return abs((angle-yaw+180.) % 360. - 180.)


def tick(_delta):
    try:
        now = time.monotonic()
        if now < state['next_sample']:
            return
        state['next_sample'] = now + .12
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
        rows = []
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase):
            if 'BP_AurelionEnforcer_C' not in actor.get_class().get_name() or not actor.is_alive() or actor.get_editor_property('hidden'):
                continue
            mesh = actor.get_editor_property('mesh')
            anim = mesh.get_anim_instance() if mesh else None
            montage = anim.get_current_active_montage() if anim else None
            ai = actor.get_controller()
            focus = ai.get_focus_actor() if ai else None
            facing = actor.get_combat_facing_target()
            if not montage and not focus and not facing:
                continue
            try:
                permits = bool(actor.get_editor_property('permits_hard_lock'))
            except Exception:
                permits = None
            rows.append(dict(actor=path(actor), elapsed=round(now-state['start'], 3),
                montage=path(montage), focus=path(focus), facing_target=path(facing),
                permits_hard_lock=permits, actor_rotation=actor.get_actor_rotation().export_text(),
                mesh_relative_transform=mesh.get_relative_transform().export_text() if mesh else None,
                player_bearing_error=bearing_error(actor, player),
                focus_bearing_error=bearing_error(actor, focus),
                native_target_bearing_error=bearing_error(actor, facing),
                velocity=actor.get_velocity().export_text()))
        if rows:
            report['status'] = 'observing'
            report['samples'].extend(rows)
        if now >= state['next_write']:
            state['next_write'] = now + 1.
            write()
    except Exception:
        report['errors'].append(traceback.format_exc())
        report['status'] = 'observer_error'
        stop()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
write()
