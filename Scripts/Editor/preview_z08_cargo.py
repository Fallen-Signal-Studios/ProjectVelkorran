"""Replace nineteen cargo visuals, optionally save, then capture."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/Z08CargoKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
old=json.loads((source/'cargo-baseline.json').read_text());a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert not json.loads((source/'coplanar-faces.json').read_text())['overlaps']
assert c.static_mesh.get_path_name()==old['mesh'] and c.get_instance_count()==19
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(19)]==[r['transform'] for r in old['instances']]
dest='/Game/Aurelion/Environment/ArchitectureKit';spec=json.loads((source/'manifest.json').read_text())['modules'][0];asset=dest+'/Meshes/'+spec['asset']
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
mesh=unreal.load_asset(asset) if globals().get('PERSIST_Z08_CARGO',False) and unreal.EditorAssetLibrary.does_asset_exist(asset) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(spec['asset']+'.fbx')).resolve()
c.modify();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
assert helpers['snapshot_actor_state'](actors)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z08_cargo.py'))['check_z08_cargo'](actors)
persist=bool(globals().get('PERSIST_Z08_CARGO',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-cargo-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_actor_states=len(actors)),indent=2))
exec(compile((root/'Scripts/Editor/review_z08_cargo.py').read_text(),'cargo_capture','exec'),globals())
