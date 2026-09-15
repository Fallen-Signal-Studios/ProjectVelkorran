"""Read-only carrier ownership and envelopes before replacing the graybox craft."""
from pathlib import Path
import json, os, unreal
root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
rows = []
for a in actors:
    if not a.get_actor_label().startswith('Aurelion_Carrier_'):
        continue
    cs = a.get_components_by_class(unreal.StaticMeshComponent)
    row = dict(label=a.get_actor_label(), path=a.get_path_name(), transform=a.get_actor_transform().export_text(),
        parent=a.get_attach_parent_actor().get_path_name() if a.get_attach_parent_actor() else None,
        hidden=a.get_editor_property('hidden'), collision=a.get_actor_enable_collision(), components=[])
    for c in cs:
        o,e,_ = unreal.SystemLibrary.get_component_bounds(c)
        row['components'].append(dict(path=c.get_path_name(), mesh=c.static_mesh.get_path_name() if c.static_mesh else None,
            transform=c.get_world_transform().export_text(), visible=c.get_editor_property('visible'),
            hidden=c.get_editor_property('hidden_in_game'), profile=str(c.get_collision_profile_name()),
            collision=str(c.get_collision_enabled()), nav=c.get_editor_property('can_ever_affect_navigation'),
            bounds=[[o.x-e.x,o.y-e.y,o.z-e.z],[o.x+e.x,o.y+e.y,o.z+e.z]],
            instances=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
                if isinstance(c,unreal.InstancedStaticMeshComponent) else None))
    rows.append(row)
assert len(rows)==8
(out/'carrier-baseline.json').write_text(json.dumps(dict(actor_count=len(actors),parts=rows),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('carrier-route',(6000,-2500,350),(0,30),90),('carrier-wide',(1000,-8000,4500),(-18,42),75),('carrier-forward',(6500,10000,2000),(-12,-55),75)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'carrier_audit_capture','exec'),globals())
