"""Preview pier-mounted vault uplights; save only via the explicit save entry point."""
import json
import os
from pathlib import Path
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False))
root=Path(unreal.Paths.project_dir()).resolve()
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original=list(subsystem.get_all_level_actors()); by_label={a.get_actor_label():a for a in original}
assert len(original)==1804
assert not any(a.get_actor_label().startswith('KIT_Z01_Uplight') for a in original)
def snapshot(actors):
    def components(actor):
        rows=[]
        for c in actor.get_components_by_class(unreal.PrimitiveComponent):
            # Construction scripts regenerate non-colliding editor sprite names.
            # Keep their class/count/collision, but not the transient numeric suffix.
            identity=c.get_path_name()
            if isinstance(c,unreal.BillboardComponent) and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION:
                identity=actor.get_path_name()+':BillboardComponent'
            rows.append((identity,str(c.get_collision_enabled())))
        return sorted(rows)
    return {a.get_path_name():(a.get_actor_transform().export_text(),a.get_actor_enable_collision(),components(a)) for a in actors}
before=snapshot(original)
source=root/'Art/Source/Aurelion/UplightKit'
name='SM_Aurelion_KIT_PierUplight'; destination='/Game/Aurelion/Environment/ArchitectureKit'
light_path=destination+'/Materials/M_AurelionKit_UplightLens'
lens=unreal.load_asset(light_path) if unreal.EditorAssetLibrary.does_asset_exist(light_path) else None
if not lens:
    lens=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AurelionKit_UplightLens',destination+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    for prop,color in ((unreal.MaterialProperty.MP_BASE_COLOR,(.92,.81,.56)),(unreal.MaterialProperty.MP_EMISSIVE_COLOR,(3,2.52,1.65))):
        node=unreal.MaterialEditingLibrary.create_material_expression(lens,unreal.MaterialExpressionConstant3Vector)
        node.set_editor_property('constant',unreal.LinearColor(*color,1))
        assert unreal.MaterialEditingLibrary.connect_material_property(node,'',prop)
    unreal.MaterialEditingLibrary.recompile_material(lens)
    assert unreal.EditorAssetLibrary.save_loaded_asset(lens)
asset=destination+'/Meshes/'+name
if unreal.EditorAssetLibrary.does_asset_exist(asset):
    old=unreal.load_asset(asset)
    assert Path(old.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(name+'.fbx')).resolve()
task=unreal.AssetImportTask(); task.filename=str(source/(name+'.fbx')); task.destination_path=destination+'/Meshes'
task.destination_name=name; task.automated=True; task.save=False; task.replace_existing=True; task.factory=unreal.FbxFactory()
options=unreal.FbxImportUI(); options.import_mesh=True; options.import_as_skeletal=False
options.import_materials=False; options.import_textures=False; options.automated_import_should_detect_type=False
options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
options.static_mesh_import_data.combine_meshes=True; options.static_mesh_import_data.auto_generate_collision=False
options.static_mesh_import_data.generate_lightmap_u_vs=False
options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
task.options=options; unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=unreal.load_asset(asset); assert mesh
materials={'M_Aurelion_IvoryStone':'Ivory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}
for i,slot in enumerate(mesh.get_editor_property('static_materials')):
    key=str(slot.get_editor_property('imported_material_slot_name'))
    material=unreal.load_asset(destination+'/Materials/M_AurelionKit_'+materials[key]); assert material
    mesh.set_material(i,material)
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assert sm.get_simple_collision_count(mesh)==0 and sm.get_num_uv_channels(mesh,0)==2
extent=mesh.get_bounds().box_extent
expected=json.loads((source/'manifest.json').read_text())['modules'][0]['nominal_dimensions_m']
assert all(abs(a-b*100)<1 for a,b in zip((extent.x*2,extent.y*2,extent.z*2),expected))
mesh.set_editor_property('light_map_coordinate_index',1)
nanite=sm.get_nanite_settings(mesh); nanite.set_editor_property('enabled',True); sm.set_nanite_settings(mesh,nanite,True)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
rows=[]
for side,face,yaw,direction in (('West',-8245.7608,-90,1),('East',-5774.1984,90,-1)):
    for i,y in enumerate((-16300,-14700,-13100)):
        label='KIT_Z01_Uplight_'+side+'_'+str(i)
        p=unreal.Vector(face,y,630)
        a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,p,unreal.Rotator(yaw=yaw))
        a.set_actor_label(label); a.set_folder_path('Aurelion/CustomArchitecture/Z01/Lighting')
        a.static_mesh_component.set_static_mesh(mesh)
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); a.set_actor_enable_collision(False)
        lamp_position=unreal.Vector(face+direction*27.5,y,679)
        rotation=unreal.MathLibrary.find_look_at_rotation(lamp_position,unreal.Vector(-7010,y,1120))
        light=subsystem.spawn_actor_from_class(unreal.RectLight,lamp_position,rotation)
        light.set_actor_label(label+'_Light'); light.set_folder_path('Aurelion/CustomArchitecture/Z01/Lighting')
        c=light.get_component_by_class(unreal.RectLightComponent); c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS); c.set_intensity(1500)
        c.set_light_color(unreal.LinearColor(1,.91,.76,1)); c.set_attenuation_radius(2200)
        c.set_source_width(45); c.set_source_height(28)
        rows.append(dict(fixture=label,light=light.get_actor_label(),lumens=1500,radius_cm=2200,position=lamp_position.export_text()))
after=snapshot(original)
changes={key:dict(before=before[key],after=after[key]) for key in before if before[key]!=after[key]}
(out/'preservation-differences.json').write_text(json.dumps(changes,indent=2))
assert not changes, 'Original actor/component state changed; inspect preservation-differences.json'
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap')
    assert editor.save_current_level()
(out/'uplight-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',placements=rows,original_actor_count=len(original),qualification='Authoring and visual review only; combat visibility and GPU cost unqualified'),indent=2))
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('vault-detail',unreal.Vector(-7010,-15100,400),unreal.Rotator(pitch=45,yaw=90),90)")
exec(compile("p=by_label['Z01_Entry_StandIn']"+capture,'z01_uplight_capture','exec'))
