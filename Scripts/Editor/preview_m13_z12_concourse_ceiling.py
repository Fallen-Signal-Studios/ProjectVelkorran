"""Unsaved fit of 28 authored roof bays against the actual M13 Z12 concourse."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=root/'Art/Source/Aurelion/Z12ConcourseCeiling'
manifest=json.loads((source/'manifest.json').read_text())
assert manifest['layout']==[7,4] and manifest['room_dimensions_m']==[42,24]
assert json.loads((source/'verification.json').read_text())['status']=='round_trip_pass'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
existing=list(actors.get_all_level_actors())
bylabel={a.get_actor_label():a for a in existing}
owner=bylabel['Aurelion_Art_M13_Z12_24_8bdcf4']
component=next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
    if c.static_mesh and c.static_mesh.get_name()=='SM_Scifi_Floor_04')
assert component.get_instance_count()==22
assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
original=[component.get_instance_transform(i,world_space=True).export_text() for i in range(22)]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
baseline=helper['snapshot_actor_state']([a for a in existing if a!=owner])
maps={name:root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
hashes={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in maps.items()}

dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={
    'M_Aurelion_IvoryStone':dest+'/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_ViewIvory':dest+'/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_ViewBlackStone':dest+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_ChannelShadow':dest+'/Materials/M_AurelionKit_Reveal',
    'M_Aurelion_AncientGold':dest+'/Materials/M_AurelionKit_Gold',
    'M_Aurelion_CeilingContact':'/Game/Aurelion/Art/Props/aURELION_pILLAR/Materials/M_GoldEmmissive',
}
meshes={s['asset']:helper['import_owned_mesh'](s,source,dest+'/Meshes',materials)
        for s in manifest['modules']}
assert len(meshes)==2
component.modify();component.clear_instances()
assert component.get_instance_count()==0
created=[];placement=[]
for ix in range(7):
    for iy in range(4):
        variant='A' if (ix+iy)%2==0 else 'B'
        asset='SM_Aurelion_KIT_Z12ConcourseCeiling_'+variant
        loc=(-1800+600*ix,46600+600*iy,560)
        actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc))
        actor.set_actor_label('PREVIEW_Z12_ConcourseRoof_%d_%d_%s'%(ix,iy,variant))
        actor.static_mesh_component.set_static_mesh(meshes[asset])
        actor.static_mesh_component.set_collision_profile_name('NoCollision')
        actor.static_mesh_component.set_editor_property('can_ever_affect_navigation',False)
        created.append(actor)
        placement.append(dict(label=actor.get_actor_label(),asset=asset,location=list(loc)))
assert len(created)==28
assert all(a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
           and not a.static_mesh_component.get_editor_property('can_ever_affect_navigation') for a in created)
assert helper['snapshot_actor_state']([a for a in existing if a!=owner])==baseline
assert {name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in maps.items()}==hashes
(out/'z12-ceiling-preview.json').write_text(json.dumps(dict(
    status='unsaved_preview',map_sha256=hashes,removed_visual_instances=original,
    owner_label=owner.get_actor_label(),retained_other_actors=True,
    created_visual_only=placement,source_fbx_sha256={s['asset']:hashlib.sha256(
        (source/(s['asset']+'.fbx')).read_bytes()).hexdigest() for s in manifest['modules']},
    imported_meshes={k:v.get_path_name() for k,v in meshes.items()},
    qualification='Unsaved editor view only; PIE lighting and route acceptance separate.'
),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':True,
    'M13_ROUTE_VIEWS':[('center-east',(0,47500,180)),('center-west',(0,47500,180)),
                       ('center-up',(0,47500,180)),('north-end',(0,48000,180))],
    'M13_ROUTE_YAWS':{'center-east':0,'center-west':180,'center-up':90,'north-end':-90},
    'M13_ROUTE_PITCHES':{'center-east':5,'center-west':5,'center-up':25,'north-end':8}})
print('Z12_CONCOURSE_CEILING_UNSAVED_PREVIEW_PASS')
