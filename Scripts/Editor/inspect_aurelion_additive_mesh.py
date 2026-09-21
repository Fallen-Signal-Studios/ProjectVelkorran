"""Read-only ownership audit of the invalid additive Nanite mesh reported in M12."""
import json,os,unreal
from pathlib import Path
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
rows=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh=component.get_editor_property('static_mesh')
        if mesh and 'splitting_star' in mesh.get_name():
            rows.append(dict(actor=actor.get_path_name(),label=actor.get_actor_label(),
                actor_class=actor.get_class().get_path_name(),component=component.get_path_name(),
                mesh=mesh.get_path_name(),disallow_nanite=component.get_editor_property('disallow_nanite'),
                materials=[m.get_path_name() if m else None for m in component.get_materials()]))
(out/'additive-mesh.json').write_text(json.dumps(dict(read_only=True,rows=rows),indent=2))
