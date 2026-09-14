"""Replace six legacy columns with one fitted custom pier mesh, optionally saving."""
from pathlib import Path
import json,os,runpy,shutil,time,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/Z08ColumnKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==3140
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
fit=json.loads((source/'column-fit.json').read_text());c=by_label[fit['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==fit['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==fit['all_instance_transforms']
assert not json.loads((source/'coplanar-faces.json').read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';spec=json.loads((source/'manifest.json').read_text())['modules'][0];path=dest+'/Meshes/'+spec['asset']
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
mesh=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(spec['asset']+'.fbx')).resolve()
c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
for row in fit['instances']:
    lo,hi=row['bounds'];x=(lo[0]+hi[0])/2;y=(lo[1]+hi[1])/2
    c.add_instance(unreal.Transform(location=unreal.Vector(x,y,lo[2]),rotation=unreal.Rotator(yaw=-90 if x<0 else 90),scale=unreal.Vector(1,1,1)),world_space=True)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_columns.py'))['check_z08_columns'](original)
persist=bool(globals().get('PERSIST_Z08_COLUMNS',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-column-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('west-pier',unreal.Vector(-2450,19350,-920),unreal.Rotator(pitch=15,yaw=160),75)")
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('east-piers',unreal.Vector(2450,19350,-920),unreal.Rotator(pitch=15,yaw=20),75)").replace('z01-','z08-')
exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'column_review','exec'),globals())
