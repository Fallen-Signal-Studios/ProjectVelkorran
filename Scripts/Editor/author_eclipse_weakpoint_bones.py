"""Replace obsolete mannequin matchers with verified parasite physics-body bones."""
import json
import os
from pathlib import Path
import re
import shutil
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'weakpoint-authoring'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not out.exists(), 'Use a new output directory; preserve previous authoring evidence'
out.mkdir(parents=True)
mapping={
    'Linkbound':{'head':['CATRigHub002'],'spine_03':['CATRigSpine2','CATRigSpine1']},
    'WallRunner':{'head':['CATRigHub002'],'spine_03':['CATRigSpine']},
    'Weaver':{'head':['CATRigHub005'],'spine_03':['CATRigHub004','CATRigSpine']},
}
report=dict(status='preflight',roles={},saved=[])
pending=[]
try:
    subsystem=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library=unreal.SubobjectDataBlueprintFunctionLibrary
    for role,zones in mapping.items():
        package='/Game/Aurelion/Enemies/BP_Aurelion'+role
        bp=unreal.load_asset(package)
        components=[]
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
            data=library.get_data(handle)
            obj=library.get_object(data)
            if isinstance(obj,unreal.SovWeakPointComponent):
                assert not library.is_inherited_component(data), 'Inherited component requires an owned override'
                assert obj.get_path_name().startswith(package+'.'), 'Refusing external component mutation'
                components.append(obj)
        assert len(components)==1, 'Expected one owned weak-point component'
        component=components[0]
        existing=list(component.get_editor_property('weak_point_zones'))
        assert {str(z.zone_id) for z in existing}==set(zones)
        assert {str(z.zone_id):[str(b) for b in z.hit_bones] for z in existing}=={
            'head':['head'],'spine_03':['spine_03','spine_02','spine_01']}, 'Unexpected preexisting matcher'
        attrs=unreal.load_asset('/Game/Aurelion/Art/Characters/Appearances/CA_Aurelion'+role).get_editor_property('character_attributes')
        mesh=attrs.get_editor_property('base_mesh')
        bones={str(b) for b in unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh)}
        physics=mesh.get_editor_property('physics_asset')
        task=unreal.AssetExportTask()
        export=out/(role+'-physics.t3d')
        for key,value in dict(object=physics,exporter=unreal.ObjectExporterT3D(),filename=str(export),automated=True,prompt=False).items():
            task.set_editor_property(key,value)
        assert unreal.Exporter.run_asset_export_task(task)
        bodies=set(re.findall(r'^\s*BoneName="?([^"\r\n]+)',export.read_text(encoding='utf-8-sig'),re.MULTILINE))
        assert all(b in bones and b in bodies for values in zones.values() for b in values)
        disk=root/'Content/Aurelion/Enemies'/('BP_Aurelion'+role+'.uasset')
        shutil.copy2(disk,out/disk.name)
        report['roles'][role]=dict(mesh=mesh.get_path_name(),component=component.get_path_name(),
            before=[z.export_text() for z in existing],matchers=zones)
        pending.append((role,bp,component,existing))
    for role,bp,component,existing in pending:
        for zone in existing:
            zone.set_editor_property('hit_bones',mapping[role][str(zone.zone_id)])
        component.set_editor_property('weak_point_zones',existing)
        assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
        report['roles'][role]['after']=[z.export_text() for z in component.get_editor_property('weak_point_zones')]
        report['saved'].append(bp.get_path_name())
    report['status']='authored_requires_native_hit_and_echo_validation'
except Exception:
    import traceback
    report.update(status='failed',error=traceback.format_exc())
finally:
    (out/'result.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('ECLIPSE_WEAKPOINT_BONES '+report['status'])
