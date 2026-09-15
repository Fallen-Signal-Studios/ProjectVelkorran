"""Read-only collision ownership survey for the finalized Z08 cargo visuals.

Broad-phase overlap is evidence for review, never permission to delete a collider.
The report deliberately does not approve campaign destruction placements.
"""
from pathlib import Path
import json
import os
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
runpy.run_path(str(root / 'Scripts/Editor/check_z08_cargo.py'))['check_z08_cargo'](actors)
helpers = runpy.run_path(str(root / 'Scripts/Editor/check_z08_railings.py'))
baseline = json.loads((root / 'Art/Source/Aurelion/Z08CargoKit/cargo-baseline.json').read_text())
owner = next(a for a in actors if a.get_actor_label() == baseline['actor'])
visual = owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
mesh_bounds = visual.static_mesh.get_bounds()


def xyz(v):
    return [v.x, v.y, v.z]


def intersect(a, b):
    # Exclude mere touching (floor, ceiling, adjacent cargo) from volume ownership.
    return all(min(a[1][i], b[1][i]) - max(a[0][i], b[0][i]) > .1 for i in range(3))


colliders = []
for actor in actors:
    if not actor.get_actor_enable_collision():
        continue
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        if component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
            continue
        origin, extent, _ = unreal.SystemLibrary.get_component_bounds(component)
        bounds = [[v - e for v, e in zip(xyz(origin), xyz(extent))],
                  [v + e for v, e in zip(xyz(origin), xyz(extent))]]
        colliders.append((actor, component, bounds))

rows = []
for index in range(visual.get_instance_count()):
    transform = visual.get_instance_transform(index, world_space=True)
    bounds = helpers['bounds'](helpers['corners'](transform, xyz(mesh_bounds.origin), xyz(mesh_bounds.box_extent)))
    center = [(a + b) / 2 for a, b in zip(*bounds)]
    overlaps = []
    for actor, component, obstruction_bounds in colliders:
        if not intersect(bounds, obstruction_bounds):
            continue
        probes = []
        for axis in (0, 1):
            start, end = center.copy(), center.copy()
            start[axis], end[axis] = bounds[0][axis] - 10, bounds[1][axis] + 10
            hit = component.line_trace_component(unreal.Vector(*start), unreal.Vector(*end), False, False, False)
            probes.append(dict(axis='xy'[axis], start_cm=start, end_cm=end,
                               contact_cm=xyz(hit[0]) if hit else None))
        overlaps.append(dict(actor=actor.get_actor_label(), actor_path=actor.get_path_name(),
                             class_name=actor.get_class().get_name(), component=component.get_path_name(),
                             transform=component.get_world_transform().export_text(), bounds_cm=obstruction_bounds,
                             collision=str(component.get_collision_enabled()), profile=str(component.get_collision_profile_name()),
                             pawn_response=str(component.get_collision_response_to_channel(unreal.CollisionChannel.cast(2))),
                             visibility_response=str(component.get_collision_response_to_channel(unreal.CollisionChannel.cast(3))),
                             center_probes=probes))
    rows.append(dict(source_instance_index=index, source_transform=transform.export_text(), bounds_cm=bounds,
                     overlapping_colliders=overlaps, approved_for_destruction=False))

report = dict(actor_count=len(actors), visual_actor=owner.get_actor_label(), visual_component=visual.get_path_name(),
              mesh=visual.static_mesh.get_path_name(), candidates=rows,
              qualification='Broad-phase AABB and two simple component traces per overlap. Not an ownership assignment, '
                            'fracture implementation, traversal test, or stable checkpoint identity. Shared colliders '
                            'must be split or retained deliberately before any instance can become destructible.')
assert len(rows) == 19 and len(actors) == 3140
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'destruction-candidates.json').write_text(json.dumps(report, indent=2))
