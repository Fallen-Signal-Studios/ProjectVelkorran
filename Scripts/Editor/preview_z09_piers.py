"""Replace the eight vendor pier visuals with the six-metre Blender variant."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09PierKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
old=json.loads((source/'pier-baseline.json').read_text(encoding='utf-8-sig'));manifest=json.loads((source/'manifest.json').read_text())
a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==old['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==[r['transform'] for r in old['instances']]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helper['snapshot_actor_state'](actors)
assert not json.loads((source/'Z09EngagedPier-coplanar.json').read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';persist=bool(globals().get('PERSIST_Z09_PIERS',False))
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
spec=manifest['modules'][0]
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert mesh
c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False)
for row in manifest['placements']:c.add_instance(unreal.Transform(location=unreal.Vector(*row['location_cm']),rotation=unreal.Rotator(yaw=row['yaw']),scale=unreal.Vector(1,1,1)),world_space=True)
assert helper['snapshot_actor_state'](actors)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z09_piers.py'))['check_z09_piers'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z09-piers-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=result),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('entry',(-300,25750,-1330),(0,90),80),('pier-detail',(-400,26350,-1260),(5,146),65),('reverse',(350,30200,-1330),(0,-90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z09_pier_capture','exec'),globals())
