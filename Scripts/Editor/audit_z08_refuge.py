"""Measure retained refuge panels and baffles without editing."""
from pathlib import Path
import json,os,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());rows=[]
for a in actors:
    if not a.get_actor_label().startswith(('ART_RecessPanel_','ART_RefugeBaffle_')):continue
    c=a.get_component_by_class(unreal.StaticMeshComponent)
    if not c:continue
    mesh=c.static_mesh;b=mesh.get_bounds();t=a.get_actor_transform()
    rows.append(dict(actor=a.get_actor_label(),path=a.get_path_name(),component=c.get_path_name(),actor_transform=t.export_text(),component_transform=c.get_world_transform().export_text(),location=[t.translation.x,t.translation.y,t.translation.z],scale=[t.scale3d.x,t.scale3d.y,t.scale3d.z],quaternion=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],actor_collision=a.get_actor_enable_collision(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),mesh=mesh.get_path_name(),origin=[b.origin.x,b.origin.y,b.origin.z],extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]))
assert len(rows)==17 and len(actors)==3140
(out/'refuge-baseline.json').write_text(json.dumps(sorted(rows,key=lambda r:r['actor']),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
