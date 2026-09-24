"""Import the owned girder and preview a supported, open M13 dock roof unsaved."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
maps={name:root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
before={name:digest(path) for name,path in maps.items()}
assert before['L_Aurelion_M13.umap']=='32d6fcafc66c06e5e53f5f351bbef74893ed833b7d73748c4bbe4aad42df576e'
source=root/'Art/Source/Aurelion/Z12OpenCanopyGirder'
manifest=json.loads((source/'manifest.json').read_text())
assert json.loads((source/'verification.json').read_text())['status']=='round_trip_pass'
spec=manifest['modules'][0]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={key:dest+'/Materials/M_AurelionKit_'+name for key,name in {
    'M_Aurelion_IvoryStone':'PavingIvory',
    'M_Aurelion_AncientGold':'Gold',
    'M_Aurelion_ChannelShadow':'Reveal',
    'M_Aurelion_DarkSteel':'PavingBasalt',
    'M_Aurelion_BlackStone':'ObservationBlackStone',
}.items()}
mesh=helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
old_manifest=json.loads((root/'Art/Source/Aurelion/Z12DepartureCanopy/manifest.json').read_text())
plan=runpy.run_path(str(root/'Scripts/Editor/aurelion_z12_canopy_plan.py'))['plan'](old_manifest)
roof=[row for row in plan if '_Canopy_Coffer_' in row['label'] or '_Canopy_Rib_' in row['label']]
assert len(roof)==66
existing=list(api.get_all_level_actors())
by_label={a.get_actor_label():a for a in existing}
targets=[by_label[r['label']] for r in roof]
assert all(a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for a in targets)
other_state=helper['snapshot_actor_state']([a for a in existing if a not in targets])
for actor in targets:
    actor.static_mesh_component.set_visibility(False)
    actor.static_mesh_component.set_hidden_in_game(True)
placed=[]
for dock,cx in (('Dominion',-3800),('Reformation',3800)):
    for side,x in (('Left',-13.2),('Right',13.2)):
        for i,y in enumerate((-6.225,-2.075,2.075,6.225)):
            actor=api.spawn_actor_from_class(unreal.StaticMeshActor,
                unreal.Vector(cx+x*100,47500+y*100,750))
            actor.set_actor_label('PREVIEW_Aurelion_Z12_%s_OpenCanopyGirder_%s_%d'%(dock,side,i))
            component=actor.static_mesh_component
            component.set_static_mesh(mesh)
            component.set_collision_profile_name('NoCollision')
            component.set_editor_property('can_ever_affect_navigation',False)
            actor.set_actor_enable_collision(False)
            placed.append(actor)
assert len(placed)==16
assert helper['snapshot_actor_state']([a for a in existing if a not in targets])==other_state
assert all(a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for a in placed)
assert {name:digest(path) for name,path in maps.items()}==before
(out/'sparse-canopy-girder-preview.json').write_text(json.dumps(dict(
    status='unsaved_visual_preview',hidden_roof_visuals=len(targets),
    retained_feet_and_pendants=36,placed_girders=[a.get_actor_label() for a in placed],
    mesh_path=mesh.get_path_name(),mesh_source_sha256=digest(source/(spec['asset']+'.fbx')),
    map_hashes_before=before,map_files_unchanged=True,all_other_actor_state_retained=True,
    girder_visual_only=True),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':True,
    'M13_ROUTE_VIEWS':[('west-sparse-girder',(-900,47200,180)),
                       ('east-sparse-girder',(900,47800,180))],
    'M13_ROUTE_YAWS':{'west-sparse-girder':180,'east-sparse-girder':0},
    'M13_ROUTE_PITCHES':{'west-sparse-girder':8,'east-sparse-girder':8},
})
print('M13_SPARSE_CANOPY_GIRDER_UNSAVED_READY')
