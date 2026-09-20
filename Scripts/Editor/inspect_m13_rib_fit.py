"""Read the chamber support mesh pivot and retained instance placements."""
from pathlib import Path
import json,os,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
owner=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Aurelion_Art_Z10_Chamber_ribs')
c=owner.get_component_by_class(unreal.InstancedStaticMeshComponent);b=c.static_mesh.get_bounds()
(out/'rib-baseline.json').write_text(json.dumps(dict(actor=owner.get_actor_label(),mesh=c.static_mesh.get_path_name(),
    mesh_origin=[b.origin.x,b.origin.y,b.origin.z],mesh_extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z],
    collision=str(c.get_collision_enabled()),instances=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]),indent=2))
