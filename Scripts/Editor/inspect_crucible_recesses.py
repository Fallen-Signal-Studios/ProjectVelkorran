"""Read-only authored crucible census; editor camera selection is presentation only."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'crucible-recesses.json'
assert not out.exists()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows = []
for actor in actors:
    label = actor.get_actor_label()
    p = actor.get_actor_location()
    if ('Z08' not in label and 'E4_' not in label and 'Stretcher' not in label
            and not (18000 < p.y < 24500 and abs(p.x) < 4000)):
        continue
    origin, extent = actor.get_actor_bounds(False)
    meshes = []
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.get_editor_property('static_mesh')
        meshes.append(dict(component=component.get_name(), mesh=mesh.get_path_name() if mesh else None,
            collision=str(component.get_collision_enabled()), location=component.get_world_location().export_text()))
    rows.append(dict(label=label, path=actor.get_path_name(), actor_class=actor.get_class().get_name(),
        location=p.export_text(), bounds_origin=origin.export_text(), bounds_extent=extent.export_text(),
        collision=actor.get_actor_enable_collision(), meshes=meshes))
out.write_text(json.dumps(dict(scope='Authored geometry only; no gameplay qualification', actors=rows), indent=2), encoding='utf8')
views = [a for a in actors if a.get_actor_label() == 'REVIEW_Camera_Quarantine']
if len(views) == 1:
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        views[0].get_actor_location(), views[0].get_actor_rotation())
unreal.log('CRUCIBLE_RECESS_CENSUS ' + str(out))
