"""Read-only dimensions and collision ownership around the Z11 central window pier."""
import json
import os
from pathlib import Path

import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'z11-pier-survey.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_name() == 'L_Aurelion_M13'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()


def vec(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]


rows = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    p = actor.get_actor_location()
    if not (-2000 <= p.x <= 1600 and 42000 <= p.y <= 44000):
        continue
    origin, extent = actor.get_actor_bounds(False)
    components = []
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = c.static_mesh
        if not mesh:
            continue
        row = dict(name=c.get_name(), mesh=mesh.get_path_name(),
                   collision=str(c.get_collision_enabled()),
                   can_affect_nav=bool(c.get_editor_property('can_ever_affect_navigation')))
        if isinstance(c, unreal.InstancedStaticMeshComponent):
            row['instances'] = []
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, world_space=True)
                q = t.translation
                if -2000 <= q.x <= -600 and 42000 <= q.y <= 44000:
                    row['instances'].append(dict(index=i, location=vec(q),
                                                 scale=vec(t.scale3d), rotation=t.rotation.export_text()))
        components.append(row)
    rows.append(dict(label=actor.get_actor_label(), path=actor.get_path_name(),
                     location=vec(p), bounds_origin=vec(origin), bounds_extent=vec(extent),
                     actor_collision=bool(actor.get_actor_enable_collision()),
                     components=components))

out.write_text(json.dumps(dict(status='read_only', map=world.get_name(), actors=rows), indent=2))
print('Z11_PIER_SURVEY_PASS', out)
