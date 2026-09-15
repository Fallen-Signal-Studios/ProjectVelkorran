"""Read-only collision and neighboring art bounds around the gallery roof volume."""
from pathlib import Path
import json,os,runpy,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));rows=[]
for a in actors:
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        mesh=c.static_mesh
        if not mesh:continue
        b=mesh.get_bounds();o=[b.origin.x,b.origin.y,b.origin.z];e=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
        ts=[(i,c.get_instance_transform(i,world_space=True)) for i in range(c.get_instance_count())] if isinstance(c,unreal.InstancedStaticMeshComponent) else [(None,c.get_world_transform())]
        for index,t in ts:
            lo,hi=geo['bounds'](geo['corners'](t,o,e))
            if not all(min(hi[i],[800,30850,-840][i])>=max(lo[i],[-800,25350,-901][i]) for i in range(3)):continue
            rows.append(dict(actor=a.get_actor_label(),component=c.get_path_name(),index=index,mesh=mesh.get_path_name(),bounds=[lo,hi],collision=str(c.get_collision_enabled()),actor_collision=a.get_actor_enable_collision(),actor_hidden=a.get_editor_property('hidden'),visible=c.get_editor_property('visible'),hidden_in_game=c.get_editor_property('hidden_in_game')))
(out/'z09-roof-neighbors.json').write_text(json.dumps(dict(status='read_only',candidate_volume=[[-800,25350,-901],[800,30850,-840]],rows=rows,qualification='Broad-phase bounds only; overlap does not prove collision ownership or contact.'),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
