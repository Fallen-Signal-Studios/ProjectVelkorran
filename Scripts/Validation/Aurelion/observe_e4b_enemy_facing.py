"""Read-only E4B attack-facing census during an earned checkpoint replay."""
import json
import os
import time
import traceback
from pathlib import Path

import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'e4b-enemy-facing.json'
state = dict(start=time.monotonic(), next_sample=0., handle=None, saw_pie=False)
report = dict(status='waiting_for_pie', read_only=True, samples=[], errors=[])


def ref(obj):
    return obj.get_path_name() if obj else None


def write():
    out.write_text(json.dumps(report, indent=2), encoding='utf-8')


def tick(_delta):
    try:
        now = time.monotonic()
        if now < state['next_sample']:
            return
        state['next_sample'] = now + .5
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            if state['saw_pie']:
                report['status'] = 'completed'
                unreal.unregister_slate_post_tick_callback(state['handle'])
                state['handle'] = None
                write()
            return
        state['saw_pie'] = True
        player = unreal.GameplayStatics.get_player_pawn(world, 0)
        rows = []
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovNPCCharacterBase):
            if not actor.is_alive() or actor.get_editor_property('hidden'):
                continue
            mesh = next((item for item in actor.get_components_by_class(unreal.SkeletalMeshComponent)
                         if item.get_name() == 'CharacterMesh0'), None)
            anim = mesh.get_anim_instance() if mesh else None
            montage = anim.get_current_active_montage() if anim else None
            target = actor.get_combat_facing_target()
            if not montage and not target:
                continue
            ai = actor.get_controller()
            rows.append(dict(actor=ref(actor), role=actor.get_class().get_name(),
                location=actor.get_actor_location().export_text(),
                actor_rotation=actor.get_actor_rotation().export_text(),
                montage=ref(montage), target=ref(target),
                target_location=target.get_actor_location().export_text() if target else None,
                focus=ref(ai.get_focus_actor()) if ai else None,
                tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(actor).export_text()))
        if rows:
            report['status'] = 'observing'
            report['samples'].append(dict(elapsed=now-state['start'],
                player=ref(player), player_location=player.get_actor_location().export_text() if player else None,
                enemies=rows))
            write()
    except Exception:
        report['errors'].append(traceback.format_exc())
        report['status'] = 'observer_error'
        if state['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(state['handle'])
            state['handle'] = None
        write()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
write()
