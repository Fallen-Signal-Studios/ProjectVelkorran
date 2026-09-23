"""Read-only map census of WallRunner starts and authored wall-route entries."""
import json
import os
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'wall-route-census.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_name() == 'L_Aurelion_M12'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()


def path(value):
    return value.get_path_name() if value else None


routes = []
for route in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionWallRoute):
    points = route.get_editor_property('local_points')
    world_points = [route.get_actor_transform().transform_location(point) for point in points]
    routes.append(dict(path=path(route), id=str(route.get_editor_property('route_id')),
        origin=route.get_actor_location().export_text(), rotation=route.get_actor_rotation().export_text(),
        local_points=[p.export_text() for p in points],
        world_points=[p.export_text() for p in world_points],
        entry=world_points[0].export_text() if world_points else None,
        tolerance=route.get_editor_property('entry_tolerance'),
        speed=route.get_editor_property('speed')))

runners = []
def hit_record(raw):
    hits = [value for value in raw if isinstance(value, unreal.HitResult)] if isinstance(raw, tuple) else [raw]
    if not hits or not isinstance(hits[0], unreal.HitResult):
        return dict(error=str(raw))
    value = hits[0].to_tuple()
    return dict(blocking=bool(value[0]), overlap=bool(value[1]),
        actor=path(value[9]), impact=value[5].export_text())


for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionWallRunner):
    traversal = actor.get_wall_traversal()
    route = traversal.get_editor_property('route')
    points = route.get_editor_property('local_points') if route else []
    entry = route.get_actor_transform().transform_location(points[0]) if points else None
    capsule = actor.get_editor_property('capsule_component')
    world_points = [route.get_actor_transform().transform_location(point) for point in points] if route else []
    sweeps = []
    for index, (start, end) in enumerate(zip(world_points, world_points[1:])):
        raw = unreal.SystemLibrary.capsule_trace_single_by_profile(world, start, end,
            capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height(),
            'Pawn', False, [actor, route], unreal.DrawDebugTrace.NONE, True)
        sweeps.append(dict(segment=index, start=start.export_text(), end=end.export_text(), hit=hit_record(raw)))
    midpoint = (world_points[0]+world_points[1])*.5 if len(world_points)>1 else None
    probe = (route.get_actor_transform().transform_location(route.get_editor_property('wall_probe_direction'))
        - route.get_actor_location()) if route else None
    if probe and probe.length() > 0:
        probe = probe/probe.length()
    wall = hit_record(unreal.SystemLibrary.line_trace_single(world, midpoint,
        midpoint+probe*route.get_editor_property('wall_probe_distance'), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, [actor, route], unreal.DrawDebugTrace.NONE, True)) if midpoint and probe else None
    landing = hit_record(unreal.SystemLibrary.line_trace_single(world, world_points[-1],
        world_points[-1]-unreal.Vector(0,0,capsule.get_scaled_capsule_half_height()+30),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [actor, route],
        unreal.DrawDebugTrace.NONE, True)) if world_points else None
    runners.append(dict(path=path(actor), location=actor.get_actor_location().export_text(),
        route=path(route), entry=entry.export_text() if entry else None,
        entry_distance=(actor.get_actor_location()-entry).length() if entry else None,
        montage=path(traversal.get_editor_property('wall_run_montage')),
        sweeps=sweeps, wall=wall, landing=landing))

out.write_text(json.dumps(dict(map=world.get_path_name(), routes=routes, runners=runners), indent=2), encoding='utf-8')
