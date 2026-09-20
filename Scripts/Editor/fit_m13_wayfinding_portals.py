"""Fit owned sign housings around the retained lettering; preview before an explicit scoped save."""
import hashlib,json,os,runpy,shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
persist=bool(globals().get('SAVE_WAYFINDING',False))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors_sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
source=root/'Art/Source/Aurelion/WayfindingPortals'
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
actors=list(actors_sub.get_all_level_actors());before=helpers['snapshot_actor_state'](actors)
assert not any(a.get_actor_label().startswith('Aurelion_WayfindingPortal_') for a in actors),'Refusing duplicate authoring'
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {
    'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold',
    'M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}.items()}
meshes={}
for spec in json.loads((source/'manifest.json').read_text())['modules']:
    meshes[spec['asset']]=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    assert meshes[spec['asset']]
unreal.SystemLibrary.execute_console_command(world,'Editor.AsyncAssetCompilationFinishAll')
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
collision_inventory={name:sm.get_convex_collision_count(mesh) for name,mesh in meshes.items()}
(out/'loaded-collision.json').write_text(json.dumps(collision_inventory,indent=2))
assert all(count==3 for count in collision_inventory.values()),collision_inventory
placements=[('Z10','Aurelion_Art_Sign_Z10_72eb71','Chamber',-1800.,33400.,-1460.),
    ('Z11','Aurelion_Art_Sign_Z11_9bb415','Gallery',0.,42000.,240.),
    ('Z12','Aurelion_Art_Sign_Z12_f79ad4','Gallery',0.,46600.,240.)]
report=dict(status='unsaved_preview',signs=[],collision_checks=[])
created=[]
for zone,sign_label,kind,floor,y,text_z in placements:
    sign=next(a for a in actors if a.get_actor_label()==sign_label)
    text=sign.get_component_by_class(unreal.TextRenderComponent);p=sign.get_actor_location()
    assert abs(p.x)<.01 and abs(p.y-y)<.01 and abs(p.z-text_z)<.01
    assert abs(text.world_size-24.)<.01 and str(text.get_editor_property('vertical_alignment')).endswith('EVRTA_TEXT_BOTTOM: 2>')
    # FBX's handedness conversion reverses the source face direction in this kit.
    actor=actors_sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,y+3,floor),unreal.Rotator(yaw=180))
    assert actor
    actor.set_actor_label('Aurelion_WayfindingPortal_'+zone);actor.set_folder_path('Aurelion/Architecture/Wayfinding')
    component=actor.static_mesh_component
    assert component.set_static_mesh(meshes['SM_Aurelion_KIT_Wayfinding'+kind])
    component.set_collision_profile_name('BlockAll');component.set_editor_property('can_ever_affect_navigation',True)
    actor.set_actor_enable_collision(True);created.append(actor)
    report['signs'].append(dict(actor=actor.get_actor_label(),mesh=component.static_mesh.get_path_name(),
        transform=actor.get_actor_transform().export_text(),sign=sign_label,text=str(text.text),
        sign_transform=sign.get_actor_transform().export_text()))
    # Isolate this housing: other scene collision must not mask the new opening's result.
    ignored=[a for a in actors_sub.get_all_level_actors() if a!=actor]
    for x,z,should_hit in [(-100,floor+110,False),(0,floor+110,False),(100,floor+110,False),
                          (181,floor+110,True),(0,text_z+24,True)]:
        radius,half_height=(55.,100.) if not should_hit else (10.,10.)
        raw=unreal.SystemLibrary.capsule_trace_single(world,unreal.Vector(x,y-100,z),unreal.Vector(x,y+100,z),
            radius,half_height,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
        hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        blocked=bool(hit and hit.to_tuple()[0])
        assert blocked==should_hit,(zone,x,z,blocked,should_hit)
        report['collision_checks'].append(dict(zone=zone,x=x,z=z,blocked=blocked,expected_blocked=should_hit))
assert helpers['snapshot_actor_state'](actors)==before
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(actors_sub.get_all_level_actors())
    assert helpers['snapshot_actor_state']([a for a in current if not a.get_actor_label().startswith('Aurelion_WayfindingPortal_')])==before
    for row in report['signs']:
        a=next(a for a in current if a.get_actor_label()==row['actor'])
        assert a.get_actor_transform().export_text()==row['transform']
        assert a.static_mesh_component.static_mesh.get_path_name()==row['mesh']
        assert str(a.static_mesh_component.get_collision_profile_name())=='BlockAll'
        sign=next(a for a in current if a.get_actor_label()==row['sign'])
        assert str(sign.get_component_by_class(unreal.TextRenderComponent).text)==row['text']
        assert sign.get_actor_transform().export_text()==row['sign_transform']
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'wayfinding-portals.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('chamber-sign',(0,32200,-1510)),('gallery-sign',(0,41500,160)),('departure-sign',(0,45900,160))],
    M13_ROUTE_PITCHES={'chamber-sign':-8}))
