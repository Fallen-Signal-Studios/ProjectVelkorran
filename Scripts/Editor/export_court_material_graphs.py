"""Read-only text exports expose material inputs hidden from the Python enum."""
from pathlib import Path
import json,os
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
rows=[]
for name in ('PavingIvory','PavingBasalt','Gold','Reveal'):
    material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_'+name)
    assert isinstance(material,unreal.Material)
    task=unreal.AssetExportTask();task.object=material;task.filename=str(out/(name+'.copy'));task.automated=True;task.prompt=False;task.replace_identical=True
    assert unreal.Exporter.run_asset_export_task(task),(name,list(task.errors))
    assert Path(task.filename).is_file()
    exported=Path(task.filename).read_text()
    assert 'MaterialEditorOnlyData' in exported and all(key+'=(Expression=' in exported for key in ('BaseColor','Roughness','Normal'))
    keys=('PixelDepthOffset=','WorldPositionOffset=','Displacement=','OpacityMask=')
    bindings=[key for key in keys if key in exported]
    rows.append(dict(material=material.get_path_name(),file=task.filename,serialized_depth_displacement_or_mask_bindings=bindings))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'material-exports.json').write_text(json.dumps(dict(status='read_only',exports=rows),indent=2))
