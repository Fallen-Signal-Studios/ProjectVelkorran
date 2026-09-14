"""Fit four visual-only Eclipse overlays; save only with explicit script flag."""
from pathlib import Path
import json,os,runpy,shutil,time,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/EclipseWallKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==3140
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
placements=[('Z08__EclipseConduit_05',1,-3485.5,19300,-895,-90),('Z08__EclipseScar_05',2,-3485.5,19900,-895,-90),
            ('Z08__EclipseConduit_06',2,3485.5,21700,-895,90),('Z08__EclipseScar_06',1,3485.5,22300,-895,90)]
selected={row[0] for row in placements};others=[a for a in actors if a.get_actor_label() not in selected]
before=helpers['snapshot_actor_state'](others);old=[]
for label,*_ in placements:
    a=by_label[label];c=a.get_component_by_class(unreal.StaticMeshComponent)
    assert a.get_class()==unreal.StaticMeshActor.static_class()
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    old.append(dict(actor=label,component=c.get_path_name(),transform=a.get_actor_transform().export_text(),mesh=c.static_mesh.get_path_name(),actor_collision=a.get_actor_enable_collision(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),materials=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())]))
fit_path=source/'placement-fit.json'
if not fit_path.exists():fit_path.write_text(json.dumps(dict(original=old,placements=placements),indent=2))
else:
    (out/'current-scar-state.json').write_text(json.dumps(old,indent=2))
    assert json.loads(fit_path.read_text())['original']==old
dest='/Game/Aurelion/Environment/ArchitectureKit';lib=unreal.MaterialEditingLibrary
materials={'M_Aurelion_IvoryStone':dest+'/Materials/M_AurelionKit_PavingIvory'}
for name,color,roughness,emissive in [('Intrusion',(.008,.006,.012),.31,False),('Seam',(.075,.006,.14),.5,True)]:
    path=dest+'/Materials/M_AurelionKit_Eclipse'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AurelionKit_Eclipse'+name,dest+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        n=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*color,1))
        assert lib.connect_material_property(n,'',unreal.MaterialProperty.MP_BASE_COLOR)
        if emissive:assert lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        r=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);r.set_editor_property('r',roughness)
        assert lib.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
        mat.set_editor_property('used_with_nanite',True);lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    materials['M_Eclipse_'+name]=path
meshes={}
for i,spec in enumerate(json.loads((source/'manifest.json').read_text())['modules'],1):
    path=dest+'/Meshes/'+spec['asset']
    meshes[i]=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);settings=sm.get_nanite_settings(meshes[i])
    if settings.get_editor_property('position_precision')!=spec['position_precision']:
        settings.set_editor_property('position_precision',spec['position_precision']);sm.set_nanite_settings(meshes[i],settings,True);assert unreal.EditorAssetLibrary.save_loaded_asset(meshes[i])
for label,index,x,y,z,yaw in placements:
    a=by_label[label];c=a.get_component_by_class(unreal.StaticMeshComponent);a.modify();c.modify()
    c.set_static_mesh(meshes[index]);c.set_editor_property('override_materials',[])
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
    a.set_actor_transform(unreal.Transform(location=unreal.Vector(x,y,z),rotation=unreal.Rotator(yaw=yaw),scale=unreal.Vector(1,1,1)),False,False)
assert helpers['snapshot_actor_state'](others)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_eclipse_wall_scars.py'))['check_eclipse_wall_scars'](actors)
persist=bool(globals().get('PERSIST_ECLIPSE_SCARS',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'eclipse-scar-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,unaffected_actor_states=len(others)),indent=2))
exec(compile((root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text(),'review_scars','exec'),globals())
