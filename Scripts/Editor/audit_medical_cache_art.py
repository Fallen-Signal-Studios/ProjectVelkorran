"""Read-only cache visual, physical body and mission linkage measurements."""
from pathlib import Path
import json,os,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();rows=[]
def xyz(v):return [v.x,v.y,v.z]
for a in actors:
    if not isinstance(a,unreal.SovAurelionMedicalCache):continue
    c=a.visual;b=c.static_mesh.get_bounds();t=c.get_world_transform();body=a.body
    rows.append(dict(actor=a.get_actor_label(),path=a.get_path_name(),actor_transform=a.get_actor_transform().export_text(),visual=c.get_path_name(),visual_transform=t.export_text(),location=xyz(t.translation),scale=xyz(t.scale3d),quaternion=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],mesh=c.static_mesh.get_path_name(),origin=xyz(b.origin),extent=xyz(b.box_extent),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),actor_hidden=a.get_editor_property('hidden'),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),actor_collision=a.get_actor_enable_collision(),body_transform=body.get_world_transform().export_text(),body_extent=xyz(body.get_unscaled_box_extent()),body_collision=str(body.get_collision_enabled()),body_profile=str(body.get_collision_profile_name()),cache_id=str(a.cache_id),support=a.support.get_path_name() if a.support else None,health_fraction=a.health_fraction,consumed=a.is_consumed(),interaction_distance=a.interactable.interaction_distance,interaction_time=a.interactable.interaction_time,label_transform=a.label.get_world_transform().export_text(),label_text=str(a.label.text)))
(out/'medical-cache-baseline.json').write_text(json.dumps(rows,indent=2));assert len(actors)==3140 and rows
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
