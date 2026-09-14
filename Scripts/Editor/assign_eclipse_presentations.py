"""Assign prepared Parasites appearances to owned Eclipse roles, preserving native gameplay."""
import json
import shutil
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Validation/Aurelion/EclipseAnimation'
OUT.mkdir(parents=True,exist_ok=True)
report={'status':'running','roles':{},'saved':[],'gameplay_qualified':False}
roles={'WallRunner':'SK_Parasite_Spider','Weaver':'SK_Alfa','Elite':'SK_Fat'}

def backup(asset):
    package=asset.get_path_name().split('.')[0]
    assert package.startswith('/Game/Aurelion/')
    path=ROOT/'Content'/(package[len('/Game/'):]+'.uasset')
    target=OUT/'before-assignment'/path.relative_to(ROOT/'Content')
    target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists(): shutil.copy2(path,target)

try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    for role,mesh_name in roles.items():
        appearance=unreal.load_asset('/Game/Aurelion/Art/Characters/Appearances/CA_Aurelion'+role)
        bp=unreal.load_asset('/Game/Aurelion/Enemies/BP_Aurelion'+role)
        anim=unreal.load_asset('/Game/Aurelion/Enemies/Animation/ABP_Eclipse'+role)
        mesh=unreal.load_asset('/Game/Parasites_Pack/Mesh/'+mesh_name)
        assert all((appearance,bp,anim,mesh))
        assert anim.get_editor_property('target_skeleton') == mesh.get_editor_property('skeleton')
        backup(appearance); backup(bp)
        attrs=appearance.get_editor_property('character_attributes')
        cdo=unreal.get_default_object(bp.generated_class())
        component=cdo.get_editor_property('mesh')
        bounds=mesh.get_imported_bounds()
        row={'appearance_before':attrs.export_text(),'mesh':mesh.get_path_name(),'bounds':bounds.export_text(),
             'component_before':component.get_relative_transform().export_text()}
        report['roles'][role]=row
        attrs.set_editor_property('base_mesh',mesh)
        attrs.set_editor_property('base_mesh_anim_bp',anim.generated_class())
        attrs.set_editor_property('base_local_mesh_anim_bp',anim.generated_class())
        attrs.set_editor_property('unarmed_anim_layer',None)
        attrs.set_editor_property('hide_base_mesh',False)
        # A single authoritative visible mesh keeps montage pose and hit bones identical.
        attrs.set_editor_property('meshes',{})
        appearance.set_editor_property('character_attributes',attrs)
        height=2.*bounds.box_extent.z
        assert height>1.
        target_height={'WallRunner':140.,'Weaver':185.,'Elite':210.}[role]
        scale=target_height/height
        component.set_editor_property('relative_scale3d',unreal.Vector(scale,scale,scale))
        half=cdo.get_editor_property('capsule_component').get_unscaled_capsule_half_height()
        previous=component.get_editor_property('relative_location')
        component.set_editor_property('relative_location',unreal.Vector(previous.x,previous.y,-half-(bounds.origin.z-bounds.box_extent.z)*scale))
        if role=='Elite':
            core=cdo.get_core_weak_points()
            zones=list(core.get_editor_property('weak_point_zones'))
            assert len(zones)==1 and str(zones[0].get_editor_property('zone_id'))=='Core'
            row['core_before']=zones[0].export_text()
            # The Fat physics asset has a body on Spine1, not Spine2.
            bone='CATRigSpine1'
            assert bone in [str(x) for x in unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh)]
            zones[0].set_editor_property('hit_bones',[bone])
            core.set_editor_property('weak_point_zones',zones)
            row['core_after']=zones[0].export_text()
        assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(bp)
        row['appearance_after']=appearance.get_editor_property('character_attributes').export_text()
        row['component_after']=unreal.get_default_object(bp.generated_class()).get_editor_property('mesh').get_relative_transform().export_text()
        for asset in (appearance,bp):
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
            report['saved'].append(asset.get_path_name())
    report['status']='assigned_requires_pie'
except Exception:
    import traceback
    report['status']='failed'; report['error']=traceback.format_exc()
finally:
    (OUT/'assign-presentation.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('ECLIPSE_ASSIGNMENT '+report['status'])
