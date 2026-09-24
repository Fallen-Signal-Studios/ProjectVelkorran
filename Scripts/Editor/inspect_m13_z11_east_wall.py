"""Read-only survey and fixed views of the remaining Z11 east stock-panel wall."""
from pathlib import Path
import hashlib
import json
import os
import runpy

import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
maps={n:root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={n:digest(p) for n,p in maps.items()}
assert before['L_Aurelion_M13.umap']=='22fbdd20587916329bea43f3f334b39feeb6fa15a28a34d37e070c6b64c53bcb'
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
by_label={a.get_actor_label():a for a in actors}
owner=by_label['Aurelion_Art_M13_Z11_1_f5f218']
part=next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
          if c.static_mesh and c.static_mesh.get_name()=='SM_Scifi_Wall_03_4m')
assert part.get_instance_count()==114
assert part.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
rows=[]
for i in range(114):
    t=part.get_instance_transform(i,world_space=True)
    assert abs(t.translation.x-1200)<.01
    rows.append(dict(index=i,transform=t.export_text()))
near=[]
for a in actors:
    p=a.get_actor_location()
    if 800<=p.x<=1600 and 42200<=p.y<=44000 and -100<=p.z<=800:
        near.append(dict(label=a.get_actor_label(),class_name=a.get_class().get_name(),
                         transform=a.get_actor_transform().export_text(),collision=a.get_actor_enable_collision()))
assert {n:digest(p) for n,p in maps.items()}==before
(out/'z11-east-survey.json').write_text(json.dumps(dict(
    status='read_only',map_hashes=before,owner=owner.get_actor_label(),
    vendor_mesh=part.static_mesh.get_path_name(),vendor_instances=rows,
    native_east_wall=by_label['Z11_Wall_EW1'].get_actor_transform().export_text(),
    nearby_actors=near),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('east-room',(-400,43100,190)),('east-close',(650,43100,190))],
    'M13_ROUTE_YAWS':{'east-room':0,'east-close':0},
    'M13_ROUTE_PITCHES':{'east-room':-6,'east-close':-6},
})
print('M13_Z11_EAST_WALL_READ_ONLY_PASS')
