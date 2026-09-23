"""Read-only E1 drone attack decisions during ordinary player exposure."""
import json
import os
import sys
import time
import traceback
from pathlib import Path

import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'e1-enemy-pressure.json'
state = dict(start=time.monotonic(), next_sample=0., next_packet=0., handle=None,
             packet_handle=None, seen_shots=set(), seen_rockets=set())
report = dict(status='waiting_for_e1', read_only=True, samples=[], gunshots=[], rockets=[], errors=[])


def ref(obj):
    return obj.get_path_name() if obj else None


def optional(call):
    try:
        return call()
    except Exception as error:
        return {'unavailable': str(error)}


def write():
    out.write_text(json.dumps(report, indent=2), encoding='utf-8')


def blackboard_snapshot(ai):
    board = ai.get_blackboard_component() if ai else None
    asset = board.get_blackboard_asset() if board else None
    if not asset:
        return None
    return {str(key.entry_name): ref(board.get_value_as_object(str(key.entry_name)))
            for key in asset.get_editor_property('keys')}


def packet_tick(_delta):
    try:
        now = time.monotonic()
        if now < state['next_packet']:
            return
        state['next_packet'] = now + .1
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        for shot in unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.SovReformationDroneGunshotPresentation):
            path = ref(shot)
            if path in state['seen_shots']:
                continue
            state['seen_shots'].add(path)
            report['gunshots'].append(dict(elapsed=now-state['start'], actor=path,
                start=shot.get_trace_start().export_text(), end=shot.get_trace_end().export_text(),
                hit=ref(shot.get_hit_actor()), blocking=shot.has_blocking_hit(),
                damaged=shot.damaged_target()))
        for rocket in unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.SovReformationDroneRocketProjectile):
            path = ref(rocket)
            if path in state['seen_rockets']:
                continue
            state['seen_rockets'].add(path)
            report['rockets'].append(dict(elapsed=now-state['start'], actor=path,
                location=rocket.get_actor_location().export_text(),
                velocity=rocket.get_velocity().export_text()))
    except Exception:
        report['errors'].append(traceback.format_exc())
        if state['packet_handle'] is not None:
            unreal.unregister_slate_post_tick_callback(state['packet_handle'])
            state['packet_handle'] = None
        write()


def tick(_delta):
    try:
        now = time.monotonic()
        if now < state['next_sample']:
            return
        state['next_sample'] = now + 1.
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        combat = sys.modules.get('continue_aurelion_e1_input')
        if combat and combat._RUN and combat._RUN.done:
            report['status'] = 'completed'
            report['driver_status'] = combat._RUN.report.get('status')
            unreal.unregister_slate_post_tick_callback(state['handle'])
            state['handle'] = None
            if state['packet_handle'] is not None:
                unreal.unregister_slate_post_tick_callback(state['packet_handle'])
                state['packet_handle'] = None
            write()
            return
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        player = controller.get_controlled_pawn() if controller else None
        if not isinstance(player, unreal.SovTarrikCharacter) or not player.is_character_ready():
            return
        directors = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(
            world, unreal.SovEncounterDirector) if str(actor.encounter_id) == 'M12_E1_PressureHall']
        if len(directors) != 1 or directors[0].get_encounter_state() != unreal.SovEncounterState.ACTIVE:
            return
        rows = []
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NarrativeNPCCharacter):
            if 'AurelionSecurityDrone' not in actor.get_class().get_name() or not actor.is_alive():
                continue
            if actor.get_editor_property('hidden'):
                continue
            ai = actor.get_controller()
            asc = actor.get_narrative_ability_system_component()
            mesh = next((component for component in actor.get_components_by_class(unreal.SkeletalMeshComponent)
                if component.get_name() == 'CharacterMesh0'), None)
            anim = mesh.get_anim_instance() if mesh else None
            montage = anim.get_current_active_montage() if anim else None
            candidates = optional(lambda: [item.export_text() for item in
                asc.get_bot_attack_candidates(player, unreal.GameplayTag())])
            rows.append(dict(actor=ref(actor), location=actor.get_actor_location().export_text(),
                velocity=actor.get_velocity().export_text(), distance=actor.get_distance_to(player),
                combat_facing_target=ref(actor.get_combat_facing_target()) if hasattr(actor, 'get_combat_facing_target') else None,
                combat_facing_target_location=optional(lambda: actor.get_combat_facing_target().get_actor_location().export_text())
                    if hasattr(actor, 'get_combat_facing_target') and actor.get_combat_facing_target() else None,
                controller=ref(ai), focus=ref(ai.get_focus_actor()) if ai else None,
                focal_point=optional(lambda: ai.get_focal_point().export_text()) if ai else None,
                control_rotation=optional(lambda: ai.get_control_rotation().export_text()) if ai else None,
                actor_rotation=actor.get_actor_rotation().export_text(),
                muzzle_gun=optional(lambda: mesh.get_socket_location('Muzzle_Gun').export_text()) if mesh else None,
                muzzle_rocket=optional(lambda: mesh.get_socket_location('Muzzle_Rocket_L').export_text()) if mesh else None,
                direct_target=optional(lambda: ai.can_directly_target_threat(player)) if ai else None,
                line_of_sight=optional(lambda: ai.line_of_sight_to(player)) if ai else None,
                threats=optional(lambda: [memory.export_text() for memory in ai.get_threat_debug_snapshot()]) if ai else None,
                blackboard=optional(lambda: blackboard_snapshot(ai)) if ai else None,
                tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(actor).export_text(),
                weapon=ref(actor.get_weapon()), montage=ref(montage), candidates=candidates))
        report['status'] = 'observing'
        report['samples'].append(dict(elapsed=now-state['start'],
            player_health=player.get_health(), player_shield=optional(lambda:
                player.get_component_by_class(unreal.SovShieldComponent).get_shield()),
            player_location=player.get_actor_location().export_text(), enemies=rows))
        write()
    except Exception:
        report['errors'].append(traceback.format_exc())
        report['status'] = 'observer_error'
        if state['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(state['handle'])
            state['handle'] = None
        write()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
state['packet_handle'] = unreal.register_slate_post_tick_callback(packet_tick)
write()
