"""Preview custom departure seats; save only an explicitly reviewed mesh and fit."""
from pathlib import Path
import hashlib,itertools,json,os,runpy,shutil
import unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z12DepartureSeat'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(actorsub.get_all_level_actors())
baseline=json.loads((source/'baseline.json').read_text(encoding='utf-8-sig'))
targets=[next(a for a in actors if a.get_actor_label()==row['actor']) for row in baseline]
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state']([a for a in actors if a not in targets])
maps={n:hashlib.sha256((root/'Content/Aurelion/Maps'/n).read_bytes()).hexdigest() for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
persist=bool(globals().get('SAVE_DEPARTURE_SEATS',False));review=None
dest='/Game/Aurelion/Environment/ArchitectureKit'
spec=json.loads((source/'manifest.json').read_text())['modules'][0];asset=dest+'/Meshes/'+spec['asset']
asset_file=root/('Content/'+asset.removeprefix('/Game/')+'.uasset')
if persist:
    review=json.loads(Path(globals()['REVIEWED_DEPARTURE_SEATS']).read_text())
    assert review['status']=='unsaved_preview' and review['all_bounds_inside_original_footprints']
    assert maps==review['map_hashes_before'] and hashlib.sha256(asset_file.read_bytes()).hexdigest()==review['mesh_sha256']
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
mesh=unreal.load_asset(asset) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert mesh;expected={}
for actor,row in zip(targets,baseline):
    c=actor.get_component_by_class(unreal.StaticMeshComponent)
    assert actor.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
    assert not actor.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    old_origin,old_extent=actor.get_actor_bounds(False)
    side=-1 if 'West' in row['actor'] else 1
    actor.modify();c.modify();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
    actor.set_actor_location(unreal.Vector(side*1750,47100,.02),False,False)
    actor.set_actor_rotation(unreal.Rotator(yaw=180 if side<0 else 0),False)
    c.set_editor_property('can_ever_affect_navigation',False)
    origin,extent=actor.get_actor_bounds(False)
    assert all(abs(getattr(origin,k)-getattr(old_origin,k))+getattr(extent,k)<=getattr(old_extent,k)+.1 for k in ('x','y'))
    assert -.1<=origin.z-extent.z<=.1 and origin.z+extent.z<=old_origin.z+old_extent.z+.1
    expected[row['actor']]=dict(transform=actor.get_actor_transform().export_text(),mesh=mesh.get_path_name(),collision=str(c.get_collision_enabled()))
assert helper['snapshot_actor_state']([a for a in actors if a not in targets])==before
if persist:assert expected==review['expected']
report=dict(status='unsaved_preview',expected=expected,mesh_sha256=hashlib.sha256(asset_file.read_bytes()).hexdigest(),
    map_hashes_before=maps,all_bounds_inside_original_footprints=True,other_actor_transforms_and_collision_preserved=True)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    restored=list(actorsub.get_all_level_actors());restored_targets=[a for a in restored if a.get_actor_label() in expected]
    assert len(restored_targets)==2
    for actor in restored_targets:
        c=actor.get_component_by_class(unreal.StaticMeshComponent);row=expected[actor.get_actor_label()]
        assert actor.get_actor_transform().export_text()==row['transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert not actor.get_actor_enable_collision() and str(c.get_collision_enabled())==row['collision']
        assert not c.get_editor_property('can_ever_affect_navigation')
    assert helper['snapshot_actor_state']([a for a in restored if a not in restored_targets])==before
    assert hashlib.sha256((root/'Content/Aurelion/Maps/L_Aurelion_M12.umap').read_bytes()).hexdigest()==maps['L_Aurelion_M12.umap']
    report.update(status='saved_reloaded',protected_m12_unchanged=True)
(out/'departure-seat-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not persist,
    'M13_ROUTE_VIEWS':[('seat-east',(1500,46800,150)),('seat-west',(-1500,47400,150)),('selene-background',(1350,48200,170))],
    'M13_ROUTE_YAWS':{'seat-east':45,'seat-west':-135,'selene-background':-90},
    'M13_ROUTE_PITCHES':{'seat-east':-12,'seat-west':-12}})
