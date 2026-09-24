"""Shared authoring checks for owned custom architecture. No work runs on import."""
from pathlib import Path
import unreal

def snapshot_actor_state(actors):
    rows={}
    for a in actors:
        components=[]
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            identity=c.get_path_name()
            if isinstance(c,unreal.BillboardComponent) and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION:
                identity=a.get_path_name()+':BillboardComponent'
            components.append((identity,str(c.get_collision_enabled())))
        rows[a.get_path_name()]=(a.get_actor_transform().export_text(),a.get_actor_enable_collision(),sorted(components))
    return rows

def import_owned_mesh(spec,source,destination,materials):
    name=spec['asset']; asset=destination+'/'+name; filename=(source/(name+'.fbx')).resolve()
    if unreal.EditorAssetLibrary.does_asset_exist(asset):
        old=unreal.load_asset(asset)
        assert Path(old.get_editor_property('asset_import_data').get_first_filename()).resolve()==filename
    task=unreal.AssetImportTask(); task.filename=str(filename); task.destination_path=destination
    task.destination_name=name; task.automated=True; task.save=False; task.replace_existing=True; task.replace_existing_settings=True; task.factory=unreal.FbxFactory()
    options=unreal.FbxImportUI(); options.import_mesh=True; options.import_as_skeletal=False
    options.import_materials=False; options.import_textures=False; options.automated_import_should_detect_type=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes=True; options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.generate_lightmap_u_vs=False
    options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task.options=options; unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(asset); assert mesh
    keys=[]
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        key=str(slot.get_editor_property('imported_material_slot_name')); keys.append(key)
        material=unreal.load_asset(spec.get('material_overrides',{}).get(key,materials[key])); assert material
        mesh.set_material(i,material)
    assert set(keys)==set(spec['materials'])
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    expected_hulls=spec.get('convex_hulls',0)
    # Atomic FBX reimport can retain or generate collision despite the import flag.
    # These owned visual-only specs explicitly require none. Never strip authored hulls.
    if expected_hulls==0 and (sm.get_convex_collision_count(mesh) or sm.get_simple_collision_count(mesh)):
        unreal.log_warning('Removing reimport collision from visual-only owned mesh '+name)
        assert sm.remove_collisions(mesh)
    assert sm.get_convex_collision_count(mesh)==expected_hulls,(name,'convex hull count',sm.get_convex_collision_count(mesh),'expected',expected_hulls)
    # This API counts boxes/spheres/capsules separately from convex hulls.
    assert sm.get_simple_collision_count(mesh)==0 and sm.get_num_uv_channels(mesh,0)==2
    extent=mesh.get_bounds().box_extent
    assert all(abs(a-b*100)<1 for a,b in zip((extent.x*2,extent.y*2,extent.z*2),spec['nominal_dimensions_m']))
    mesh.set_editor_property('light_map_coordinate_index',1)
    nanite=sm.get_nanite_settings(mesh)
    nanite.set_editor_property('enabled',spec.get('nanite_enabled',True))
    if 'position_precision' in spec:
        nanite.set_editor_property('position_precision',spec['position_precision'])
    if spec.get('preserve_fallback_geometry',False):
        # Opt in for authored thin surfaces whose default reduced fallback moves collision.
        nanite.set_editor_property('fallback_target',unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
        nanite.set_editor_property('fallback_percent_triangles',1.0)
        nanite.set_editor_property('fallback_relative_error',0.0)
    sm.set_nanite_settings(mesh,nanite,True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh
