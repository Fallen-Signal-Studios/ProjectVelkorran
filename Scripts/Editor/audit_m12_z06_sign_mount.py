"""Read-only geometric survey for the Z06 graybox route sign."""
from pathlib import Path
import hashlib
import json
import os

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M12'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
map_path = root / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before = hashlib.sha256(map_path.read_bytes()).hexdigest()
actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
sign = next(a for a in actors if a.get_name() == 'TextRenderActor_113')
text = sign.get_component_by_class(unreal.TextRenderComponent)
assert str(text.get_editor_property('text')).startswith('BREACH RESCUE')
location = text.get_world_location()

rows = []
for actor in actors:
    if actor is sign:
        continue
    origin, extent = actor.get_actor_bounds(False)
    dx = max(0, abs(origin.x-location.x)-extent.x)
    dy = max(0, abs(origin.y-location.y)-extent.y)
    dz = max(0, abs(origin.z-location.z)-extent.z)
    if dx*dx+dy*dy+dz*dz > 1800*1800:
        continue
    components = []
    for c in actor.get_components_by_class(unreal.PrimitiveComponent):
        if not isinstance(c, (unreal.StaticMeshComponent, unreal.InstancedStaticMeshComponent)):
            continue
        mesh = c.static_mesh
        components.append(dict(name=c.get_name(), mesh=mesh.get_path_name() if mesh else None,
                               collision=str(c.get_collision_enabled()),
                               visible=c.get_editor_property('visible'),
                               hidden_in_game=c.get_editor_property('hidden_in_game')))
    if not components:
        continue
    rows.append(dict(name=actor.get_name(), label=actor.get_actor_label(),
                     origin=origin.export_text(), extent=extent.export_text(),
                     distance_cm=round((dx*dx+dy*dy+dz*dz)**.5, 2),
                     actor_collision=actor.get_actor_enable_collision(), components=components))
rows.sort(key=lambda r: r['distance_cm'])

traces = []
for direction, endpoint in (
    ('west', (-1800, location.y, location.z)),
    ('east', (1800, location.y, location.z)),
    ('north', (0, location.y+1200, location.z)),
    ('south', (0, location.y-1200, location.z)),
):
    raw = unreal.SystemLibrary.line_trace_single(world, location, unreal.Vector(*endpoint),
             unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [sign],
             unreal.DrawDebugTrace.NONE, True)
    hit = next((v for v in raw if isinstance(v, unreal.HitResult)), None) if isinstance(raw, tuple) else raw
    t = hit.to_tuple() if hit else None
    traces.append(dict(direction=direction, blocking=bool(t and t[0]),
                       actor=t[9].get_actor_label() if t and t[0] and t[9] else None,
                       location=t[5].export_text() if t and t[0] else None))

(out / 'z06-sign-mount-audit.json').write_text(json.dumps(dict(
    status='read_only', sign=sign.get_actor_label(), text=str(text.get_editor_property('text')),
    location=location.export_text(), rotation=text.get_world_rotation().export_text(),
    world_size=text.get_editor_property('world_size'), nearby=rows, traces=traces,
    map_sha256=before), indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert hashlib.sha256(map_path.read_bytes()).hexdigest() == before
