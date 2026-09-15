"""Read-only native support gates and nearby visual/collision coverage."""
from pathlib import Path
import json,os,runpy,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());rows=[]
def xyz(v):return [v.x,v.y,v.z]
for a in actors:
    native=isinstance(a,(unreal.SovAurelionPrioritySupport,unreal.SovAurelionSupportPresentation))
    for c in a.get_components_by_class(unreal.PrimitiveComponent):
        loc=c.get_world_location()
        if not native and not (-3350<loc.x<-2800 and 22300<loc.y<23000):continue
        row=dict(actor=a.get_actor_label(),path=a.get_path_name(),class_name=a.get_class().get_name(),actor_transform=a.get_actor_transform().export_text(),component=c.get_path_name(),transform=c.get_world_transform().export_text(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),actor_hidden=a.get_editor_property('hidden'),actor_collision=a.get_actor_enable_collision())
        if isinstance(c,unreal.BoxComponent):row['box_extent']=xyz(c.get_unscaled_box_extent())
        if isinstance(c,unreal.StaticMeshComponent):
            m=c.static_mesh;row['mesh']=m.get_path_name() if m else None
            row['materials']=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]
            if m:
                b=m.get_bounds();row['mesh_origin']=xyz(b.origin);row['mesh_extent']=xyz(b.box_extent)
        rows.append(row)
(out/'cache-enclosure-audit.json').write_text(json.dumps(dict(actor_count=len(actors),components=rows),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
