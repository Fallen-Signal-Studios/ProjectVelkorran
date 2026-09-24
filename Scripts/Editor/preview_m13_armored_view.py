"""Preview two authored Z12 armored views without saving or changing collision."""
from pathlib import Path
import hashlib,json,os,runpy,shutil
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=root/'Art/Source/Aurelion/Z12ArmoredView'
persist=bool(globals().get('SAVE_M13_ARMORED_VIEW',False))
review=json.loads(Path(globals()['REVIEWED_M13_ARMORED_VIEW']).read_text()) if persist else None
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actorsub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
existing=list(actorsub.get_all_level_actors())
bylabel={a.get_actor_label():a for a in existing}
owner=bylabel['Aurelion_Art_M13_Z12_6_21a580']
column_owner=bylabel['Aurelion_Art_M13_Z12_8_dce7c8']
owner_label=owner.get_actor_label()
column_label=column_owner.get_actor_label()
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
unchanged=helper['snapshot_actor_state']([a for a in existing if a not in (owner,column_owner)])
map_hashes={name:hashlib.sha256((root/'Content/Aurelion/Maps'/name).read_bytes()).hexdigest()
            for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
specs=json.loads((source/'manifest.json').read_text())['modules']
source_hashes={s['asset']:hashlib.sha256((source/(s['asset']+'.fbx')).read_bytes()).hexdigest()
               for s in specs}
if persist:
    assert review['status']=='unsaved_preview'
    assert review['map_hashes']==map_hashes
    assert review['source_fbx_sha256']==source_hashes
    assert review['removed_visual_instances']==[8,9,10,11,12,13,14,15,32,33,34,35,36,37,38,39]
side_mesh='/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z12CofferSide'
side=next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
          if c.static_mesh and c.static_mesh.get_path_name().split('.')[0]==side_mesh)
assert side.get_instance_count()==48 and side.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
original=[side.get_instance_transform(i,world_space=True) for i in range(48)]
removed=[]
for i,t in enumerate(original):
    p=t.translation
    if abs(abs(p.x)-2100)<.01 and (abs(p.y-47300)<.01 or abs(p.y-47700)<.01):
        removed.append(i)
assert len(removed)==16,removed
column=next(c for c in column_owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
            if c.static_mesh and c.static_mesh.get_name()=='SM_KB3D_CPI_IntGarage_A_Column_A')
assert column.get_instance_count()==2 and column.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
column_poses=[column.get_instance_transform(i,world_space=True).export_text() for i in range(2)]
assert all('Y=47500.' in t for t in column_poses)
for label in ('Z12_Wall_EW-1','Z12_Wall_EW1'):
    wall=bylabel[label].static_mesh_component
    assert not wall.get_editor_property('visible')
    assert wall.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS

assetroot='/Game/Aurelion/Environment/ArchitectureKit'
assets=unreal.EditorAssetLibrary
glasspath=assetroot+'/Materials/M_AurelionKit_ViewGlass'
if assets.does_asset_exist(glasspath):
    glass=unreal.load_asset(glasspath)
else:
    glass=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_AurelionKit_ViewGlass',assetroot+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    assert glass
    glass.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    glass.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    glass.set_editor_property('two_sided',True)
    edit=unreal.MaterialEditingLibrary
    color=edit.create_material_expression(glass,unreal.MaterialExpressionConstant3Vector,-350,0)
    color.set_editor_property('constant',unreal.LinearColor(.18,.27,.32,1))
    assert edit.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for value,prop,y in ((.115,unreal.MaterialProperty.MP_OPACITY,140),
                         (.075,unreal.MaterialProperty.MP_ROUGHNESS,280),
                         (.0,unreal.MaterialProperty.MP_METALLIC,420)):
        node=edit.create_material_expression(glass,unreal.MaterialExpressionConstant,-350,y)
        node.set_editor_property('r',value)
        assert edit.connect_material_property(node,'',prop)
    edit.recompile_material(glass)
    assert assets.save_loaded_asset(glass,False)
assert glass.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_TRANSLUCENT

materials={
    'M_Aurelion_ViewBlackStone':assetroot+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_IvoryStone':assetroot+'/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_ChannelShadow':'/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_dark_trim',
    'M_Aurelion_AncientGold':assetroot+'/Materials/M_AurelionKit_Gold',
    'M_Aurelion_ViewWarmConduit':'/Game/Aurelion/Art/Props/aURELION_pILLAR/Materials/M_GoldEmmissive',
    'M_Aurelion_ViewGlass':glasspath,
}
meshes={s['asset']:helper['import_owned_mesh'](s,source,assetroot+'/Meshes',materials) for s in specs}
frame=meshes['SM_Aurelion_KIT_Z12ArmoredViewFrame_8x6']
pane=meshes['SM_Aurelion_KIT_Z12ArmoredViewPane_8x6']
mullion=meshes['SM_Aurelion_KIT_Z12ArmoredViewMullion_6m']
assert not unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_nanite_settings(pane).get_editor_property('enabled')

