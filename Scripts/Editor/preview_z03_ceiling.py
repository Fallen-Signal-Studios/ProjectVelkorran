"""Replace the whole saved vendor ceiling batch with true-size Aurelion coffers."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z03CeilingKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());old=json.loads((source/'ceiling-baseline.json').read_text());a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==old['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==[r['transform'] for r in old['instances']]
assert not json.loads((source/'coplanar.json').read_text())['overlaps']
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helper['snapshot_actor_state'](actors);manifest=json.loads((source/'manifest.json').read_text());spec=manifest['modules'][0]
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()};persist=bool(globals().get('PERSIST_Z03_CEILING',False))
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
a.modify();c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[]);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
for row in manifest['placements']:c.add_instance(unreal.Transform(location=unreal.Vector(*row['location_cm']),rotation=unreal.Rotator(),scale=unreal.Vector(1,1,1)),world_space=True)
assert helper['snapshot_actor_state'](actors)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z03_ceiling.py'))['check_z03_ceiling'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z03-ceiling-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_actor_states=3140),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=capture.split('views=[',1)[0];suffix=capture.split('state=dict',1)[1]
views="views=[('z03-approach',(6800,-19300,165),(0,90),90),('z03-middle',(6800,-17200,165),(15,90),90),('z03-coffer',(7000,-18000,380),(65,90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z03_ceiling_review','exec'),globals())
