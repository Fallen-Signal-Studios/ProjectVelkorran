"""Preview/save the owned Crownmark centerpiece while retaining native collision."""
from pathlib import Path
import hashlib,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z10CrownmarkCore'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
actors=list(actorsub.get_all_level_actors())
owner=next(a for a in actors if a.get_actor_label()=='Z10_Integrated_Crownmark')
components=owner.get_components_by_class(unreal.StaticMeshComponent)
body=next(c for c in components if c.static_mesh and c.static_mesh.get_name()=='SM_research_unit_1')
glass=next(c for c in components if c.static_mesh and c.static_mesh.get_name()=='SM_research_unit_glass')
assert body.get_collision_enabled()==glass.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
native=[c for c in components if c not in (body,glass)]
assert len(native)==1 and native[0].get_collision_profile_name()=='BlockAll'
def native_state():
    return [(c.get_world_transform().export_text(),str(c.get_collision_enabled()),str(c.get_collision_profile_name()),c.static_mesh.get_path_name()) for c in native]
native_before=native_state()
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state'](actors)
persist=bool(globals().get('SAVE_CROWNMARK_CORE',False))
spec=json.loads((source/'manifest.json').read_text())['modules'][0]
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {
    'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold',
    'M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}.items()}
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert mesh
b=mesh.get_bounds()
for axis,limit in [('x',67.6),('y',69.2),('z',100)]:
    assert abs(getattr(b.origin,axis))+getattr(b.box_extent,axis)<=limit+.1,(axis,b)
body.modify();body.set_static_mesh(mesh);body.set_editor_property('override_materials',[])
body.set_world_transform(unreal.Transform(location=unreal.Vector(0,35100,-1680),scale=unreal.Vector(1,1,1)),False,True)
glass.modify();glass.set_visibility(False);glass.set_hidden_in_game(True)
assert native_state()==native_before and helper['snapshot_actor_state'](actors)==before
expected=body.get_world_transform().export_text()
report=dict(status='unsaved_preview',mesh=mesh.get_path_name(),transform=expected,
    native_collision_preserved=True,all_actor_transforms_and_collision_preserved=True)
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(actorsub.get_all_level_actors());assert helper['snapshot_actor_state'](current)==before
    owner=next(a for a in current if a.get_actor_label()=='Z10_Integrated_Crownmark')
    components=owner.get_components_by_class(unreal.StaticMeshComponent)
    body=next(c for c in components if c.static_mesh==mesh)
    glass=next(c for c in components if c.static_mesh and c.static_mesh.get_name()=='SM_research_unit_glass')
    native=[c for c in components if c not in (body,glass)]
    assert native_state()==native_before and body.get_world_transform().export_text()==expected
    assert not glass.is_visible() and glass.get_editor_property('hidden_in_game')
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'crownmark-core-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('crownmark',(180,34780,-1620))],
    M13_ROUTE_PITCHES={'crownmark':-8},M13_ROUTE_YAWS={'crownmark':119}))
