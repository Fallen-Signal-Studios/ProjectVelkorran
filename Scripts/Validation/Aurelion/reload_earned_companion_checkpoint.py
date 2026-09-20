"""Public checkpoint retry after the actual companion qualification route fails."""
import json
import os
import time
from pathlib import Path
import unreal
import observe_encounter_damage_share
import observe_companion_animation
import observe_companion_weapon_hit_path

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'EarnedCheckpointRetry'
out.mkdir(exist_ok=False)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
instance = unreal.GameplayStatics.get_game_instance(world)
owners = [s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer() == instance]
assert len(owners) == 1
saves = owners[0]
headers = [h for h in saves.list_slots() if h.kind == unreal.SovSaveSlotKind.CHECKPOINT and h.slot_index == 0]
assert len(headers) == 1
report = dict(status='loading', header=headers[0].export_text(), callbacks=[],
              method='Public load_slot of the checkpoint earned in this session; no fabricated state')
def write():
    (out / 'reload.json').write_text(json.dumps(report, indent=2))
def completed(result, header, message):
    report['callbacks'].append(dict(result=str(result), header=header.export_text(), message=str(message)))
    write()
delegate = saves.on_load_completed
delegate.add_callable(completed)
observe_encounter_damage_share.stop()
observe_companion_animation._RUN.stop('Earned checkpoint retry')
observe_companion_weapon_hit_path._RUN.stop('Earned checkpoint retry')
before_world = hash(world)
world = None
started = time.monotonic()
result, message = saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT, 0)
report.update(request_result=str(result), request_message=str(message))
write()
assert result == unreal.SovSaveResult.LOAD_STARTED, str(message)

def tick(delta):
    current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(current, 0) if current else None
    if current and hash(current) != before_world and isinstance(pawn, unreal.SovPlayerCharacterBase) and pawn.is_character_ready() and pawn.is_alive() and not pawn.is_character_pending_load():
        pc = unreal.GameplayStatics.get_player_controller(current, 0)
        state = pc.get_campaign_state()
        report.update(status='ready' if state.is_state_valid() else 'invalid_state',
                      protagonist=pawn.get_class().get_path_name(), health=pawn.get_health(),
                      journal=[str(e.beat_id) for e in state.get_journal()])
    elif time.monotonic()-started > 180:
        report['status'] = 'timeout'
    else:
        return
    delegate.remove_callable(completed)
    unreal.unregister_slate_post_tick_callback(handle)
    write()

handle = unreal.register_slate_post_tick_callback(tick)
