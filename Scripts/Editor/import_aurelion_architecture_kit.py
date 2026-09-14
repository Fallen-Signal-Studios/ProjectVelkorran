"""Import owned kit and create a separate art-review map; campaign maps are untouched."""
import json
import os
from pathlib import Path
import time
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
source=root/'Art/Source/Aurelion/ArchitectureKit'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
destination='/Game/Aurelion/Environment/ArchitectureKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
lib=unreal.MaterialEditingLibrary
def duplicate(name,original):
    path=destination+'/Materials/'+name
    material=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(original,path)
    assert material
    return material
stone=duplicate('M_AurelionKit_Ivory','/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_IvoryStone')
base=lib.get_material_property_input_node(stone,unreal.MaterialProperty.MP_BASE_COLOR)
assert isinstance(base,unreal.MaterialExpressionLinearInterpolate)
color,textured,_=lib.get_inputs_for_material_expression(stone,base)
assert isinstance(color,unreal.MaterialExpressionConstant3Vector)
color.set_editor_property('constant',unreal.LinearColor(.62,.59,.52,1))
base.set_editor_property('const_alpha',.22)
normal=lib.get_material_property_input_node(stone,unreal.MaterialProperty.MP_NORMAL)
assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate)
normal.set_editor_property('const_alpha',.2)
rough=lib.get_material_property_input_node(stone,unreal.MaterialProperty.MP_ROUGHNESS)
assert isinstance(rough,unreal.MaterialExpressionAdd)
variation=lib.get_inputs_for_material_expression(stone,rough)[0]
assert isinstance(variation,unreal.MaterialExpressionMultiply)
rough.set_editor_property('const_b',.34); variation.set_editor_property('const_b',.2)
lib.recompile_material(stone)
gold=duplicate('M_AurelionKit_Gold','/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_gold')
dark=duplicate('M_AurelionKit_Reveal','/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_dark_trim')
materials={'M_Aurelion_IvoryStone':stone,'M_Aurelion_AncientGold':gold,'M_Aurelion_ChannelShadow':dark}
for material in materials.values(): assert unreal.EditorAssetLibrary.save_loaded_asset(material)
meshes={}; rows=[]
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for spec in json.loads((source/'manifest.json').read_text())['modules']:
    name=spec['asset']; path=destination+'/Meshes/'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task=unreal.AssetImportTask(); task.filename=str(source/(name+'.fbx')); task.destination_path=destination+'/Meshes'
        task.destination_name=name; task.automated=True; task.save=False; task.replace_existing=False; task.factory=unreal.FbxFactory()
        options=unreal.FbxImportUI(); options.import_mesh=True; options.import_as_skeletal=False
        options.import_materials=False; options.import_textures=False; options.automated_import_should_detect_type=False
        options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        options.static_mesh_import_data.combine_meshes=True; options.static_mesh_import_data.auto_generate_collision=False
        options.static_mesh_import_data.generate_lightmap_u_vs=False; task.options=options
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(path); assert isinstance(mesh,unreal.StaticMesh)
    assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve()==(source/(name+'.fbx')).resolve()
    extent=mesh.get_bounds().box_extent; dimensions=[extent.x*2,extent.y*2,extent.z*2]
    assert all(abs(a-b*100)<5 for a,b in zip(dimensions,spec['nominal_dimensions_m'])), (name,dimensions)
    slots=[]
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        key=str(slot.get_editor_property('imported_material_slot_name')); assert key in materials
        mesh.set_material(i,materials[key]); slots.append(key)
    assert len(slots)==3
    mesh.set_editor_property('light_map_coordinate_index',1)
    if 'Cornice' not in name and sm.get_simple_collision_count(mesh)==0:
        assert sm.add_simple_collisions(mesh,unreal.ScriptingCollisionShapeType.BOX)>=0
    nanite=sm.get_nanite_settings(mesh); nanite.set_editor_property('enabled',True); sm.set_nanite_settings(mesh,nanite,True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    assert sm.get_nanite_settings(mesh).get_editor_property('enabled')
    meshes[name]=mesh
    rows.append(dict(asset=path,dimensions_cm=dimensions,materials=slots,
        simple_collision=sm.get_simple_collision_count(mesh),nanite=True,lightmap_channel=1))

review='/Game/Aurelion/ArtReview/L_Aurelion_ArchitectureKit'
assert not unreal.EditorAssetLibrary.does_asset_exist(review), 'Preserve existing art-review map'
assert editor.new_level(review)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def place(name,x,y,z):
    a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,z))
    a.set_actor_label('KIT_REVIEW_'+name+'_'+str(x)); a.static_mesh_component.set_static_mesh(meshes['SM_Aurelion_KIT_'+name])
    a.set_actor_rotation(unreal.Rotator(yaw=180),False)
    return a
for x in (-200,200): place('WallBay_4x7',x,0,0); place('Floor_4m',x,-200,0)
for x in (-400,0,400): place('Pier_1x7',x,-15,0)
place('Cornice_4m',0,200,0)
for pos,energy,size in (((-500,-600,1000),50000,700),((500,-300,600),28000,500),((0,300,900),38000,400)):
    location=unreal.Vector(*pos)
    rot=unreal.MathLibrary.find_look_at_rotation(location,unreal.Vector(0,0,300))
    light=actors.spawn_actor_from_class(unreal.RectLight,location,rot)
    c=light.get_component_by_class(unreal.RectLightComponent); c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS); c.set_intensity(energy)
    c.set_attenuation_radius(3000); c.set_source_width(size); c.set_source_height(size)
post=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
post.set_editor_property('unbound',True)
settings=post.get_editor_property('settings')
settings.set_editor_property('override_auto_exposure_method',True)
settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property('override_auto_exposure_bias',True); settings.set_editor_property('auto_exposure_bias',1)
post.set_editor_property('settings',settings)
camera=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
location=unreal.Vector(1000,-1600,720); rotation=unreal.MathLibrary.find_look_at_rotation(location,unreal.Vector(0,0,310))
camera.set_level_viewport_camera_info(location,rotation); editor.editor_set_game_view(True)
assert editor.save_current_level()
(out/'kit-import.json').write_text(json.dumps(dict(status='imported_with_review_map',meshes=rows,
    map=review,limitations='No campaign deployment, runtime traversal or performance acceptance'),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.monotonic(),phase=0)
def tick(delta):
    try:
        elapsed=time.monotonic()-state['start']
        if state['phase']==0 and elapsed>20:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,1000,str(out/'kit-unreal-review.png'))
            state['phase']=1
        elif state['phase']==1 and elapsed>30:
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        raise
state['handle']=unreal.register_slate_post_tick_callback(tick)
