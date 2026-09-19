"""Bind the Elite Core to the actual torso physics body; preserve all consequences."""
import json
import os
import re
import shutil
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
bp=unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionElite')
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
disk=Path(unreal.Paths.project_dir())/'Content/Aurelion/Enemies/BP_AurelionElite.uasset'
backup=out/'BP_AurelionElite.before.uasset'
assert not backup.exists(), 'Use a fresh run directory to preserve the original asset'
shutil.copy2(disk,backup)
core=unreal.get_default_object(bp.generated_class()).get_core_weak_points()
assert core.get_path_name().startswith('/Game/Aurelion/Enemies/BP_AurelionElite.'), 'Refusing native or external defaults'
zones=list(core.get_editor_property('weak_point_zones'))
assert len(zones)==1 and str(zones[0].get_editor_property('zone_id'))=='Core'
before=zones[0].export_text()
mesh=unreal.load_asset('/Game/Aurelion/Art/Characters/Appearances/CA_AurelionElite').get_editor_property('character_attributes').get_editor_property('base_mesh')
assert 'CATRigSpine1' in {str(b) for b in unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh)}
physics=mesh.get_editor_property('physics_asset')
task=unreal.AssetExportTask()
export=out/'elite-physics.t3d'
for key,value in dict(object=physics,exporter=unreal.ObjectExporterT3D(),filename=str(export),automated=True,prompt=False).items():
    task.set_editor_property(key,value)
assert unreal.Exporter.run_asset_export_task(task)
bodies=set(re.findall(r'^\s*BoneName="?([^"\r\n]+)',export.read_text(encoding='utf-8-sig'),re.MULTILINE))
assert 'CATRigSpine1' in bodies, 'Core matcher needs a real hit-test physics body'
# Change only the matcher, preserving authored break/reveal consequences.
zones[0].set_editor_property('hit_bones',['CATRigSpine1'])
core.set_editor_property('weak_point_zones',zones)
assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(bp)
saved_core=unreal.get_default_object(bp.generated_class()).get_core_weak_points()
saved_zones=list(saved_core.get_editor_property('weak_point_zones'))
assert len(saved_zones)==1 and saved_zones[0].export_text()==zones[0].export_text(), 'Compilation discarded the override'
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
(out/'core-body.json').write_text(json.dumps({'before':before,'after':saved_zones[0].export_text(),
    'mesh':mesh.get_path_name(),'physics':physics.get_path_name(),
    'physics_body':'CATRigSpine1','live_hit_qualified':False},indent=2),encoding='utf8')
