"""Replace retained generic floor visuals with fitted true-size deck assembly."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/Z08DeckKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
old=json.loads((source/'deck-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text());a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==old['mesh'] and c.get_instance_count()==42
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(42)]==[row['transform'] for row in old['instances']]
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
for spec in manifest['modules']:
    path=dest+'/Meshes/'+spec['asset'];mesh=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
c.add_instance(unreal.Transform(location=unreal.Vector(*[v*100 for v in manifest['anchor_world_m']]),rotation=unreal.Rotator(),scale=unreal.Vector(1,1,1)),world_space=True)
assert helpers['snapshot_actor_state'](actors)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z08_decking.py'))['check_z08_decking'](actors)
persist=bool(globals().get('PERSIST_Z08_DECKING',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-deck-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_actor_states=len(actors)),indent=2))
exec(compile((root/'Scripts/Editor/review_z08_railings.py').read_text(),'deck_capture','exec'),globals())
