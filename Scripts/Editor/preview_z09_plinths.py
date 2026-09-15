"""Replace two floating art bands with grounded split plinths on their original actors."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z09PlinthKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
manifest=json.loads((source/'manifest.json').read_text());old=json.loads((source/'band-baseline.json').read_text(encoding='utf-8-sig'))
names={r['actor'] for r in old};assert len(names)==2
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));others=[a for a in actors if a.get_actor_label() not in names];before=helper['snapshot_actor_state'](others)
assert not json.loads((source/'Z09SplitPlinth-coplanar.json').read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';persist=bool(globals().get('PERSIST_Z09_PLINTHS',False));spec=manifest['modules'][0]
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_Basalt':'PavingBasalt','M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold'}.items()}
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials);assert mesh
for row in manifest['placements']:
    a=next(a for a in actors if a.get_actor_label()==row['actor']);baseline=next(b for b in old if b['actor']==row['actor']);c=a.get_component_by_class(unreal.StaticMeshComponent)
    assert a.get_actor_transform().export_text()==baseline['actor_transform'] and c.static_mesh.get_path_name()==baseline['mesh']
    assert not a.tags and not a.get_attach_parent_actor() and len(a.get_components_by_class(unreal.ActorComponent))==1
    a.modify();c.modify();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
    a.set_actor_scale3d(unreal.Vector(1,1,1));a.set_actor_location(unreal.Vector(*row['location_cm']),False,False);a.set_actor_rotation(unreal.Rotator(yaw=row['yaw']),False)
    c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False)
assert helper['snapshot_actor_state'](others)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z09_plinths.py'))['check_z09_plinths'](actors)
runpy.run_path(str(root/'Scripts/Editor/check_z09_floor.py'))['check_z09_floor'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z09-plinth-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=result,preserved_other_actors=len(others)),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('north-context',(-200,29900,-1330),(10,90),80),('north-plinth',(-450,30200,-1300),(-10,115),70),('south-context',(200,26300,-1330),(0,-90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z09_plinth_capture','exec'),globals())
