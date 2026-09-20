"""Replace only the two decorative assent consoles, preserving interaction owners."""
from pathlib import Path
import hashlib,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z10AssentConsole';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
actors=list(sub.get_all_level_actors());owner=next(a for a in actors if a.get_actor_label()=='Aurelion_Art_M13_Z10_13_211d30')
c=owner.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.get_instance_count()==2 and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert c.static_mesh.get_name()=='SM_KB3D_CPI_PropConsoleLarge_A'
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helper['snapshot_actor_state'](actors)
old=c.static_mesh.get_bounds();poses=[c.get_instance_transform(i,world_space=True) for i in range(2)]
baseline=json.loads((source/'fixture-baseline.json').read_text(encoding='utf-8-sig'))
assert c.static_mesh.get_path_name()==baseline['mesh'] and [t.export_text() for t in poses]==baseline['instances']
centers=[unreal.MathLibrary.transform_location(t,old.origin) for t in poses]
assert all(abs(p.y-34800)<.01 and abs(p.z+1730)<.01 for p in centers)
spec=json.loads((source/'manifest.json').read_text())['modules'][0];dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}.items()}
persist=bool(globals().get('SAVE_ASSENT_CONSOLES',False))
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
b=mesh.get_bounds()
for axis,limit in [('x',50),('y',35),('z',60)]:assert abs(getattr(b.origin,axis))+getattr(b.box_extent,axis)<=limit+.1
c.modify();c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
for center in centers:c.add_instance(unreal.Transform(location=center,scale=unreal.Vector(1,1,1)),world_space=True)
expected=[c.get_instance_transform(i,world_space=True).export_text() for i in range(2)]
assert helper['snapshot_actor_state'](actors)==before
report=dict(status='unsaved_preview',mesh=mesh.get_path_name(),poses=expected,all_actor_transforms_and_collision_preserved=True,interaction_actors_untouched=True)
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(sub.get_all_level_actors());assert helper['snapshot_actor_state'](current)==before
    restored=next(a for a in current if a.get_actor_label()=='Aurelion_Art_M13_Z10_13_211d30').get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert restored.static_mesh==mesh and restored.get_instance_count()==2
    assert [restored.get_instance_transform(i,world_space=True).export_text() for i in range(2)]==expected
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'assent-console-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('console',(-340,34580,-1580))],M13_ROUTE_PITCHES={'console':-18},M13_ROUTE_YAWS={'console':72}))
