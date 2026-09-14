"""Compare complete ground approach paths to a live airborne drone; no input."""
import json
import math
import os
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = pc.get_controlled_pawn()
targets = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionSecurityDrone)
           if a.is_alive() and not a.get_editor_property('hidden')]
assert len(targets) == 1
target = targets[0]
start, end = pawn.get_actor_location(), target.get_actor_location()
ignored = [pawn] + list(pawn.get_attached_actors())
if pawn.get_character_visual():
    ignored.append(pawn.get_character_visual())
rows = []
points = [('actor', end), ('ground_height', unreal.Vector(end.x, end.y, start.z))]
for center_name, center in [('target', end), ('player', start)]:
    for radius in (300., 600., 1000.):
        for index in range(8):
            angle = index*math.pi/4
            points.append((center_name+'_'+str(radius)+'_'+str(index),
                unreal.Vector(center.x+radius*math.cos(angle), center.y+radius*math.sin(angle), start.z)))
for name, point in points:
    path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world, start, point, pawn, None)
    complete = bool(path and path.is_valid() and not path.is_partial())
    row = dict(name=name, requested=point.export_text(), complete=complete,
        points=[p.export_text() for p in path.path_points] if path else [])
    if complete and path.path_points:
        floor = path.path_points[-1]
        eye = unreal.Vector(floor.x, floor.y, floor.z+160.)
        hit = unreal.SystemLibrary.line_trace_single(world, eye, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            False, ignored, unreal.DrawDebugTrace.NONE, True)
        if isinstance(hit, tuple):
            hit = next((h for h in hit if isinstance(h, unreal.HitResult)), None)
        actor = hit.to_tuple()[9] if hit else None
        row['clear'] = not hit or not hit.to_tuple()[0] or actor == target or (actor and actor.get_owner() == target)
        row['blocker'] = actor.get_path_name() if actor else None
    rows.append(row)
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'drone-approach-readonly.json'
assert not out.exists()
out.write_text(json.dumps(dict(read_only=True, player=start.export_text(), drone=end.export_text(), paths=rows), indent=2), encoding='utf-8')
