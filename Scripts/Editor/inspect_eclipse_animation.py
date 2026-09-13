"""Export the existing locomotion graph and sample definitions for read-only review."""
from pathlib import Path
import json
import unreal
out=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/Aurelion/EclipseAnimation'
out.mkdir(parents=True,exist_ok=True)
for path in ('/Game/Aurelion/Enemies/Animation/ABP_EclipseLinkbound', '/Game/Parasites_Pack/Animations/Parasites/BS_Linkbound'):
    asset=unreal.load_asset(path)
    task=unreal.AssetExportTask()
    for key,value in {'object':asset,'exporter':unreal.ObjectExporterT3D(),'filename':str(out/(asset.get_name()+'.t3d')),
                      'automated':True,'prompt':False,'selected':False,'replace_identical':True}.items():
        task.set_editor_property(key,value)
    assert unreal.Exporter.run_asset_export_task(task)
unreal.log('ECLIPSE_ANIMATION_INSPECTION_COMPLETE')
