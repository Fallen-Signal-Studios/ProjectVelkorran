"""Read-only E2 Enforcer weapon-fire and body-animation correlation in PIE."""
import json
import math
import os
import time
import traceback
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'e2-enforcer-weapon-fire.json'
state = dict(start=time.monotonic(), next_sample=0., next_write=0., saw_pie=False,
             stopped=False, last_attack={}, last_ammo={}, fire_frames=0,
             capture_attempts=0, captured=False, active_since=None,
             active_capture_attempted=False)
report = dict(status='waiting_for_pie', read_only=True, samples=[], attack_events=[],
              ammo_spend_events=[], player_viewport_captures=[], active_viewport_captures=[], errors=[])


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
    out.write_text(json.dumps(report, indent=2), encoding='utf-8')


def stop():
    if state['stopped']:
        return
    state['stopped'] = True
    unreal.unregister_slate_post_tick_callback(state['handle'])
    if report['status'] != 'observer_error':
        report['status'] = 'completed'
    write()


def tick(_delta):
    try:
        now = time.monotonic()
        if now < state['next_sample']:
            return
        state['next_sample'] = now + .05
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            if state['saw_pie']:
                stop()
            return
        state['saw_pie'] = True
        directors = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(
            world, unreal.SovEncounterDirector) if str(actor.encounter_id) == 'M12_E2_RelayOverlook']
        if len(directors) != 1 or directors[0].get_encounter_state() != unreal.SovEncounterState.ACTIVE:
            return
        player = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not isinstance(player, unreal.SovSeleneCharacter) or player.is_character_pending_load():
            return
        if state['active_since'] is None:
            state['active_since'] = now
        if not state['active_capture_attempted'] and now-state['active_since'] >= 5.:
            state['active_capture_attempted'] = True
            capture = unreal.SovAurelionPIEInputLibrary.capture_aurelion_pie_viewport_with_ui(
                world, str(out.parent / 'e2-active-player-with-ui.png'))
            report['active_viewport_captures'].append(capture.export_text())
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase):
            if ('BP_AurelionEnforcer_C' not in actor.get_class().get_name()
                    or not actor.is_alive() or actor.get_editor_property('hidden')
                    or not actor.get_actor_enable_collision()):
                continue
            weapon = actor.get_weapon()
            mesh = actor.get_editor_property('mesh')
            anim = mesh.get_anim_instance() if mesh else None
            ai = actor.get_controller()
            focus = ai.get_focus_actor() if ai else None
            activity = actor.get_activity_component()
            attack_time = optional(lambda: weapon.get_editor_property('last_attack_time')) if weapon else None
            ammo = optional(lambda: weapon.get_auth_ammo_in_clip()) if weapon else None
            row = dict(elapsed=round(now-state['start'], 3), actor=path(actor),
                distance_to_player_cm=(actor.get_actor_location()-player.get_actor_location()).length(),
                weapon=path(weapon), weapon_class=path(weapon.get_class()) if weapon else None,
                wielded=optional(lambda: weapon.is_wielded()) if weapon else None,
                last_attack_time=attack_time,
                ammo=ammo, weapon_visual=path(actor.get_wielded_weapon_visual()),
                montage=path(anim.get_current_active_montage()) if anim else None,
                focus=path(focus), focus_bearing_error=bearing_error(actor, focus),
                activity=path(activity.get_current_activity()) if activity else None,
                goal=path(activity.get_current_activity_goal()) if activity else None,
                tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(actor).export_text())
            report['samples'].append(row)
            if row['montage'] and 'AM_AurelionEnforcer_RifleFire' in row['montage']:
                state['fire_frames'] += 1
                if (state['fire_frames'] >= 2 and not state['captured']
                        and state['capture_attempts'] < 3):
                    state['capture_attempts'] += 1
                    filename = out.parent / ('e2-player-rifle-fire-with-ui-' + str(state['capture_attempts']) + '.png')
                    capture = unreal.SovAurelionPIEInputLibrary.capture_aurelion_pie_viewport_with_ui(
                        world, str(filename))
                    report['player_viewport_captures'].append(capture.export_text())
                    state['captured'] = bool(capture.captured)
            ident = path(actor)
            if isinstance(attack_time, (float, int)) and attack_time > 0:
                previous = state['last_attack'].get(ident)
                if previous is None or attack_time > previous + .0001:
                    report['attack_events'].append(row.copy())
                state['last_attack'][ident] = attack_time
            if isinstance(ammo, int):
                previous_ammo = state['last_ammo'].get(ident)
                if previous_ammo is not None and ammo < previous_ammo:
                    report['ammo_spend_events'].append(row.copy())
                state['last_ammo'][ident] = ammo
        if report['samples']:
            report['status'] = 'observing'
        if now >= state['next_write']:
            state['next_write'] = now + 1.
            write()
    except Exception:
        report['errors'].append(traceback.format_exc())
        report['status'] = 'observer_error'
        stop()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
write()
