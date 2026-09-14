"""Read-only crucible art component inspection from a stopped editor."""
import json
from pathlib import Path
import unreal

rows = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not actor.get_actor_label().startswith('Aurelion_Art_M12_Z08_'):
        continue
    components = []
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = c.get_editor_property('static_mesh')
        entry = dict(name=c.get_name(), mesh=mesh.get_path_name() if mesh else None,
                     transform=c.get_world_transform().export_text(),
                     collision=str(c.get_collision_enabled()),
                     materials=[c.get_material(i).get_path_name() if c.get_material(i) else None
                                for i in range(c.get_num_materials())])
        if isinstance(c, unreal.InstancedStaticMeshComponent):
            entry['instances'] = [c.get_instance_transform(i, world_space=True).export_text()
                                  for i in range(c.get_instance_count())]
        components.append(entry)
    rows.append(dict(label=actor.get_actor_label(), components=components))
out = Path(__file__).resolve().parents[2] / 'Saved/Validation/Aurelion/crucible-art-components-20260913.json'
out.write_text(json.dumps(rows, indent=2), encoding='utf8')
unreal.log('CRUCIBLE_ART_INSPECTION ' + str(out))