side.modify();side.clear_instances()
for i,t in enumerate(original):
    if i not in removed:side.add_instance(t,world_space=True)
assert side.get_instance_count()==32
assert [side.get_instance_transform(i,world_space=True).export_text() for i in range(32)]==[
    t.export_text() for i,t in enumerate(original) if i not in removed]
column.modify();column.clear_instances()
assert column.get_instance_count()==0
assert helper['snapshot_actor_state']([a for a in existing if a not in (owner,column_owner)])==unchanged
created=[]
for direction,x in (('West',-2100),('East',2100)):
    yaw=-90 if x<0 else 90
    for role,mesh in (('Frame',frame),('Pane',pane),('Mullion',mullion)):
        actor=actorsub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,47500,0))
        actor.set_actor_label('PREVIEW_Z12_%sView%s'%(direction,role))
        actor.set_actor_rotation(unreal.Rotator(yaw=yaw),True)
        component=actor.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_collision_profile_name('NoCollision')
        component.set_editor_property('can_ever_affect_navigation',False)
        if role=='Pane':component.set_cast_shadow(False)
        created.append(actor)
assert all(a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for a in created)
assert all(hashlib.sha256((root/'Content/Aurelion/Maps'/name).read_bytes()).hexdigest()==value
           for name,value in map_hashes.items())
report=dict(status='unsaved_preview',
    map_hashes=map_hashes,removed_visual_instances=removed,remaining_visual_instances=32,
    removed_vendor_column_instances=column_poses,retained_wall_collision=True,
    retained_native_rib_collision=True,glass_material=glass.get_path_name(),
    glass_blend=str(glass.get_editor_property('blend_mode')),
    imported_meshes={k:v.get_path_name() for k,v in meshes.items()},
    source_fbx_sha256=source_hashes,
    preview_actors=[a.get_actor_label() for a in created],
    limitations='No PIE view, no player traversal or performance qualification')
if persist:
    backup=out/'L_Aurelion_M13.before.umap'
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',backup)
    assert level.save_current_level()
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    loaded=list(actorsub.get_all_level_actors())
    labels={a.get_actor_label():a for a in loaded}
    restored_owner=labels[owner_label]
    restored_side=next(c for c in restored_owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
                       if c.static_mesh and c.static_mesh.get_path_name().split('.')[0]==side_mesh)
    assert restored_side.get_instance_count()==32
    assert [restored_side.get_instance_transform(i,world_space=True).export_text() for i in range(32)]==[
        t.export_text() for i,t in enumerate(original) if i not in removed]
    restored_column=next(c for c in labels[column_label].get_components_by_class(unreal.InstancedStaticMeshComponent)
                         if c.static_mesh and c.static_mesh.get_name()=='SM_KB3D_CPI_IntGarage_A_Column_A')
    assert restored_column.get_instance_count()==0
    for label in ('Z12_Wall_EW-1','Z12_Wall_EW1','Z12_Rib_1_-1','Z12_Rib_1_1'):
        body=labels[label].static_mesh_component
        assert not body.get_editor_property('visible')
        assert body.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    for label in report['preview_actors']:
        component=labels[label].static_mesh_component
        assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert not component.get_editor_property('can_ever_affect_navigation')
    assert hashlib.sha256((root/'Content/Aurelion/Maps/L_Aurelion_M12.umap').read_bytes()).hexdigest()==map_hashes['L_Aurelion_M12.umap']
    after=hashlib.sha256((root/'Content/Aurelion/Maps/L_Aurelion_M13.umap').read_bytes()).hexdigest()
    assert after!=map_hashes['L_Aurelion_M13.umap']
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report.update(status='saved_reloaded',m13_sha256_after=after,m13_backup=str(backup),
                  limitations='PIE view, earned CP9 recovery and performance still require separate evidence')
(out/'armored-view-preview.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not persist,
    'M13_ROUTE_VIEWS':[('east-view-after',(0,47500,180)),('west-view-after',(0,47500,180))],
    'M13_ROUTE_YAWS':{'east-view-after':0,'west-view-after':180},
    'M13_ROUTE_PITCHES':{'east-view-after':5,'west-view-after':5}})
print('M13_ARMORED_VIEW_'+('SAVED_RELOADED' if persist else 'UNSAVED_PREVIEW')+'_PASS')
