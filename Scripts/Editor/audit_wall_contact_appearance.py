"""Read-only material and overlapping-geometry audit of the two climb faces."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]
for a in actors:
    o,e=a.get_actor_bounds(False)
    near=any(abs(o.x-x)<=e.x+25 and abs(o.y-y)<=e.y+25 and abs(o.z-z)<=e.z+25 for x,y,z in [(1426,21930,-1033),(-1374,9430,-433)])
    if not near:continue
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if not c.static_mesh:continue
        rows.append(dict(actor=a.get_actor_label(),component=c.get_path_name(),mesh=c.static_mesh.get_path_name(),
            origin=[o.x,o.y,o.z],extent=[e.x,e.y,e.z],visible=c.get_editor_property('visible'),hidden_in_game=c.get_editor_property('hidden_in_game'),actor_hidden=a.get_editor_property('hidden'),
            collision=str(c.get_collision_enabled()),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],
            overrides=[m.get_path_name() if m else None for m in c.get_editor_property('override_materials')]))
(out/'wall-appearance-audit.json').write_text(json.dumps(rows,indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
