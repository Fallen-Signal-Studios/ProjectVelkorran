"""Read-only terminal state after the late M13 validation pilot stopped."""
import json
import os
import sys
import traceback
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
sys.path.insert(0,str(root/'Scripts/Validation/Aurelion'))
import probe_m13_native_checkpoint_reload as cp9

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'terminal-after-hold.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_Aurelion_M13' in world.get_name()
pc = unreal.GameplayStatics.get_player_controller(world,0)
pawn = unreal.GameplayStatics.get_player_pawn(world,0)
state = pc.get_campaign_state()
events = cp9.completed_route.journal(state)
instance = unreal.GameplayStatics.get_game_instance(world)
saves = next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance)
headers = [cp9.header_data(h) for h in saves.list_slots()
           if h.kind==unreal.SovSaveSlotKind.CHECKPOINT and h.slot_index==0]
scenes = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionRequestActor)
          if str(a.beat_id)=='SeparateDepartures']
report = dict(status='read_only',world=world.get_path_name(),
              player=pawn.get_path_name() if pawn else None,
              player_location=pawn.get_actor_location().export_text() if pawn else None,
              mission_complete=state.is_mission_complete(unreal.Name('M13_ContraryWitness')),
              journal_count=len(events),journal_beats=[e['beat'] for e in events],
              cp9_headers=headers,scenes=[dict(actor=a.get_path_name(),
              phase=str(a.story.campaign_cinematic.get_phase())) for a in scenes])
try:
    report['snapshot']=cp9.snapshot(world,pc,pawn)
    report['status']='terminal_snapshot_pass'
except Exception:
    report['snapshot_error']=traceback.format_exc()
out.write_text(json.dumps(report,indent=2,default=str),encoding='utf-8')
print('Z11_PIER_TERMINAL_INSPECTION',report['status'],out)
